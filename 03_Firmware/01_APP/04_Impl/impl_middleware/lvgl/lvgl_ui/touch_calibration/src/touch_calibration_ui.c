/******************************************************************************
 * @file touch_calibration_ui.c
 *
 * @par dependencies
 * - touch_calibration_ui.h
 * - bsp_cst816t_calibration.h
 * - cfg_touch.h
 * - lvgl.h
 *
 * @author Ethan-Hang
 *
 * @brief LVGL implementation of the 9-point calibration screen.
 *
 *        Adapted from ec_s100_watch_1(2)/touch_calibration_ui.c with these
 *        changes for the current project:
 *
 *          - 9 standard points are computed at runtime from
 *            touch_calibration_get_standard_point() so they follow
 *            CFG_TOUCH_PANEL_WIDTH/HEIGHT changes (no hard-coded table)
 *          - finished signal is delivered via touch_calibration_ui_done_cb_t
 *            so the boot orchestrator can wait on a semaphore
 *          - removed the 180-degree rotation tables — this board is mounted
 *            with the panel in its native orientation
 *          - per-point timeout supervised by an lv_timer the screen owns
 *
 * @version V1.0 2026-05-23
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "touch_calibration_ui.h"
#include "bsp_cst816t_calibration.h"
#include "cfg_touch.h"
#include "Debug.h"

#include "lvgl.h"

#include <stddef.h>
#include <stdio.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define CALIB_CROSS_SIZE        (20)
#define CALIB_CROSS_BORDER_PX   (2)
#define CALIB_COMPLETE_DELAY_MS (3000U)
//******************************** Defines **********************************//

//******************************** Typedefs *********************************//
typedef enum
{
    CALIB_UI_INIT = 0,
    CALIB_UI_SHOW_POINT,
    CALIB_UI_WAIT_TOUCH,
    CALIB_UI_CALCULATING,
    CALIB_UI_COMPLETE,
    CALIB_UI_ERROR,
} calibration_ui_state_t;

typedef struct
{
    lv_obj_t                       *screen;
    lv_obj_t                       *label_title;
    lv_obj_t                       *label_instruction;
    lv_obj_t                       *cross;
    lv_obj_t                       *btn_skip;
    lv_obj_t                       *btn_restart;

    lv_timer_t                     *timeout_timer;
    lv_timer_t                     *complete_timer;

    calibration_ui_state_t          state;
    uint8_t                         current_idx;
    uint32_t                        point_start_tick;
    bool                            screen_ready;

    touch_calibration_t            *p_calib;
    touch_calibration_ui_done_cb_t  done_cb;
    calibration_status_t            final_status;
} calibration_ui_t;
//******************************** Typedefs *********************************//

//******************************* Variables *********************************//
static calibration_ui_t s_ui = {0};
//******************************* Variables *********************************//

