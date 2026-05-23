/******************************************************************************
 * @file touch_calibration_ui.h
 *
 * @par dependencies
 * - lvgl.h
 * - bsp_cst816t_calibration.h
 *
 * @author Ethan-Hang
 *
 * @brief Fullscreen LVGL calibration UI driving the 9-point capture
 *        sequence.  Used by the boot orchestrator on first launch and any
 *        future "recalibrate" entry point.
 *
 *        Flow (state machine inside the .c):
 *
 *            INIT -> SHOW_POINT -> WAIT_TOUCH -> SHOW_POINT -> ... -> CALCULATING
 *                                                                          |
 *                                                              +-----------+----------+
 *                                                              |                      |
 *                                                          COMPLETE                ERROR
 *                                                              |                      |
 *                                                       done_cb(SUCCESS)      done_cb(<err>)
 *
 *        Sample collection uses the live LVGL indev (LV_EVENT_PRESSED on
 *        the calibration screen, lv_indev_get_point).  The orchestrator
 *        sets lv_port_indev_set_bypass(true) before calling _start so the
 *        indev passes the raw panel coordinate through.
 *
 * @version V1.0 2026-05-23
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __TOUCH_CALIBRATION_UI_H__
#define __TOUCH_CALIBRATION_UI_H__

//******************************** Includes *********************************//
#include "bsp_cst816t_calibration.h"
//******************************** Includes *********************************//

//******************************** Typedefs *********************************//
/**
 * @brief Invoked exactly once when the calibration session ends, either by
 *        successful sample completion, calculation failure, or per-point
 *        timeout.  The callback context is the LVGL task (lvgl_display_task
 *        on this project) — keep it short.
 */
typedef void (*touch_calibration_ui_done_cb_t)(calibration_status_t result);
//******************************** Typedefs *********************************//

//******************************* Functions *********************************//
/**
 * @brief Build the LVGL screen + widgets and reset the internal state.
 *        Must be called once before _start.  Does NOT load the screen.
 */
calibration_status_t touch_calibration_ui_init(void);

/**
 * @brief Push the calibration screen via lv_scr_load and begin sampling
 *        the first point.  The done_cb fires from the lv_timer / event
 *        path once the user has either completed all points (or hit the
 *        skip / restart / timeout path).
 */
void touch_calibration_ui_start(touch_calibration_ui_done_cb_t done_cb);

/**
 * @brief Tear down the screen + widgets created by _init.  Safe to call
 *        repeatedly; a no-op when nothing is allocated.  Does not switch
 *        the active screen — the caller is responsible for lv_scr_load'ing
 *        the next screen before calling this.
 */
void touch_calibration_ui_cleanup(void);
//******************************* Functions *********************************//

#endif /* __TOUCH_CALIBRATION_UI_H__ */
