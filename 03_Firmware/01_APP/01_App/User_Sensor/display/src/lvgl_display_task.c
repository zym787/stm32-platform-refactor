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
 *   lv_init -> lv_log_register_print_cb (bridge to DEBUG_OUT) ->
 *   lv_port_disp_init -> lv_port_indev_init ->
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
 * @version V5.0 2026-06-04
 * @upgrade 2.0: Removed the local ST7789 / CST816T driver bind code; the
 *               task now drives both peripherals through bsp_wrapper_display
 *               and bsp_wrapper_touch.
 * @upgrade 3.0: Task now triggers OSAL-dependent driver instantiation
 *               before the wrapper init step (was previously running
 *               pre-kernel inside drv_adapter_*_register()).
 * @upgrade 4.0: Inst now goes through display_drv_inst / touch_drv_inst on
 *               the wrapper layer instead of bsp_adapter_port_*; the task
 *               no longer depends on the concrete adapter port headers.
 * @upgrade 5.0: lvgl_log_output_cb now deep-matches LVGL's level prefix
 *               ([Trace]/[Info]/[Warn]/[Error]/[User]) and forwards through
 *               the matching EasyLogger level (d/i/w/e) instead of flattening
 *               everything to info; LVGL's duplicate "[Level]\t(time)\t"
 *               header is stripped so EasyLogger's own level + timestamp win.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_type.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "user_task_reso_config.h"
#include "bsp_wrapper_display.h"
#include "bsp_wrapper_touch.h"
#include "service_storage_facade.h"
#include "platform_def.h"
#include "Debug.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_extflash.h"
#include "gui_guider.h"

#include "touch_calibration_boot.h"
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
 * LVGL log levels, mirrored from lv_log.h's textual prefixes.  This LVGL
 * build hands the print callback only a pre-formatted string (the callback
 * signature is `void(const char *)`, no level enum), so the level has to be
 * recovered from the leading "[Trace]/[Info]/[Warn]/[Error]/[User]" token.
 */
typedef enum
{
    LVGL_LOG_LVL_TRACE = 0,
    LVGL_LOG_LVL_INFO,
    LVGL_LOG_LVL_WARN,
    LVGL_LOG_LVL_ERROR,
    LVGL_LOG_LVL_USER,
} lvgl_log_level_t;

/**
 * @brief  Recover the LVGL log level from the buffer's "[Level]" prefix and
 *         locate the human-readable message body.
 *
 *         LVGL formats every _lv_log_add() line as:
 *             "[Warn]\t(t.ms, +dt)\t func: message \t(in file line #n)\n"
 *         The leading "[Level]\t(time)\t " header duplicates the level tag +
 *         timestamp that EasyLogger already prints, so we skip past it (the
 *         body starts after the second tab) and keep only
 *         "func: message (in file line #n)".  Plain lv_log() output has no
 *         "[Level]" prefix nor tabs — it falls through to INFO + whole buffer.
 *
 * @param[in]  buf  : NUL-terminated log string from LVGL (non-NULL).
 * @param[out] body : receives a pointer into @p buf at the message body.
 *
 * @return     The decoded level (defaults to LVGL_LOG_LVL_INFO).
 */
static lvgl_log_level_t lvgl_log_parse_level(const char *buf, const char **body)
{
    static const struct
    {
        const char       *name;
        lvgl_log_level_t   level;
    } k_level_map[] = {
        { "[Trace]", LVGL_LOG_LVL_TRACE },
        { "[Info]",  LVGL_LOG_LVL_INFO  },
        { "[Warn]",  LVGL_LOG_LVL_WARN  },
        { "[Error]", LVGL_LOG_LVL_ERROR },
        { "[User]",  LVGL_LOG_LVL_USER  },
    };

    lvgl_log_level_t level = LVGL_LOG_LVL_INFO;

    /* Default: unknown format / plain lv_log() — keep the whole buffer. */
    *body = buf;

    if ('[' != buf[0])
    {
        return level;
    }

    for (SIZE_T i = 0U; i < ARRAY_SIZE(k_level_map); i++)
    {
        if (0 == strncmp(buf, k_level_map[i].name,
                         strlen(k_level_map[i].name)))
        {
            level = k_level_map[i].level;
            break;
        }
    }

    /* Advance the body pointer past the "[Level]\t(time)\t " header — i.e. to
     * just after the second tab.  Leave *body at the whole buffer if the
     * format is unexpected (fewer than two tabs). */
    int tabs = 0;
    for (const char *p = buf; '\0' != *p; p++)
    {
        if ('\t' == *p)
        {
            if (2 == ++tabs)
            {
                const char *msg = p + 1;
                while (' ' == *msg)
                {
                    msg++;
                }
                *body = msg;
                break;
            }
        }
    }

    return level;
}

