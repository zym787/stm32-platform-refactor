/******************************************************************************
 * @file lv_port_disp.h
 *
 * @par dependencies
 * - lvgl.h
 * - bsp_wrapper_display.h
 *
 * @author Ethan-Hang
 *
 * @brief LVGL display port over the bsp_wrapper_display abstraction.
 *
 * @version V1.0 2026-04-24
 * @version V2.0 2026-04-26
 * @upgrade 2.0: Decoupled from bsp_st7789_driver.  Flush callback now calls
 *               display_draw_image() so the LVGL port has no compile-time
 *               dependency on a specific driver.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __LV_PORT_DISP_H__
#define __LV_PORT_DISP_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Register an LVGL display that flushes through the bsp_wrapper_display
 *        abstraction.  Must be called after lv_init() and after the display
 *        adapter has been registered (drv_adapter_display_register) and
 *        initialised (display_drv_init).
 *
 * @return true on success, false if LVGL rejects the registration.
 */
bool lv_port_disp_init(void);
//******************************* Functions *********************************//

#endif /* __LV_PORT_DISP_H__ */
