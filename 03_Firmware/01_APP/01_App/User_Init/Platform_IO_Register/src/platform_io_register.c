/******************************************************************************
 * @file platform_io_register.c
 *
 * @par dependencies
 * - bsp_adapter_port_temp_humi.h
 * - bsp_adapter_port_motion.h
 * - bsp_adapter_port_audio.h
 * - bsp_adapter_port_display.h
 * - bsp_adapter_port_touch.h
 *
 * @author Ethan-Hang
 *
 * @brief Centralized registration of all BSP platform IO adapters.
 *
 * @details Pure vtable mounting — every drv_adapter_*_register() call in
 *          here is required to be pre-kernel safe (no hardware touch, no
 *          OSAL primitive use).  Driver instantiation that depends on
 *          OSAL (bus mutex, os delay) is deferred to the consuming task:
 *            - display / touch: drv_adapter_display_inst() and
 *              drv_adapter_touch_inst() run inside lvgl_display_task.
 *            - temp_humi / motion / audio: each handler thread instantiates
 *              its own driver at task entry.
 *
 *          Processing flow:
 *          platform_io_register()
 *              └─ drv_adapter_temp_humi_register()   mount temp/humi vtable
 *              └─ drv_adapter_motion_register()      mount motion vtable
 *              └─ drv_adapter_audio_register()       mount audio vtable
 *              └─ drv_adapter_display_register()     mount display vtable
 *              └─ drv_adapter_touch_register()       mount touch vtable
 *
 * @version V1.0 2026-4-16
 * @version V2.0 2026-4-26
 * @version V3.0 2026-4-28
 * @upgrade 2.0: Added touch adapter registration.
 * @upgrade 3.0: Display / touch instantiation moved to lvgl_display_task;
 *               this file is now strictly vtable-mount only.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_io_register.h"
#include "bsp_adapter_port_temp_humi.h"
#include "bsp_adapter_port_motion.h"
#include "bsp_adapter_port_audio.h"
#include "bsp_adapter_port_display.h"
#include "bsp_adapter_port_touch.h"
#include "bsp_adapter_port_externflash.h"
#include "bsp_adapter_port_heart_rate.h"
//******************************** Includes *********************************//

//********************************* Macros **********************************//

//********************************* Macros **********************************//

//******************************* Variables *********************************//

//******************************* Variables *********************************//

//******************************* Functions *********************************//

/**
 * @brief Mount the wrapper-vtable for every BSP adapter.
 *
 * Called from main() ahead of osKernelInitialize().  Every register()
 * here must remain pre-kernel safe — only function-pointer assignment,
 * no hardware probe and no OSAL call.  The matching driver instantiation
 * (which does need OSAL) runs from each consumer task entry point.
 */
void platform_io_register(void)
{
    /* BSP adapter mounts. */
    drv_adapter_temp_humi_register();
    drv_adapter_motion_register();
    drv_adapter_audio_register();
    drv_adapter_display_register();
    drv_adapter_touch_register();
    drv_adapter_externflash_register();
    drv_adapter_heart_rate_register();
}

//******************************* Functions *********************************//