//******************************* Declaring *********************************//
static void show_point(uint8_t idx);
static void show_instruction(const char *text);
static void on_screen_pressed(lv_event_t *e);
static void on_btn_skip_clicked(lv_event_t *e);
static void on_btn_restart_clicked(lv_event_t *e);
static void enter_complete(void);
static void enter_error(const char *msg, calibration_status_t err);
static void timeout_cb(lv_timer_t *timer);
static void complete_cb(lv_timer_t *timer);
static void finish(calibration_status_t result);
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
calibration_status_t touch_calibration_ui_init(void)
{
    s_ui.p_calib = touch_calibration_get_instance();
    if (NULL == s_ui.p_calib)
    {
        return CALIBRATION_ERROR_INVALID_POINT;
    }

    /* Wipe to a known starting state so a re-init after cleanup behaves the
     * same as the first init. */
    (void)touch_calibration_reset(s_ui.p_calib);

    s_ui.state            = CALIB_UI_INIT;
    s_ui.current_idx      = 0U;
    s_ui.point_start_tick = 0U;
    s_ui.screen_ready     = false;
    s_ui.done_cb          = NULL;
    s_ui.final_status     = CALIBRATION_SUCCESS;
    s_ui.timeout_timer    = NULL;
    s_ui.complete_timer   = NULL;

    /* Build the LVGL widgets.  Layout is fixed at panel size, so the
     * standard points (computed at runtime) all fall inside the screen
     * regardless of CFG_TOUCH_PANEL_* configuration. */
    s_ui.screen = lv_obj_create(NULL);
    lv_obj_set_size(s_ui.screen, CFG_TOUCH_PANEL_WIDTH, CFG_TOUCH_PANEL_HEIGHT);
    lv_obj_set_style_bg_color(s_ui.screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(s_ui.screen, 0, 0);
    lv_obj_clear_flag(s_ui.screen, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.label_title = lv_label_create(s_ui.screen);
    lv_label_set_text(s_ui.label_title, "Touch Calibration");
    lv_obj_set_style_text_color(s_ui.label_title, lv_color_white(), 0);
    lv_obj_align(s_ui.label_title, LV_ALIGN_TOP_MID, 0, 8);

    s_ui.label_instruction = lv_label_create(s_ui.screen);
    lv_label_set_text(s_ui.label_instruction, "");
    lv_obj_set_style_text_color(s_ui.label_instruction, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_ui.label_instruction,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_ui.label_instruction, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_ui.label_instruction,
                     (lv_coord_t)(CFG_TOUCH_PANEL_WIDTH - 20));
    lv_obj_align(s_ui.label_instruction, LV_ALIGN_TOP_MID, 0, 30);

    s_ui.cross = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.cross, CALIB_CROSS_SIZE, CALIB_CROSS_SIZE);
    lv_obj_set_style_bg_color(s_ui.cross, lv_color_white(), 0);
    lv_obj_set_style_border_width(s_ui.cross, CALIB_CROSS_BORDER_PX, 0);
    lv_obj_set_style_border_color(s_ui.cross, lv_color_make(255, 0, 0), 0);
    lv_obj_set_style_radius(s_ui.cross, 0, 0);
    lv_obj_set_style_pad_all(s_ui.cross, 0, 0);
    lv_obj_clear_flag(s_ui.cross, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_ui.cross, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.cross, LV_OBJ_FLAG_HIDDEN);

    /* The press event on the calibration screen carries the indev's
     * current point — that's the raw panel coordinate while bypass is on. */
    lv_obj_add_event_cb(s_ui.screen, on_screen_pressed, LV_EVENT_PRESSED, NULL);

    /* Skip / restart buttons are hidden on the happy path; only the error /
     * timeout state surfaces them.
     *
     * They MUST sit in the central band, not at the panel edges.  When this
     * screen surfaces, touch is still UNcalibrated and fed raw via bypass
     * (see lv_port_indev.c), and raw samples only reliably cover the same
     * MARGIN..(dim - MARGIN) region the 9 calibration crosses live in.
     * Edge-aligned buttons (the old LV_ALIGN_BOTTOM_* placement) fell partly
     * outside that reachable area, so a tap on the bottom-most button never
     * registered and the user was stuck.  Centre + enlarge so an uncalibrated
     * finger can always land on them.  Restart is primary (top); Skip below. */
    s_ui.btn_restart = lv_btn_create(s_ui.screen);
    lv_obj_set_size(s_ui.btn_restart, 140, 44);
    lv_obj_align(s_ui.btn_restart, LV_ALIGN_CENTER, 0, -2);
    lv_obj_t *lbl_restart = lv_label_create(s_ui.btn_restart);
    lv_label_set_text(lbl_restart, "Restart");
    lv_obj_center(lbl_restart);
    lv_obj_add_event_cb(s_ui.btn_restart, on_btn_restart_clicked,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_ui.btn_restart, LV_OBJ_FLAG_HIDDEN);

    s_ui.btn_skip = lv_btn_create(s_ui.screen);
    lv_obj_set_size(s_ui.btn_skip, 140, 40);
    lv_obj_align(s_ui.btn_skip, LV_ALIGN_CENTER, 0, 52);
    lv_obj_t *lbl_skip = lv_label_create(s_ui.btn_skip);
    lv_label_set_text(lbl_skip, "Skip");
    lv_obj_center(lbl_skip);
    lv_obj_add_event_cb(s_ui.btn_skip, on_btn_skip_clicked,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_ui.btn_skip, LV_OBJ_FLAG_HIDDEN);

    s_ui.screen_ready = true;
    return CALIBRATION_SUCCESS;
}

void touch_calibration_ui_start(touch_calibration_ui_done_cb_t done_cb)
{
    if (!s_ui.screen_ready)
    {
        if (NULL != done_cb)
        {
            done_cb(CALIBRATION_ERROR_INVALID_POINT);
        }
        return;
    }

    s_ui.done_cb      = done_cb;
    s_ui.current_idx  = 0U;
    s_ui.final_status = CALIBRATION_SUCCESS;
    s_ui.state        = CALIB_UI_SHOW_POINT;

    lv_scr_load(s_ui.screen);

    /* Per-point timeout supervisor — ticks every second, computes elapsed
     * inside the callback to bound at CFG_TOUCH_CALIB_TIMEOUT_MS. */
    s_ui.timeout_timer = lv_timer_create(timeout_cb, 1000U, NULL);

    show_point(0U);
}

void touch_calibration_ui_cleanup(void)
{
    if (NULL != s_ui.timeout_timer)
    {
        lv_timer_del(s_ui.timeout_timer);
        s_ui.timeout_timer = NULL;
    }
    if (NULL != s_ui.complete_timer)
    {
        lv_timer_del(s_ui.complete_timer);
        s_ui.complete_timer = NULL;
    }
    if (NULL != s_ui.screen)
    {
        lv_obj_del(s_ui.screen);
        s_ui.screen = NULL;
    }
    s_ui.screen_ready = false;
}

static void show_point(uint8_t idx)
{
    if (idx >= CFG_TOUCH_CALIB_POINT_COUNT)
    {
        return;
    }

    uint16_t sx = 0u;
    uint16_t sy = 0u;
    if (CALIBRATION_SUCCESS !=
            touch_calibration_get_standard_point(idx, &sx, &sy))
    {
        enter_error("standard-point lookup failed",
                    CALIBRATION_ERROR_INVALID_POINT);
        return;
    }

    char buf[48];
    (void)snprintf(buf, sizeof(buf),
                   "Touch the cross\n  (%u / %u)",
                   (unsigned)(idx + 1U),
                   (unsigned)CFG_TOUCH_CALIB_POINT_COUNT);
    show_instruction(buf);

    /* Position the cross so that its centre lands on (sx, sy). */
    lv_obj_set_pos(s_ui.cross,
                   (lv_coord_t)(sx - (CALIB_CROSS_SIZE / 2)),
                   (lv_coord_t)(sy - (CALIB_CROSS_SIZE / 2)));
    lv_obj_clear_flag(s_ui.cross, LV_OBJ_FLAG_HIDDEN);

    s_ui.state            = CALIB_UI_WAIT_TOUCH;
    s_ui.point_start_tick = lv_tick_get();
}

static void show_instruction(const char *text)
{
    if (NULL != s_ui.label_instruction)
    {
        lv_label_set_text(s_ui.label_instruction, text);
    }
}

static void on_screen_pressed(lv_event_t *e)
{
    (void)e;

    if (CALIB_UI_WAIT_TOUCH != s_ui.state)
    {
        return;
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (NULL == indev)
    {
        return;
    }

    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    uint16_t std_x = 0u;
    uint16_t std_y = 0u;
    if (CALIBRATION_SUCCESS !=
            touch_calibration_get_standard_point(s_ui.current_idx,
                                                 &std_x, &std_y))
    {
        enter_error("standard-point lookup failed",
                    CALIBRATION_ERROR_INVALID_POINT);
        return;
    }

    const calibration_status_t st = touch_calibration_add_point(
            s_ui.p_calib,
            (uint16_t)pt.x, (uint16_t)pt.y,
            std_x, std_y,
            s_ui.current_idx);
    if (CALIBRATION_SUCCESS != st)
    {
        enter_error("add_point failed", st);
        return;
    }

    DEBUG_OUT(i, TOUCH_CALIB_LOG_TAG,
              "calib pt%u: raw(%d,%d) -> std(%u,%u)",
              (unsigned)(s_ui.current_idx + 1U),
              (int)pt.x, (int)pt.y,
              (unsigned)std_x, (unsigned)std_y);

    lv_obj_add_flag(s_ui.cross, LV_OBJ_FLAG_HIDDEN);

    s_ui.current_idx = (uint8_t)(s_ui.current_idx + 1U);
    if (s_ui.current_idx < CFG_TOUCH_CALIB_POINT_COUNT)
    {
        show_point(s_ui.current_idx);
        return;
    }

    /* All 9 points collected -> compute coefficients + persist. */
    s_ui.state = CALIB_UI_CALCULATING;
    show_instruction("Computing calibration...");

    const calibration_status_t calc_st =
            touch_calibration_calculate_affine(s_ui.p_calib);
    if (CALIBRATION_SUCCESS != calc_st)
    {
        enter_error("calculate_affine failed", calc_st);
        return;
    }

    const calibration_status_t save_st =
            touch_calibration_save_to_storage(s_ui.p_calib);
    if (CALIBRATION_SUCCESS != save_st)
    {
        /* Persist failed but the in-RAM coefficients are still good —
         * surface the error to the user but continue with the freshly
         * computed coefficients in this session. */
        DEBUG_OUT(e, TOUCH_CALIB_ERR_LOG_TAG,
                  "calib save failed st=%d (in-RAM only)", (int)save_st);
    }

    enter_complete();
}

static void on_btn_skip_clicked(lv_event_t *e)
{
    (void)e;
    DEBUG_OUT(w, TOUCH_CALIB_LOG_TAG, "calib UI: user skipped");
    finish(CALIBRATION_ERROR_INSUFFICIENT_POINTS);
}

static void on_btn_restart_clicked(lv_event_t *e)
{
    (void)e;
    DEBUG_OUT(i, TOUCH_CALIB_LOG_TAG, "calib UI: user restart");

    (void)touch_calibration_reset(s_ui.p_calib);
    lv_obj_add_flag(s_ui.btn_skip, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ui.btn_restart, LV_OBJ_FLAG_HIDDEN);
    s_ui.current_idx = 0U;
    show_point(0U);
}

static void enter_complete(void)
{
    s_ui.state = CALIB_UI_COMPLETE;
    lv_obj_add_flag(s_ui.cross, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ui.btn_skip, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ui.btn_restart, LV_OBJ_FLAG_HIDDEN);
    show_instruction("Calibration complete!\nReturning in 3s...");

    /* Stop per-point timeout, schedule the auto-dismiss. */
    if (NULL != s_ui.timeout_timer)
    {
        lv_timer_del(s_ui.timeout_timer);
        s_ui.timeout_timer = NULL;
    }
    s_ui.complete_timer = lv_timer_create(complete_cb,
                                          CALIB_COMPLETE_DELAY_MS, NULL);
    lv_timer_set_repeat_count(s_ui.complete_timer, 1);
}

static void enter_error(const char *msg, calibration_status_t err)
{
    s_ui.state        = CALIB_UI_ERROR;
    s_ui.final_status = err;

    DEBUG_OUT(e, TOUCH_CALIB_ERR_LOG_TAG, "calib UI error: %s (st=%d)",
              (NULL != msg) ? msg : "?", (int)err);

    char buf[96];
    (void)snprintf(buf, sizeof(buf),
                   "Calibration error\n%s\nTap Restart or Skip",
                   (NULL != msg) ? msg : "");
    show_instruction(buf);

    lv_obj_add_flag(s_ui.cross, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_ui.btn_restart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_ui.btn_skip,    LV_OBJ_FLAG_HIDDEN);
}

static void timeout_cb(lv_timer_t *timer)
{
    (void)timer;
    if (CALIB_UI_WAIT_TOUCH != s_ui.state)
    {
        return;
    }
    const uint32_t elapsed = lv_tick_elaps(s_ui.point_start_tick);
    if (elapsed >= CFG_TOUCH_CALIB_TIMEOUT_MS)
    {
        enter_error("per-point timeout",
                    CALIBRATION_ERROR_INSUFFICIENT_POINTS);
    }
}

static void complete_cb(lv_timer_t *timer)
{
    (void)timer;
    /**
     * LVGL auto-deletes a timer once its repeat_count hits 0 (we set it to
     * 1 in enter_complete).  Drop our cached pointer here so the cleanup
     * path does not lv_timer_del a freed object — that was leaving the
     * "Returning in 3s..." screen stuck because the resulting UB happened
     * after s_done was set, blocking the orchestrator's exit path.
     **/
    s_ui.complete_timer = NULL;
    finish(CALIBRATION_SUCCESS);
}

static void finish(calibration_status_t result)
{
    if (NULL == s_ui.done_cb)
    {
        return;
    }
    const touch_calibration_ui_done_cb_t cb = s_ui.done_cb;
    s_ui.done_cb = NULL;     /* fire-once */
    cb(result);
}
//******************************* Functions *********************************//
