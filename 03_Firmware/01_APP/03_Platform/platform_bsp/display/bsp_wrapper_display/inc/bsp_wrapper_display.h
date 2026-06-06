/******************************************************************************
 * @file bsp_wrapper_display.h
 *
 * @par dependencies
 * - platform_type.h
 *
 * @author Ethan-Hang
 *
 * @brief Abstract interface for display controller access.
 *        Decouples the application (LVGL port, drawing consumers) from any
 *        specific display driver.  The adapter layer registers a concrete
 *        driver via drv_adapter_display_mount(); the application calls the
 *        public API without knowing which hardware is underneath.
 *
 * Processing flow:
 *   1. Adapter calls drv_adapter_display_mount() to register the driver vtable.
 *   2. Application calls display_drv_inst() once at task entry — this drives
 *      the adapter's bsp_*_inst() (OSAL-dependent, post-kernel only).
 *   3. Application calls display_drv_init() once at startup.
 *   4. Application calls drawing primitives (draw_pixel, draw_line, etc.)
 *      through the wrapper API, which dispatches to the registered driver.
 *
 * @version V1.0 2026-04-26
 * @version V2.0 2026-04-28
 * @upgrade 2.0: Added display_drv_inst() so consumers reach the OSAL-
 *               dependent driver instantiation through the wrapper instead
 *               of the concrete adapter port header.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_WRAPPER_DISPLAY_H__
#define __BSP_WRAPPER_DISPLAY_H__

//******************************** Includes *********************************//
#include "platform_type.h"
#include "platform_error.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//

/**
 * @brief Driver vtable for a display controller.
 *        Filled by the adapter layer; opaque to the application.
 */
typedef struct _drv_display_t
{
    UINT32_T                       idx;       /* Slot index in wrapper array    */
    UINT32_T                    dev_id;       /* Hardware device identifier     */
    void *                   user_data;       /* Adapter private context        */

    platform_err_t (*pf_display_drv_inst  )(
                                  struct _drv_display_t *const dev);
    void (*pf_display_drv_init  )(struct _drv_display_t *const dev);
    void (*pf_display_drv_deinit)(struct _drv_display_t *const dev);

    platform_err_t (*pf_display_fill_color     )(
                                 struct _drv_display_t *const driver_instance, 
                                              UINT16_T                  color);
    platform_err_t (*pf_display_draw_pixel     )(
                                 struct _drv_display_t *const driver_instance, 
                                              UINT16_T                      x,
                                              UINT16_T                      y,
                                              UINT16_T                  color);
    platform_err_t (*pf_display_fill_region    )(
                                 struct _drv_display_t *const driver_instance, 
                                              UINT16_T                x_start,
                                              UINT16_T                y_start,
                                              UINT16_T                  x_end,
                                              UINT16_T                  y_end,
                                              UINT16_T                  color);
    platform_err_t (*pf_display_draw_line      )(
                                 struct _drv_display_t *const driver_instance,
                                              UINT16_T                     x0,
                                              UINT16_T                     y0,
                                              UINT16_T                     x1,
                                              UINT16_T                     y1,
                                              UINT16_T                  color);
    platform_err_t (*pf_display_draw_rectangle )(
                                 struct _drv_display_t *const driver_instance,
                                              UINT16_T                     x0,
                                              UINT16_T                     y0,
                                              UINT16_T                     x1,
                                              UINT16_T                     y1,
                                              UINT16_T                  color);
    platform_err_t (*pf_display_draw_circle    )(
                                 struct _drv_display_t *const driver_instance,
                                              UINT16_T               x_center,
                                              UINT16_T               y_center,
                                              UINT16_T                 radius,
                                              UINT16_T                  color);
    platform_err_t (*pf_display_draw_image     )(
                                 struct _drv_display_t *const driver_instance,
                                              UINT16_T                x_start,
                                              UINT16_T                y_start,
                                              UINT16_T                      w,
                                              UINT16_T                      h,
                                              UINT16_T  const*         bitmap);
    platform_err_t (*pf_invert_colors         )(
                                 struct _drv_display_t *const driver_instance,
                                               UINT8_T                 invert);
    platform_err_t (*pf_display_draw_char      )(
                                 struct _drv_display_t *const driver_instance,
                                              UINT16_T                      x,
                                              UINT16_T                      y,
                                                  char                     ch,
                                              UINT16_T                  color, 
                                              UINT16_T               bg_color);
    platform_err_t (*pf_display_draw_string    )(
                                 struct _drv_display_t *const driver_instance,
                                              UINT16_T                      x,
                                              UINT16_T                      y,
                                                  char  const*            str, 
                                              UINT16_T                  color, 
                                              UINT16_T               bg_color);
    platform_err_t (*pf_display_draw_filled_rectangle)(
                                 struct _drv_display_t *const driver_instance,
                                              UINT16_T                     x0,
                                              UINT16_T                     y0,
                                              UINT16_T                     x1,
                                              UINT16_T                     y1, 
                                              UINT16_T                  color);
    platform_err_t (*pf_display_draw_filled_triangle )(
                                 struct _drv_display_t *const driver_instance,
                                              UINT16_T                     x0,
                                              UINT16_T                     y0,
                                              UINT16_T                     x1,
                                              UINT16_T                     y1, 
                                              UINT16_T                     x2, 
                                              UINT16_T                     y2, 
                                              UINT16_T                  color);
    platform_err_t (*pf_display_draw_filled_circle   )(
                                 struct _drv_display_t *const driver_instance,
                                              UINT16_T               x_center,
                                              UINT16_T               y_center,
                                              UINT16_T                 radius,
                                              UINT16_T                  color);

    platform_err_t (*pf_display_tear_effect          )(
                                 struct _drv_display_t *const driver_instance,
                                               UINT8_T                 enable);
} drv_display_t;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief   Register a display driver into the wrapper slot table.
 *          Called exclusively by the adapter layer at system init.
 *
 * @param[in] idx : Slot index (0 ~ MAX_DISPLAY_DRV_NUM-1).
 * @param[in] drv : Pointer to the vtable-filled driver struct.
 *
 * @return  true  - Mounted successfully.
 *          false - Invalid index or NULL drv.
 */
