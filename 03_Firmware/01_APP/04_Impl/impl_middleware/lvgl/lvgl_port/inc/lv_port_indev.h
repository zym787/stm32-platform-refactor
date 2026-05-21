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
 * @upgrade 2.0: Decoupled from bsp_cst816t_driver.  Read callback now calls
 *               touch_get_finger_num / touch_get_xy through the wrapper.
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
//******************************* Functions *********************************//

#endif /* __LV_PORT_INDEV_H__ */
