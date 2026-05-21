/******************************************************************************
 * @file lvgl_display_task.c
 *
 * @par dependencies
 * - bsp_wrapper_display.h
 * - bsp_wrapper_touch.h
 * - lvgl.h
 * - lv_port_disp.h
 * - lv_port_indev.h
 * - gui_guider.h
 *
 * @author Ethan-Hang
 *
 * @brief LVGL bring-up task.  Drives the display + touch entirely through
 *        the platform wrapper APIs (no direct driver coupling), wires them
 *        into LVGL's display + pointer indev, then hands control to the
 *        gui_guider-generated UI.
 *
 * Processing flow:
 *   display_drv_init -> display_fill_color(BLACK) ->
 *   touch_drv_init  -> touch_get_chip_id (probe) ->
 *   lv_init -> lv_port_disp_init -> lv_port_indev_init ->
 *   setup_ui(&guider_ui) ->
 *   loop { lv_timer_handler; delay 5 ms }
 *
 * @note  Vtable mounting happens pre-kernel via drv_adapter_*_register()
 *        in platform_io_register().  Actual driver instantiation
 *        (bsp_st7789_driver_inst / bsp_cst816t_inst) is deferred to this
 *        task — it depends on OSAL primitives (bus mutex, os delay) that
 *        are only valid after osKernelStart() — and is reached purely
 *        through the wrapper API (display_drv_inst / touch_drv_inst).
 *
 * @version V1.0 2026-04-25
 * @version V2.0 2026-04-26
 * @version V3.0 2026-04-28
 * @version V4.0 2026-04-28
 * @upgrade 2.0: Removed the local ST7789 / CST816T driver bind code; the
 *               task now drives both peripherals through bsp_wrapper_display
 *               and bsp_wrapper_touch.
 * @upgrade 3.0: Task now triggers OSAL-dependent driver instantiation
 *               before the wrapper init step (was previously running
 *               pre-kernel inside drv_adapter_*_register()).
 * @upgrade 4.0: Inst now goes through display_drv_inst / touch_drv_inst on
 *               the wrapper layer instead of bsp_adapter_port_*; the task
 *               no longer depends on the concrete adapter port headers.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "user_task_reso_config.h"
#include "bsp_wrapper_display.h"
#include "bsp_wrapper_touch.h"
#include "service_storage_facade.h"
#include "Debug.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_extflash.h"
#include "gui_guider.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define LV_TASK_BOOT_WAIT_MS      2000U
#define LV_TASK_TIMER_PERIOD_MS   5U

/* RGB565 black, used to clear the panel before LVGL takes over. */
#define LV_TASK_BG_COLOR_BLACK    0x0000U
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/**
 * gui_guider.h declares `extern lv_ui guider_ui` but the generated tree
 * does not provide a definition; supply it here so setup_ui() and the
 * generated screen handlers link.  Keeping it file-private to this task
 * gives one owner.
 **/
lv_ui guider_ui;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief      LVGL + gui_guider task entry: brings up display and touch
 *             through the platform wrapper APIs, then hands control to
 *             the gui_guider-generated UI.
 *
 * @param[in]  argument : Unused.
 *
 * @return     None.
 * */
void lvgl_display_task(void *argument)
{
    (void)argument;

    /**
     * Boot wait gives lower-priority init paths (e.g. logging, queues) time
     * to come up before we start hammering the display.
     **/
    osal_task_delay(LV_TASK_BOOT_WAIT_MS);

    DEBUG_OUT(i, ST7789_LOG_TAG, "lvgl_display_task start (gui_guider)");

    /**
     * Driver instantiation lives here (not in platform_io_register) because
     * the underlying bsp_*_inst() touches OSAL primitives that are only
     * valid post-kernel.  Reached through the wrapper API so the task does
     * not need to know which concrete driver is mounted; failures are
     * already logged by the adapter, and the existing fill_color / chip_id
     * checks below act as a second-line guard.
     */
    (void)display_drv_inst();
    (void)touch_drv_inst();

    /* 1. Display bring-up via wrapper. */
    display_drv_init();
    wp_display_status_t dret = display_fill_color(LV_TASK_BG_COLOR_BLACK);
    if (WP_DISPLAY_OK != dret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "display_fill_color failed = %d", (int)dret);
        for (;;)
        {
            osal_task_delay(1000U);
        }
    }

    /* 2. Touch bring-up via wrapper.  Display still works without touch,
     *    so a probe failure is logged but does not block the task. */
    touch_drv_init();
    uint8_t chip_id = 0u;
    wp_touch_status_t tret = touch_get_chip_id(&chip_id);
    bool touch_ok = (WP_TOUCH_OK == tret);
    if (touch_ok)
    {
        DEBUG_OUT(i, CST816T_LOG_TAG, "touch chip_id = 0x%02X",
                  (unsigned)chip_id);
    }
    else
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "touch chip_id probe failed = %d", (int)tret);
    }

    /* 3. LVGL core + display port. */
    lv_init();
    if (!lv_port_disp_init())
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG, "lvgl disp port init failed");
        for (;;)
        {
            osal_task_delay(1000U);
        }
    }

    /* 4. LVGL pointer indev (skip if touch probe failed). */
    if (touch_ok)
    {
        if (!lv_port_indev_init())
        {
            DEBUG_OUT(e, CST816T_ERR_LOG_TAG, "lvgl indev port init failed");
        }
    }

    /* 5. Provision LVGL pointer-sprite assets on the external W25Q64 flash
     *    and load them into RAM mirrors before setup_ui() runs.  Bootstrap
     *    is idempotent: if the magic matches it just re-reads the data. */
    ext_flash_status_t boot_st = storage_assets_bootstrap();
    if (EXT_FLASH_OK != boot_st)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "storage_assets_bootstrap failed st=%d", (int)boot_st);
        /* Fall through: setup_ui will still draw, but the *_ext needle
         * descriptors will reference uninitialised RAM. */
    }

    /* 5b. Register the external-flash line-streaming decoder.  Must run
     *     after lv_init() but before setup_ui(): once gui_guider creates
     *     the image widget, LVGL's first render walks the decoder list to
     *     identify the image source. */
    lv_port_extflash_init();

    /* 6. Hand off to gui_guider's generated UI. */
    setup_ui(&guider_ui);
    DEBUG_OUT(i, ST7789_LOG_TAG, "lvgl_display_task: gui_guider UI loaded");

    /* 7. LVGL service loop. */
    for (;;)
    {
        lv_timer_handler();
        osal_task_delay(LV_TASK_TIMER_PERIOD_MS);
    }
}
//******************************* Functions *********************************//