/**
 * @brief LVGL log bridge: receives pre-formatted log buffers from LVGL's
 *        internal _lv_log_add() and routes them through the project's
 *        centralized DEBUG_OUT → EasyLogger → RTT pipeline, mapping each
 *        LVGL level onto the matching EasyLogger level so warnings/errors
 *        surface as W/E (not flattened to info).
 *
 *        DEBUG_OUT()'s level is a compile-time token (elog_##LEVEL), hence
 *        the per-level switch instead of a single parameterized call.
 *
 *        LVGL's buffer ends with '\n'; we strip it because DEBUG_OUT
 *        (EasyLogger) appends its own line terminator.
 *
 * @param[in] buf : NUL-terminated log string from LVGL.
 */
static void lvgl_log_output_cb(const char *buf)
{
    if (NULL == buf)
    {
        return;
    }

    const char            *body  = buf;
    const lvgl_log_level_t  level = lvgl_log_parse_level(buf, &body);

    /* Temporarily strip the trailing '\n'.  LVGL's buffer is a stack-local in
     * _lv_log_add(); writing is safe and we restore it before returning.
     * Truncating the whole buffer also terminates the body substring. */
    SIZE_T      blen     = strlen(buf);
    BOOL_T        stripped = (blen > 0U) && ('\n' == buf[blen - 1U]);
    char *const mut_buf  = (char *const)buf;
    if (stripped)
    {
        mut_buf[blen - 1U] = '\0';
    }

    switch (level)
    {
        case LVGL_LOG_LVL_TRACE:
            DEBUG_OUT(d, LVGL_LOG_TAG, "%s", body);
            break;
        case LVGL_LOG_LVL_WARN:
            DEBUG_OUT(w, LVGL_LOG_TAG, "%s", body);
            break;
        case LVGL_LOG_LVL_ERROR:
            DEBUG_OUT(e, LVGL_LOG_TAG, "%s", body);
            break;
        case LVGL_LOG_LVL_USER:
        case LVGL_LOG_LVL_INFO:
        default:
            DEBUG_OUT(i, LVGL_LOG_TAG, "%s", body);
            break;
    }

    if (stripped)
    {
        mut_buf[blen - 1U] = '\n';
    }
}

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
    platform_err_t dret = display_fill_color(LV_TASK_BG_COLOR_BLACK);
    if (PLATFORM_OK != dret)
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
    UINT8_T chip_id = 0u;
    platform_err_t tret = touch_get_chip_id(&chip_id);
    BOOL_T touch_ok = (PLATFORM_OK == tret);
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
    lv_log_register_print_cb(lvgl_log_output_cb);
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

    /* 5c. Touch-panel calibration.  Loads saved coefficients from W25Q64
     *     if valid; otherwise drives the 9-point on-screen UI synchronously
     *     before the main UI is shown.  Skipped when the touch probe failed
     *     above (calibration requires a working panel). */
    if (touch_ok)
    {
        const calibration_status_t calib_st = touch_calibration_boot_apply();
        if (CALIBRATION_SUCCESS != calib_st)
        {
            DEBUG_OUT(w, TOUCH_CALIB_LOG_TAG,
                      "calib boot: skipped/failed st=%d — raw indev",
                      (int)calib_st);
        }
    }

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