BOOL_T drv_adapter_display_mount(UINT32_T idx, drv_display_t *const drv);

/**
 * @brief Instantiate the currently active display driver.
 *
 * Forwards to pf_display_drv_inst in the registered vtable.  Must run from
 * task context (post-kernel) because the underlying driver inst typically
 * binds OS primitives (bus mutex, os delay) that are only valid after
 * osKernelStart().  Idempotent — implementations should ignore repeated
 * calls after a successful instantiation.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver
 *         is mounted or the slot has no inst hook, other values on driver
 *         failure (logged by the adapter).
 */
platform_err_t display_drv_inst(void);

/**
 * @brief Initialize the currently active display driver.
 *        Forwards to pf_display_drv_init in the registered vtable.
 */
void display_drv_init  (void);

/**
 * @brief Deinitialize the currently active display driver.
 *        Forwards to pf_display_drv_deinit in the registered vtable.
 */
void display_drv_deinit(void);

/**
 * @brief Forward draw-pixel request to the display driver.
 *
 * @param[in] x     : X pixel coordinate.
 * @param[in] y     : Y pixel coordinate.
 * @param[in] color : 16-bit RGB565 colour value.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_draw_pixel    (UINT16_T x,  UINT16_T y,
                                           UINT16_T color);

/**
 * @brief Fill the entire screen with a single colour.
 *
 * @param[in] color : 16-bit RGB565 fill colour.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_fill_color    (UINT16_T color);

/**
 * @brief Fill a rectangular region with a single colour.
 *
 * @param[in] x0,y0 : Top-left corner.
 * @param[in] x1,y1 : Bottom-right corner.
 * @param[in] color : 16-bit RGB565 fill colour.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_fill_region   (UINT16_T x0, UINT16_T y0,
                                           UINT16_T x1, UINT16_T y1,
                                           UINT16_T color);

/**
 * @brief Draw a line between two points.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_draw_line     (UINT16_T x0, UINT16_T y0,
                                           UINT16_T x1, UINT16_T y1,
                                           UINT16_T color);

/**
 * @brief Draw a rectangle outline.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_draw_rectangle(UINT16_T x0, UINT16_T y0,
                                           UINT16_T x1, UINT16_T y1,
                                           UINT16_T color);

/**
 * @brief Draw a circle outline.
 *
 * @param[in] x,y       : Centre coordinates.
 * @param[in] radius    : Circle radius in pixels.
 * @param[in] color     : 16-bit RGB565 colour.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_draw_circle   (UINT16_T x,  UINT16_T y,
                                           UINT16_T radius,
                                           UINT16_T color);

/**
 * @brief Draw a bitmap image at the specified position.
 *
 * @param[in] x0,y0 : Top-left corner.
 * @param[in] w,h   : Image width and height in pixels.
 * @param[in] bitmap: Pointer to 16-bit RGB565 pixel data.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_draw_image    (UINT16_T x0, UINT16_T y0,
                                           UINT16_T  w, UINT16_T  h,
                                           UINT16_T const* bitmap);

/**
 * @brief Invert or restore display colours.
 *
 * @param[in] invert : true to invert, false to restore.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_invert_colors (BOOL_T invert);

/**
 * @brief Draw a single character using the built-in font.
 *
 * @param[in] x,y       : Character top-left position.
 * @param[in] c         : Character to draw.
 * @param[in] color     : Foreground colour (RGB565).
 * @param[in] bg_color  : Background colour (RGB565).
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_draw_char            (UINT16_T x, UINT16_T y,
                                                      char c,
                                                  UINT16_T color,
                                                  UINT16_T bg_color);

/**
 * @brief Draw a null-terminated string using the built-in font.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_draw_string          (UINT16_T x, UINT16_T y,
                                                const char* str,
                                                  UINT16_T color,
                                                  UINT16_T bg_color);

/**
 * @brief Draw a filled rectangle.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_draw_filled_rectangle(UINT16_T x0, UINT16_T y0,
                                                  UINT16_T x1, UINT16_T y1,
                                                  UINT16_T color);

/**
 * @brief Draw a filled triangle.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_draw_filled_triangle (UINT16_T x0, UINT16_T y0,
                                                  UINT16_T x1, UINT16_T y1,
                                                  UINT16_T x2, UINT16_T y2,
                                                  UINT16_T color);

/**
 * @brief Draw a filled circle.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_draw_filled_circle   (UINT16_T x,   UINT16_T y,
                                                  UINT16_T radius,
                                                  UINT16_T color);

/**
 * @brief Enable or disable the display tear-effect (TE) output.
 *
 * @param[in] enable : true to enable, false to disable.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NO_RESOURCE if no driver mounted.
 */
platform_err_t display_tear_effect(BOOL_T enable);

//******************************* Functions *********************************//

#endif /* __BSP_WRAPPER_DISPLAY_H__ */
