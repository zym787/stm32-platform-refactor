/******************************************************************************
 * @file bsp_wrapper_display.c
 *
 * @par dependencies
 * - bsp_wrapper_display.h
 *
 * @author Ethan-Hang
 *
 * @brief Implementation of the abstract display interface.
 *        Maintains a static array of registered driver vtables and
 *        dispatches all public API calls to the currently active slot.
 *
 * Processing flow:
 *   1. drv_adapter_display_mount() copies the adapter vtable into
 *      s_display_driver[].
 *   2. All public API functions resolve the active driver via
 *      s_cur_display_drv_idx and forward through the corresponding
 *      function pointer.
 *
 * @version V1.0 2026-04-26
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_wrapper_display.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define MAX_DISPLAY_DRV_NUM 1

static drv_display_t s_display_driver[MAX_DISPLAY_DRV_NUM];
static uint32_t    s_cur_display_drv_idx = 0;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief   Register a display driver into the wrapper slot table.
 *
 * @param[in] idx : Slot index (0 ~ MAX_DISPLAY_DRV_NUM-1).
 * @param[in] drv : Pointer to the vtable-filled driver struct.
 *
 * @return  true  - Mounted successfully.
 *          false - Invalid index or NULL drv.
 */
bool drv_adapter_display_mount(uint32_t idx, drv_display_t *const drv)
{
    if (idx >= MAX_DISPLAY_DRV_NUM || drv == NULL)
    {
        return false;
    }

    s_display_driver[idx].idx = \
                          idx;
    s_display_driver[idx].dev_id = \
                          drv->dev_id;
    s_display_driver[idx].user_data = \
                          drv;
    s_display_driver[idx].pf_invert_colors = \
                          drv->pf_invert_colors;
    s_display_driver[idx].pf_display_drv_inst = \
                          drv->pf_display_drv_inst;
    s_display_driver[idx].pf_display_drv_init = \
                          drv->pf_display_drv_init;
    s_display_driver[idx].pf_display_draw_char = \
                          drv->pf_display_draw_char;
    s_display_driver[idx].pf_display_draw_line = \
                          drv->pf_display_draw_line;
    s_display_driver[idx].pf_display_draw_image = \
                          drv->pf_display_draw_image;
    s_display_driver[idx].pf_display_drv_deinit = \
                          drv->pf_display_drv_deinit;
    s_display_driver[idx].pf_display_fill_color = \
                          drv->pf_display_fill_color;
    s_display_driver[idx].pf_display_draw_pixel = \
                          drv->pf_display_draw_pixel;
    s_display_driver[idx].pf_display_draw_circle = \
                          drv->pf_display_draw_circle;
    s_display_driver[idx].pf_display_fill_region = \
                          drv->pf_display_fill_region;
    s_display_driver[idx].pf_display_draw_string = \
                          drv->pf_display_draw_string;
    s_display_driver[idx].pf_display_tear_effect = \
                          drv->pf_display_tear_effect;
    s_display_driver[idx].pf_display_draw_rectangle = \
                          drv->pf_display_draw_rectangle;
    s_display_driver[idx].pf_display_draw_filled_circle = \
                          drv->pf_display_draw_filled_circle;
    s_display_driver[idx].pf_display_draw_filled_triangle = \
                          drv->pf_display_draw_filled_triangle;
    s_display_driver[idx].pf_display_draw_filled_rectangle = \
                          drv->pf_display_draw_filled_rectangle;

    s_cur_display_drv_idx = idx;
    return true;
}

/**
 * @brief   Forward driver-instantiation request to the active display
 *          driver.  Returns ERRORRESOURCE if no slot has an inst hook
 *          (e.g. when the adapter only does mount).
 */
wp_display_status_t display_drv_inst(void)
{
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_drv_inst)
    {
        return drv->pf_display_drv_inst(drv);
    }
    return WP_DISPLAY_ERRORRESOURCE;
}

/**
 * @brief   Initialise the currently active display driver.
 *          Forwards to pf_display_drv_init in the registered vtable.
 */
void display_drv_init(void)
{
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_drv_init)
    {
        drv->pf_display_drv_init(drv);
    }
}

/**
 * @brief   Deinitialise the currently active display driver.
 *          Forwards to pf_display_drv_deinit in the registered vtable.
 */
void display_drv_deinit(void)
{
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_drv_deinit)
    {
        drv->pf_display_drv_deinit(drv);
    }
}

