/******************************************************************************
 * @file lv_port_indev.c
 *
 * @par dependencies
 * - lvgl.h
 * - bsp_wrapper_touch.h
 * - bsp_cst816t_calibration.h
 * - cfg_touch.h
 *
 * @author Ethan-Hang
 *
 * @brief LVGL pointer indev port over the bsp_wrapper_touch abstraction.
 *
 * Processing flow:
 *   LVGL polls indev_read_cb every LV_INDEV_DEF_READ_PERIOD ms (10 ms by
 *   default) -> we call touch_get_finger_num / touch_get_xy on the wrapper,
 *   feed the raw panel pixel value through touch_calibration_apply_matrix
 *   when a calibration is loaded, then clamp and emit the result as the
 *   LVGL pointer state.
 *
 *   When the calibration UI is running it temporarily flips the bypass
 *   flag via lv_port_indev_set_bypass(true) so the UI's LV_EVENT_PRESSED
 *   handler can sample the raw (uncorrected) coordinate via
 *   lv_indev_get_point().
 *
 * @version V1.0 2026-04-25
 * @version V2.0 2026-04-26
 * @version V3.0 2026-05-23
 * @upgrade 2.0: Removed the direct bsp_cst816t_driver_t coupling; reads now
 *               flow through bsp_wrapper_touch.
 * @upgrade 3.0: Apply calibration affine transform via
 *               touch_calibration_apply_matrix() when calibrated.  Panel
 *               geometry is now sourced from cfg_touch.h (single source of
 *               truth) instead of local PANEL_* macros.  Added
 *               lv_port_indev_set_bypass() so the calibration UI can take
 *               raw samples.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "lv_port_indev.h"

#include <stddef.h>

#include "lvgl.h"
#include "bsp_wrapper_touch.h"
#include "bsp_cst816t_calibration.h"
#include "cfg_touch.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* Panel rectangle (single source of truth lives in cfg_touch.h). */
#define LV_PORT_INDEV_PANEL_WIDTH     (CFG_TOUCH_PANEL_WIDTH)
#define LV_PORT_INDEV_PANEL_HEIGHT    (CFG_TOUCH_PANEL_HEIGHT)
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static lv_indev_drv_t         s_indev_drv;
static lv_point_t             s_last_point        = {0, 0};
static volatile bool          s_bypass_calibration = false;
static bool                   s_prev_pressed       = false;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
void lv_port_indev_set_bypass(bool bypass)
{
    s_bypass_calibration = bypass;
}

/**
 * @brief      LVGL indev read callback.  Polled every 10 ms by lv_timer.
 *
 * @param[in]  drv  : Indev driver pointer (unused).
 *
 * @param[out] data : Output state + coordinates LVGL feeds into widgets.
 *
 * @return     None.
 *
 * @note       Holds the last reported coordinate while the finger is up so
 *             LVGL receives a clean PRESSED -> RELEASED transition at the
 *             same point, which makes click / long-press recognition work.
 * */
static void lv_port_indev_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    /**
     * Default to RELEASED + last point so any wrapper failure produces a
     * clean lift-off rather than a stuck press.
     **/
    data->state = LV_INDEV_STATE_RELEASED;
    data->point = s_last_point;

    uint8_t finger_num = 0u;
    platform_err_t st = touch_get_finger_num(&finger_num);
    if (PLATFORM_OK != st)
    {
        return;
    }
    if (0u == finger_num)
    {
        /**
         * Release edge: log once when the finger lifts.  Skipped if we
         * never reported a press to begin with — keeps the line count
         * proportional to real user actions, not the 10 ms poll cadence.
         **/
        if (s_prev_pressed)
        {
            DEBUG_OUT(i, CST816T_LOG_TAG,
                      "tp release @ (%d,%d)",
                      (int)s_last_point.x, (int)s_last_point.y);
            s_prev_pressed = false;
        }
        return;
    }

    uint16_t x_pos = 0u;
    uint16_t y_pos = 0u;
    st = touch_get_xy(&x_pos, &y_pos);
    if (PLATFORM_OK != st)
    {
        return;
    }

    /* Capture the raw value before calibration so the press-edge log can
     * report both raw and calibrated coordinates side by side. */
    const uint16_t raw_x = x_pos;
    const uint16_t raw_y = y_pos;

    /**
     * Apply the calibration affine transform unless we're inside the
     * calibration UI itself (which needs raw samples to feed into
     * touch_calibration_add_point).  apply_matrix is a no-op pass-through
     * when no calibration is loaded.
     **/
    if (!s_bypass_calibration && touch_calibration_is_calibrated())
    {
        uint16_t cal_x = 0u;
        uint16_t cal_y = 0u;
        if (CALIBRATION_SUCCESS ==
                touch_calibration_apply_matrix(touch_calibration_get_instance(),
                                               x_pos, y_pos,
                                               &cal_x, &cal_y))
        {
            x_pos = cal_x;
            y_pos = cal_y;
        }
    }

    /**
     * Clamp to panel rectangle so a stray off-edge sample (controllers can
     * occasionally report values just past the active area) does not land
     * inside an invalid widget hit-test.
     **/
    if (x_pos >= LV_PORT_INDEV_PANEL_WIDTH)
    {
        x_pos = (uint16_t)(LV_PORT_INDEV_PANEL_WIDTH - 1u);
    }
    if (y_pos >= LV_PORT_INDEV_PANEL_HEIGHT)
    {
        y_pos = (uint16_t)(LV_PORT_INDEV_PANEL_HEIGHT - 1u);
    }

    s_last_point.x = (lv_coord_t)x_pos;
    s_last_point.y = (lv_coord_t)y_pos;
    data->state    = LV_INDEV_STATE_PRESSED;
    data->point    = s_last_point;

    /**
     * Press edge: log once per finger-down.  bypass=1 means the calibration
     * UI is sampling raw values, so calling out the bypass state in the
     * line keeps the trace unambiguous when reading RTT after the fact.
     **/
    if (!s_prev_pressed)
    {
        DEBUG_OUT(i, CST816T_LOG_TAG,
                  "tp press  raw(%u,%u) -> screen(%d,%d) [bypass=%u]",
                  (unsigned)raw_x, (unsigned)raw_y,
                  (int)s_last_point.x, (int)s_last_point.y,
                  (unsigned)s_bypass_calibration);
        s_prev_pressed = true;
    }
}

bool lv_port_indev_init(void)
{
    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = lv_port_indev_read_cb;

    lv_indev_t *indev = lv_indev_drv_register(&s_indev_drv);
    return (NULL != indev);
}
//******************************* Functions *********************************//
