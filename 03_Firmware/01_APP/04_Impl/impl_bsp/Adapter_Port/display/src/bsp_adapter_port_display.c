/******************************************************************************
 * @file bsp_adapter_port_display.c
 *
 * @par dependencies
 * - bsp_adapter_port_display.h
 * - bsp_wrapper_display.h
 * - bsp_st7789_driver.h
 * - st7789_integration.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief Adapter port for the ST7789 display controller.
 *
 * Owns a single static bsp_st7789_driver_t instance and exposes the
 * bsp_wrapper_display vtable that forwards each abstract call to the
 * matching ST7789 driver function.  Bring-up is split so OSAL-dependent
 * setup never runs before osKernelStart():
 *
 *   drv_adapter_display_register()        -- pre-kernel, vtable mount only
 *      └─ drv_adapter_display_mount(0, &display_drv)
 *
 *   display_drv_inst() (wrapper API)      -- post-kernel, in-task
 *      └─ pf_display_drv_inst (vtable)
 *           └─ display_drv_inst_adapter (this file)
 *                └─ bsp_st7789_driver_inst(&s_st7789_drv, st7789_input_arg.*)
 *
 *   wrapper API call ──► display_*_adapter() ──► s_st7789_drv.pf_st7789_*()
 *
 * @version V1.0 2026-04-25
 * @version V2.0 2026-04-26
 * @version V3.0 2026-04-28
 * @version V4.0 2026-04-28
 * @upgrade 2.0: Adapter now instantiates the driver from st7789_input_arg
 *               instead of relying on an externally-set pointer; init/deinit
 *               hooks reuse the static instance.
 * @upgrade 3.0: Split bring-up — register() is pure vtable mount, inst()
 *               drives bsp_st7789_driver_inst() from task context.
 * @upgrade 4.0: inst hook moved into the wrapper vtable; consumers reach
 *               it through display_drv_inst() instead of a public adapter
 *               entry point.  drv_adapter_display_inst() removed.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "bsp_adapter_port_display.h"
#include "bsp_wrapper_display.h"
#include "bsp_st7789_driver.h"
#include "st7789_integration.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************** Variables ********************************//
static bsp_st7789_driver_t s_st7789_drv;

/* True once bsp_st7789_driver_inst() has populated s_st7789_drv. */
static bool s_inst_ok = false;
//******************************** Variables ********************************//

//******************************* Functions *********************************//
/* ---------- inst / init / deinit ------------------------------------------ */
/**
 * @brief   Instantiate the ST7789 driver from st7789_input_arg.
 *
 * Wrapper-vtable hook reached via display_drv_inst().  Must run from task
 * context (post-kernel) — st7789_input_arg.p_os_interface and the SPI
 * bus mutex are only valid after osKernelStart().  Idempotent.
 */
static wp_display_status_t display_drv_inst_adapter(
    struct _drv_display_t *const dev)
{
    (void)dev;
    if (s_inst_ok)
    {
        return WP_DISPLAY_OK;
    }

    st7789_status_t st = bsp_st7789_driver_inst(
        &s_st7789_drv,
        st7789_input_arg.p_spi_interface,
        st7789_input_arg.p_timebase_interface,
        st7789_input_arg.p_os_interface,
        &st7789_input_arg.panel);
    if (ST7789_OK != st)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "display_drv_inst_adapter: st7789 inst failed = %d",
                  (int)st);
        s_inst_ok = false;
        return WP_DISPLAY_ERROR;
    }

    s_inst_ok = true;
    return WP_DISPLAY_OK;
}

/**
 * @brief   Forward init request to the ST7789 driver.
 */
static void display_drv_init_adapter(struct _drv_display_t *const dev)
{
    (void)dev;
    if (s_inst_ok && NULL != s_st7789_drv.pf_st7789_init)
    {
        (void)s_st7789_drv.pf_st7789_init(&s_st7789_drv);
    }
}

/**
 * @brief   Forward deinit request to the ST7789 driver.
 */
static void display_drv_deinit_adapter(struct _drv_display_t *const dev)
{
    (void)dev;
    if (s_inst_ok && NULL != s_st7789_drv.pf_st7789_deinit)
    {
        (void)s_st7789_drv.pf_st7789_deinit(&s_st7789_drv);
    }
}

/* ---------- basic functions ----------------------------------------------- */
/**
 * @brief   Forward fill-screen request to the ST7789 driver.
 */
static wp_display_status_t display_fill_color_adapter(
    struct _drv_display_t *const driver_instance, uint16_t color)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_fill_color)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_fill_color(
        &s_st7789_drv, color);
}

/**
 * @brief   Forward draw-pixel request to the ST7789 driver.
 */
static wp_display_status_t display_draw_pixel_adapter(
    struct _drv_display_t *const driver_instance,
    uint16_t x, uint16_t y, uint16_t color)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_draw_pixel)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_draw_pixel(
        &s_st7789_drv, x, y, color);
}

/**
 * @brief   Forward fill-region request to the ST7789 driver.
 */
static wp_display_status_t display_fill_region_adapter(
    struct _drv_display_t *const driver_instance,
    uint16_t x_start, uint16_t y_start,
    uint16_t x_end,   uint16_t y_end, uint16_t color)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_fill_region)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_fill_region(
        &s_st7789_drv, x_start, y_start, x_end, y_end, color);
}

/* ---------- graphic functions --------------------------------------------- */
/**
 * @brief   Forward draw-line request to the ST7789 driver.
 */
static wp_display_status_t display_draw_line_adapter(
    struct _drv_display_t *const driver_instance,
    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_draw_line)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_draw_line(
        &s_st7789_drv, x0, y0, x1, y1, color);
}

