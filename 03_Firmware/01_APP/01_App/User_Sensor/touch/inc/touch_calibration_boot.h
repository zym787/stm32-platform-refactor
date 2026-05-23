/******************************************************************************
 * @file touch_calibration_boot.h
 *
 * @par dependencies
 * - bsp_cst816t_calibration.h
 *
 * @author Ethan-Hang
 *
 * @brief Boot-time orchestration of the touch panel calibration:
 *        load the saved coefficients from W25Q64 and, on any failure
 *        (missing magic, CRC mismatch, panel-size mismatch, ...) drive the
 *        on-screen calibration UI synchronously so the user can re-calibrate
 *        before the main UI is presented.
 *
 *        Lives under 01_App/User_Sensor/touch/ because it is application
 *        glue — it joins the algorithm (04_Impl/Bsp_Drivers/cst816t/
 *        calibration/), the LVGL UI (04_Impl/impl_middleware/lvgl/lvgl_ui/
 *        touch_calibration/), the indev bypass switch (lv_port_indev), and
 *        the boot sequence of lvgl_display_task.
 *
 * @version V1.0 2026-05-23
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __TOUCH_CALIBRATION_BOOT_H__
#define __TOUCH_CALIBRATION_BOOT_H__

//******************************** Includes *********************************//
#include "bsp_cst816t_calibration.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Synchronously load the saved calibration; if invalid, build the
 *        LVGL calibration screen and block until the user has finished it.
 *
 *        Must be called from the LVGL service task context (i.e. the same
 *        task that runs lv_timer_handler), AFTER lv_init / lv_port_disp_init
 *        / lv_port_indev_init / storage_assets_bootstrap, and BEFORE
 *        setup_ui — the function temporarily owns the LVGL service loop
 *        while the UI is active, then returns control to the caller.
 *
 * @return CALIBRATION_SUCCESS once a calibration is in-RAM (either loaded
 *         from storage or freshly captured).  Any other code means the
 *         user explicitly skipped — the indev will fall back to raw
 *         coordinates, which is degraded but functional.
 */
calibration_status_t touch_calibration_boot_apply(void);
//******************************* Functions *********************************//

#endif /* __TOUCH_CALIBRATION_BOOT_H__ */
