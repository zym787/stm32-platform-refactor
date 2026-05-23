/******************************************************************************
 * @file lv_port_indev.h
 *
 * @par dependencies
 * - lvgl.h
 * - bsp_wrapper_touch.h
 *
 * @author Ethan-Hang
 *
 * @brief LVGL pointer input-device port over the bsp_wrapper_touch
 *        abstraction.
 *
 * @version V1.0 2026-04-25
 * @version V2.0 2026-04-26
 * @version V3.0 2026-05-23
 * @upgrade 2.0: Decoupled from bsp_cst816t_driver.  Read callback now calls
 *               touch_get_finger_num / touch_get_xy through the wrapper.
 * @upgrade 3.0: Reads now go through touch_calibration_apply_matrix() when
 *               a calibration is loaded.  Added lv_port_indev_set_bypass()
 *               so the calibration UI itself can sample raw coordinates
 *               while the read callback is otherwise transforming them.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __LV_PORT_INDEV_H__
#define __LV_PORT_INDEV_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief      Register an LVGL pointer indev backed by the bsp_wrapper_touch
 *             abstraction.  Must be called after lv_init() and after the
 *             touch adapter has been registered (drv_adapter_touch_register)
 *             and initialised (touch_drv_init).
 *
 * @return     true on success, false on LVGL registration error.
 * */
bool lv_port_indev_init(void);

/**
 * @brief      Toggle the calibration-bypass flag in the indev read path.
 *
 *             Used exclusively by the touch calibration UI: while the UI is
 *             collecting samples it needs the indev to feed LVGL raw panel
 *             coordinates (because the UI's LV_EVENT_PRESSED handler asks
 *             LVGL for the indev's current point and uses it as the raw
 *             value to feed into touch_calibration_add_point).  Outside
 *             that window the read path applies the saved affine transform
 *             on every sample.
 *
 *             Default state is false (i.e. apply calibration when valid).
 *
 * @param[in]  bypass  true to skip apply_matrix, false to resume.
 */
void lv_port_indev_set_bypass(bool bypass);
//******************************* Functions *********************************//

#endif /* __LV_PORT_INDEV_H__ */