/**
 * @brief   Forward draw-rectangle request to the ST7789 driver.
 */
static wp_display_status_t display_draw_rectangle_adapter(
    struct _drv_display_t *const driver_instance,
    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_draw_rectangle)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_draw_rectangle(
        &s_st7789_drv, x0, y0, x1, y1, color);
}

/**
 * @brief   Forward draw-circle request to the ST7789 driver.
 */
static wp_display_status_t display_draw_circle_adapter(
    struct _drv_display_t *const driver_instance,
    uint16_t x_center, uint16_t y_center, uint16_t radius, uint16_t color)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_draw_circle)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_draw_circle(
        &s_st7789_drv, x_center, y_center, radius, color);
}

/**
 * @brief   Forward draw-image request to the ST7789 driver.
 */
static wp_display_status_t display_draw_image_adapter(
    struct _drv_display_t *const driver_instance,
    uint16_t x_start, uint16_t y_start,
    uint16_t w, uint16_t h, uint16_t const *bitmap)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_draw_image)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_draw_image(
        &s_st7789_drv, x_start, y_start, w, h, bitmap);
}

/**
 * @brief   Forward invert-colors request to the ST7789 driver.
 */
static wp_display_status_t invert_colors_adapter(
    struct _drv_display_t *const driver_instance, uint8_t invert)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_invert_colors)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_invert_colors(
        &s_st7789_drv, invert);
}

/* ---------- text functions ------------------------------------------------ */
/**
 * @brief   Forward draw-character request to the ST7789 driver.
 */
static wp_display_status_t display_draw_char_adapter(
    struct _drv_display_t *const driver_instance,
    uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_draw_char)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_draw_char(
        &s_st7789_drv, x, y, ch, color, bg_color);
}

/**
 * @brief   Forward draw-string request to the ST7789 driver.
 */
static wp_display_status_t display_draw_string_adapter(
    struct _drv_display_t *const driver_instance,
    uint16_t x, uint16_t y, char const *str, uint16_t color, uint16_t bg_color)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_draw_string)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_draw_string(
        &s_st7789_drv, x, y, str, color, bg_color);
}

/* ---------- extended functions -------------------------------------------- */
/**
 * @brief   Forward draw-filled-rectangle request to the ST7789 driver.
 */
static wp_display_status_t display_draw_filled_rectangle_adapter(
    struct _drv_display_t *const driver_instance,
    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_draw_filled_rectangle)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_draw_filled_rectangle(
        &s_st7789_drv, x0, y0, x1, y1, color);
}

/**
 * @brief   Forward draw-filled-triangle request to the ST7789 driver.
 */
static wp_display_status_t display_draw_filled_triangle_adapter(
    struct _drv_display_t *const driver_instance,
    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
    uint16_t x2, uint16_t y2, uint16_t color)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_draw_filled_triangle)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_draw_filled_triangle(
        &s_st7789_drv, x0, y0, x1, y1, x2, y2, color);
}

/**
 * @brief   Forward draw-filled-circle request to the ST7789 driver.
 */
static wp_display_status_t display_draw_filled_circle_adapter(
    struct _drv_display_t *const driver_instance,
    uint16_t x_center, uint16_t y_center, uint16_t radius, uint16_t color)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_draw_filled_circle)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_draw_filled_circle(
        &s_st7789_drv, x_center, y_center, radius, color);
}

/**
 * @brief   Forward tear-effect request to the ST7789 driver.
 */
static wp_display_status_t display_tear_effect_adapter(
    struct _drv_display_t *const driver_instance, uint8_t enable)
{
    (void)driver_instance;
    if (!s_inst_ok || NULL == s_st7789_drv.pf_st7789_tear_effect)
    {
        return WP_DISPLAY_ERRORRESOURCE;
    }
    return (wp_display_status_t)s_st7789_drv.pf_st7789_tear_effect(
        &s_st7789_drv, enable);
}

/* ---------- Wrapper vtable registration ----------------------------------- */
/**
 * @brief Mount the adapter vtable into bsp_wrapper_display.
 *
 * Pre-kernel safe: touches no hardware and no OSAL primitive.  The driver
 * instance behind the vtable stays uninstantiated until
 * drv_adapter_display_inst() runs; wrapper calls return
 * WP_DISPLAY_ERRORRESOURCE until then.
 */
void drv_adapter_display_register(void)
{
    drv_display_t display_drv = {
        .idx                              = 0,
        .dev_id                           = 0,
        .user_data                        = NULL,
        .pf_display_drv_inst              = display_drv_inst_adapter,
        .pf_display_drv_init              = display_drv_init_adapter,
        .pf_display_drv_deinit            = display_drv_deinit_adapter,
        .pf_display_fill_color            = display_fill_color_adapter,
        .pf_display_draw_pixel            = display_draw_pixel_adapter,
        .pf_display_fill_region           = display_fill_region_adapter,
        .pf_display_draw_line             = display_draw_line_adapter,
        .pf_display_draw_rectangle        = display_draw_rectangle_adapter,
        .pf_display_draw_circle           = display_draw_circle_adapter,
        .pf_display_draw_image            = display_draw_image_adapter,
        .pf_invert_colors                 = invert_colors_adapter,
        .pf_display_draw_char             = display_draw_char_adapter,
        .pf_display_draw_string           = display_draw_string_adapter,
        .pf_display_draw_filled_rectangle = display_draw_filled_rectangle_adapter,
        .pf_display_draw_filled_triangle  = display_draw_filled_triangle_adapter,
        .pf_display_draw_filled_circle    = display_draw_filled_circle_adapter,
        .pf_display_tear_effect           = display_tear_effect_adapter,
    };

    (void)drv_adapter_display_mount(0u, &display_drv);
}
//******************************* Functions *********************************//
