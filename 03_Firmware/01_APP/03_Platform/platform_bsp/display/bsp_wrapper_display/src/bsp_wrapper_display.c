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
static UINT32_T    s_cur_display_drv_idx = 0;

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
BOOL_T drv_adapter_display_mount(UINT32_T idx, drv_display_t *const drv)
{
    if (idx >= MAX_DISPLAY_DRV_NUM || drv == NULL)
    {
        return false;
    }

    /**
    * drv_display_t is a flat vtable, so a whole-struct copy captures every
    * slot — replaces the per-field copy. idx is pinned to the slot.
    *
    * NOTE: user_data is deliberately set to drv itself (the driver pointer),
    * NOT drv->user_data, preserving this wrapper's pre-refactor behaviour.
    * (Other wrappers copy drv->user_data; this divergence is intentional
    * here only to keep the refactor behaviour-neutral — worth revisiting.)
    **/
    s_display_driver[idx]           = *drv;
    s_display_driver[idx].idx       = idx;
    s_display_driver[idx].user_data = drv;

    s_cur_display_drv_idx = idx;
    return true;
}

/**
 * @brief   Forward driver-instantiation request to the active display
 *          driver.  Returns ERRORRESOURCE if no slot has an inst hook
 *          (e.g. when the adapter only does mount).
 */
platform_err_t display_drv_inst(void)
{
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_drv_inst)
    {
        return drv->pf_display_drv_inst(drv);
    }
    return PLATFORM_ERR_NO_RESOURCE;
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
 * @return  PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_draw_pixel    (UINT16_T x,  UINT16_T y,
                                           UINT16_T color)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_pixel)
    {
        ret = drv->pf_display_draw_pixel(drv, x, y, color);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }

    return ret;
}

/**
 * @brief   Forward fill-screen request to the display driver.
 */
platform_err_t display_fill_color    (UINT16_T color)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_fill_color)
    {
        ret = drv->pf_display_fill_color(drv, color);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward fill-region request to the display driver.
 */
platform_err_t display_fill_region   (UINT16_T x0, UINT16_T y0,
                                           UINT16_T x1, UINT16_T y1,
                                           UINT16_T color)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_fill_region)
    {
        ret = drv->pf_display_fill_region(drv, x0, y0, x1, y1, color);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-line request to the display driver.
 */
platform_err_t display_draw_line     (UINT16_T x0, UINT16_T y0,
                                           UINT16_T x1, UINT16_T y1,
                                           UINT16_T color)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_line)
    {
        ret = drv->pf_display_draw_line(drv, x0, y0, x1, y1, color);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-rectangle request to the display driver.
 */
platform_err_t display_draw_rectangle(UINT16_T x0, UINT16_T y0,
                                           UINT16_T x1, UINT16_T y1,
                                           UINT16_T color)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_rectangle)
    {
        ret = drv->pf_display_draw_rectangle(drv, x0, y0, x1, y1, color);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-circle request to the display driver.
 */
platform_err_t display_draw_circle   (UINT16_T x,  UINT16_T y,
                                           UINT16_T radius,
                                           UINT16_T color)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_circle)
    {
        ret = drv->pf_display_draw_circle(drv, x, y, radius, color);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-image request to the display driver.
 */
platform_err_t display_draw_image    (UINT16_T x0, UINT16_T y0,
                                           UINT16_T  w, UINT16_T  h,
                                           UINT16_T const* bitmap)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_image)
    {
        ret = drv->pf_display_draw_image(drv, x0, y0, w, h, bitmap);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward invert-colors request to the display driver.
 */
platform_err_t display_invert_colors (BOOL_T invert)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_invert_colors)
    {
        ret = drv->pf_invert_colors(drv, (UINT8_T)invert);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-character request to the display driver.
 */
platform_err_t display_draw_char            (UINT16_T x, UINT16_T y,
                                                      char c,
                                                  UINT16_T color,
                                                  UINT16_T bg_color)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_char)
    {
        ret = drv->pf_display_draw_char(drv, x, y, c, color, bg_color);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-string request to the display driver.
 */
platform_err_t display_draw_string          (UINT16_T x, UINT16_T y,
                                                const char* str,
                                                  UINT16_T color,
                                                  UINT16_T bg_color)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_string)
    {
        ret = drv->pf_display_draw_string(drv, x, y, str, color, bg_color);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-filled-rectangle request to the display driver.
 */
platform_err_t display_draw_filled_rectangle(UINT16_T x0, UINT16_T y0,
                                                  UINT16_T x1, UINT16_T y1,
                                                  UINT16_T color)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_filled_rectangle)
    {
        ret = drv->pf_display_draw_filled_rectangle(drv, x0, y0, x1, y1, color);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-filled-triangle request to the display driver.
 */
platform_err_t display_draw_filled_triangle (UINT16_T x0, UINT16_T y0,
                                                  UINT16_T x1, UINT16_T y1,
                                                  UINT16_T x2, UINT16_T y2,
                                                  UINT16_T color)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_filled_triangle)
    {
        ret = drv->pf_display_draw_filled_triangle(drv, x0, y0, x1, y1, x2, y2, color);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward draw-filled-circle request to the display driver.
 */
platform_err_t display_draw_filled_circle   (UINT16_T x,   UINT16_T y,
                                                  UINT16_T radius,
                                                  UINT16_T color)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_draw_filled_circle)
    {
        ret = drv->pf_display_draw_filled_circle(drv, x, y, radius, color);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

/**
 * @brief   Forward tear-effect request to the display driver.
 */
platform_err_t display_tear_effect(BOOL_T enable)
{
    platform_err_t ret = PLATFORM_OK;
    drv_display_t *drv = &s_display_driver[s_cur_display_drv_idx];
    if (drv->pf_display_tear_effect)
    {
        ret = drv->pf_display_tear_effect(drv, (UINT8_T)enable);
    }
    else
    {
        ret = PLATFORM_ERR_NO_RESOURCE;
    }
    return ret;
}

//******************************* Functions *********************************//