/**
 * @brief   Forward draw-pixel request to the display driver.
 *          Allocates one pixel at (x, y) with the given colour.
 *
 * @return  WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_draw_pixel    (uint16_t x,  uint16_t y,
                                           uint16_t color)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_pixel)
    {
        ret = drv->pf_display_draw_pixel(drv, x, y, color);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }

    return ret;
}

/**
 * @brief   Forward fill-screen request to the display driver.
 */
wp_display_status_t display_fill_color    (uint16_t color)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_fill_color)
    {
        ret = drv->pf_display_fill_color(drv, color);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward fill-region request to the display driver.
 */
wp_display_status_t display_fill_region   (uint16_t x0, uint16_t y0,
                                           uint16_t x1, uint16_t y1,
                                           uint16_t color)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_fill_region)
    {
        ret = drv->pf_display_fill_region(drv, x0, y0, x1, y1, color);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-line request to the display driver.
 */
wp_display_status_t display_draw_line     (uint16_t x0, uint16_t y0,
                                           uint16_t x1, uint16_t y1,
                                           uint16_t color)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_line)
    {
        ret = drv->pf_display_draw_line(drv, x0, y0, x1, y1, color);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-rectangle request to the display driver.
 */
wp_display_status_t display_draw_rectangle(uint16_t x0, uint16_t y0,
                                           uint16_t x1, uint16_t y1,
                                           uint16_t color)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_rectangle)
    {
        ret = drv->pf_display_draw_rectangle(drv, x0, y0, x1, y1, color);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-circle request to the display driver.
 */
wp_display_status_t display_draw_circle   (uint16_t x,  uint16_t y,
                                           uint16_t radius,
                                           uint16_t color)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_circle)
    {
        ret = drv->pf_display_draw_circle(drv, x, y, radius, color);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-image request to the display driver.
 */
wp_display_status_t display_draw_image    (uint16_t x0, uint16_t y0,
                                           uint16_t  w, uint16_t  h,
                                           uint16_t const* bitmap)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_image)
    {
        ret = drv->pf_display_draw_image(drv, x0, y0, w, h, bitmap);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward invert-colors request to the display driver.
 */
wp_display_status_t display_invert_colors (bool invert)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_invert_colors)
    {
        ret = drv->pf_invert_colors(drv, (uint8_t)invert);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-character request to the display driver.
 */
wp_display_status_t display_draw_char            (uint16_t x, uint16_t y,
                                                      char c,
                                                  uint16_t color,
                                                  uint16_t bg_color)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_char)
    {
        ret = drv->pf_display_draw_char(drv, x, y, c, color, bg_color);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-string request to the display driver.
 */
wp_display_status_t display_draw_string          (uint16_t x, uint16_t y,
                                                const char* str,
                                                  uint16_t color,
                                                  uint16_t bg_color)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_string)
    {
        ret = drv->pf_display_draw_string(drv, x, y, str, color, bg_color);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-filled-rectangle request to the display driver.
 */
wp_display_status_t display_draw_filled_rectangle(uint16_t x0, uint16_t y0,
                                                  uint16_t x1, uint16_t y1,
                                                  uint16_t color)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_filled_rectangle)
    {
        ret = drv->pf_display_draw_filled_rectangle(drv, x0, y0, x1, y1, color);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-filled-triangle request to the display driver.
 */
wp_display_status_t display_draw_filled_triangle (uint16_t x0, uint16_t y0,
                                                  uint16_t x1, uint16_t y1,
                                                  uint16_t x2, uint16_t y2,
                                                  uint16_t color)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_filled_triangle)
    {
        ret = drv->pf_display_draw_filled_triangle(drv, x0, y0, x1, y1, x2, y2, color);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-filled-circle request to the display driver.
 */
wp_display_status_t display_draw_filled_circle   (uint16_t x,   uint16_t y,
                                                  uint16_t radius,
                                                  uint16_t color)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_filled_circle)
    {
        ret = drv->pf_display_draw_filled_circle(drv, x, y, radius, color);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward tear-effect request to the display driver.
 */
wp_display_status_t display_tear_effect(bool enable)
{
    wp_display_status_t ret = WP_DISPLAY_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_tear_effect)
    {
        ret = drv->pf_display_tear_effect(drv, (uint8_t)enable);
    }
    else
    {
        ret = WP_DISPLAY_ERRORRESOURCE;
    }
    return ret;
}

//******************************* Functions *********************************//
