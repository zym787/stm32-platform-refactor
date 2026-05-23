/******************************************************************************
 * @file touch_calibration_boot.c
 *
 * @par dependencies
 * - touch_calibration_boot.h
 * - bsp_cst816t_calibration.h
 * - touch_calibration_ui.h
 * - lv_port_indev.h
 * - lvgl.h
 *
 * @author Ethan-Hang
 *
 * @brief Boot-time touch calibration orchestrator.  Runs inside
 *        lvgl_display_task: tries to load saved coefficients, and on any
 *        failure drives the LVGL calibration screen synchronously by
 *        owning the LVGL service loop until the user is done.
 *
 *        While the UI is active, the indev port is flipped into bypass
 *        mode so the UI's LV_EVENT_PRESSED handler can sample raw panel
 *        coordinates (which it then feeds into touch_calibration_add_point).
 *
 * @version V1.0 2026-05-23
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "touch_calibration_boot.h"

#include "bsp_cst816t_calibration.h"
#include "touch_calibration_ui.h"
#include "lv_port_indev.h"
#include "osal_wrapper_adapter.h"
#include "Debug.h"

#include "lvgl.h"

#include <stdbool.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_LV_TIMER_PERIOD_MS    (5U)
//******************************** Defines **********************************//

//******************************* Variables *********************************//
/**
 * The done callback runs from the LVGL timer/event path on the same task
 * that is spinning lv_timer_handler.  No cross-task synchronisation needed —
 * a plain volatile flag is enough to break the polling loop.
 */
static volatile bool                 s_done = false;
static volatile calibration_status_t s_result = CALIBRATION_SUCCESS;
//******************************* Variables *********************************//

//******************************* Functions *********************************//
static void on_calibration_done(calibration_status_t result)
{
    s_result = result;
    s_done   = true;
}

calibration_status_t touch_calibration_boot_apply(void)
{
    touch_calibration_t *p_calib = touch_calibration_get_instance();
    (void)touch_calibration_init(p_calib);

#if CFG_TOUCH_CALIB_FORCE_ON_BOOT
    /**
     * Bring-up / test override: skip the storage load entirely so the UI
     * fires every boot.  Set CFG_TOUCH_CALIB_FORCE_ON_BOOT back to 0 before
     * shipping — there is no runtime way to undo this short of recompiling.
     **/
    DEBUG_OUT(w, TOUCH_CALIB_LOG_TAG,
              "calib boot: FORCE_ON_BOOT=1, skipping storage load");
#else
    const calibration_status_t load_st =
            touch_calibration_load_from_storage(p_calib);
    if (CALIBRATION_SUCCESS == load_st)
    {
        DEBUG_OUT(i, TOUCH_CALIB_LOG_TAG, "calib boot: loaded from storage");
        return CALIBRATION_SUCCESS;
    }

    DEBUG_OUT(w, TOUCH_CALIB_LOG_TAG,
              "calib boot: load failed (st=%d), launching UI",
              (int)load_st);
#endif

    /* Indev must surface raw coordinates while the UI samples them. */
    lv_port_indev_set_bypass(true);

    s_done   = false;
    s_result = CALIBRATION_SUCCESS;

    const calibration_status_t init_st = touch_calibration_ui_init();
    if (CALIBRATION_SUCCESS != init_st)
    {
        DEBUG_OUT(e, TOUCH_CALIB_ERR_LOG_TAG,
                  "calib boot: ui_init failed st=%d", (int)init_st);
        lv_port_indev_set_bypass(false);
        return init_st;
    }

    touch_calibration_ui_start(on_calibration_done);

    /**
     * Own the LVGL service loop until the UI finishes.  The done callback
     * fires from inside lv_timer_handler (either from the touch event or
     * the 3-second complete timer), flipping s_done.  Polling cadence
     * matches lvgl_display_task's normal loop period so the perceived
     * responsiveness is identical to steady-state operation.
     **/
    while (!s_done)
    {
        lv_timer_handler();
        osal_task_delay(BOOT_LV_TIMER_PERIOD_MS);
    }

    /* One more tick so the cleanup-side LVGL widget removal runs before we
     * return — keeps the screen tree clean for setup_ui's lv_scr_load. */
    touch_calibration_ui_cleanup();
    lv_timer_handler();

    lv_port_indev_set_bypass(false);

    if (CALIBRATION_SUCCESS == s_result)
    {
        DEBUG_OUT(i, TOUCH_CALIB_LOG_TAG, "calib boot: user calibration done");
    }
    else
    {
        DEBUG_OUT(w, TOUCH_CALIB_LOG_TAG,
                  "calib boot: user exited (st=%d) — raw passthrough",
                  (int)s_result);
    }
    return s_result;
}
//******************************* Functions *********************************//
