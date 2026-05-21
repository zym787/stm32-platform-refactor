/******************************************************************************
 * @file bsp_wrapper_display.h
 *
 * @par dependencies
 * - stdint.h
 * - stdbool.h
 * - stddef.h
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
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    WP_DISPLAY_OK               = 0,           /* Operation successful       */
    WP_DISPLAY_ERROR            = 1,           /* General error              */
    WP_DISPLAY_ERRORTIMEOUT     = 2,           /* Timeout error              */
    WP_DISPLAY_ERRORRESOURCE    = 3,           /* Resource unavailable       */
    WP_DISPLAY_ERRORPARAMETER   = 4,           /* Invalid parameter          */
    WP_DISPLAY_ERRORNOMEMORY    = 5,           /* Out of memory              */
    WP_DISPLAY_ERRORUNSUPPORTED = 6,           /* Unsupported feature        */
    WP_DISPLAY_ERRORISR         = 7,           /* ISR context error          */
    WP_DISPLAY_RESERVED         = 0xFF,        /* Reserved                   */
} wp_display_status_t;

/**
 * @brief Driver vtable for a display controller.
 *        Filled by the adapter layer; opaque to the application.
 */
typedef struct _drv_display_t
{
    uint32_t                       idx;       /* Slot index in wrapper array    */
    uint32_t                    dev_id;       /* Hardware device identifier     */
    void *                   user_data;       /* Adapter private context        */

    wp_display_status_t (*pf_display_drv_inst  )(
                                  struct _drv_display_t *const dev);
    void (*pf_display_drv_init  )(struct _drv_display_t *const dev);
    void (*pf_display_drv_deinit)(struct _drv_display_t *const dev);

    wp_display_status_t (*pf_display_fill_color     )(
                                 struct _drv_display_t *const driver_instance, 
                                              uint16_t                  color);
    wp_display_status_t (*pf_display_draw_pixel     )(
                                 struct _drv_display_t *const driver_instance, 
                                              uint16_t                      x,
                                              uint16_t                      y,
                                              uint16_t                  color);
    wp_display_status_t (*pf_display_fill_region    )(
                                 struct _drv_display_t *const driver_instance, 
                                              uint16_t                x_start,
                                              uint16_t                y_start,
                                              uint16_t                  x_end,
                                              uint16_t                  y_end,
                                              uint16_t                  color);
    wp_display_status_t (*pf_display_draw_line      )(
                                 struct _drv_display_t *const driver_instance,
                                              uint16_t                     x0,
                                              uint16_t                     y0,
                                              uint16_t                     x1,
                                              uint16_t                     y1,
                                              uint16_t                  color);
    wp_display_status_t (*pf_display_draw_rectangle )(
                                 struct _drv_display_t *const driver_instance,
                                              uint16_t                     x0,
                                              uint16_t                     y0,
                                              uint16_t                     x1,
                                              uint16_t                     y1,
                                              uint16_t                  color);
    wp_display_status_t (*pf_display_draw_circle    )(
                                 struct _drv_display_t *const driver_instance,
                                              uint16_t               x_center,
                                              uint16_t               y_center,
                                              uint16_t                 radius,
                                              uint16_t                  color);
    wp_display_status_t (*pf_display_draw_image     )(
                                 struct _drv_display_t *const driver_instance,
                                              uint16_t                x_start,
                                              uint16_t                y_start,
                                              uint16_t                      w,
                                              uint16_t                      h,
                                              uint16_t  const*         bitmap);
    wp_display_status_t (*pf_invert_colors         )(
                                 struct _drv_display_t *const driver_instance,
                                               uint8_t                 invert);
    wp_display_status_t (*pf_display_draw_char      )(
                                 struct _drv_display_t *const driver_instance,
                                              uint16_t                      x,
                                              uint16_t                      y,
                                                  char                     ch,
                                              uint16_t                  color, 
                                              uint16_t               bg_color);
    wp_display_status_t (*pf_display_draw_string    )(
                                 struct _drv_display_t *const driver_instance,
                                              uint16_t                      x,
                                              uint16_t                      y,
                                                  char  const*            str, 
                                              uint16_t                  color, 
                                              uint16_t               bg_color);
    wp_display_status_t (*pf_display_draw_filled_rectangle)(
                                 struct _drv_display_t *const driver_instance,
                                              uint16_t                     x0,
                                              uint16_t                     y0,
                                              uint16_t                     x1,
                                              uint16_t                     y1, 
                                              uint16_t                  color);
    wp_display_status_t (*pf_display_draw_filled_triangle )(
                                 struct _drv_display_t *const driver_instance,
                                              uint16_t                     x0,
                                              uint16_t                     y0,
                                              uint16_t                     x1,
                                              uint16_t                     y1, 
                                              uint16_t                     x2, 
                                              uint16_t                     y2, 
                                              uint16_t                  color);
    wp_display_status_t (*pf_display_draw_filled_circle   )(
                                 struct _drv_display_t *const driver_instance,
                                              uint16_t               x_center,
                                              uint16_t               y_center,
                                              uint16_t                 radius,
                                              uint16_t                  color);

    wp_display_status_t (*pf_display_tear_effect          )(
                                 struct _drv_display_t *const driver_instance,
                                               uint8_t                 enable);
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
bool drv_adapter_display_mount(uint32_t idx, drv_display_t *const drv);

/**
 * @brief Instantiate the currently active display driver.
 *
 * Forwards to pf_display_drv_inst in the registered vtable.  Must run from
 * task context (post-kernel) because the underlying driver inst typically
 * binds OS primitives (bus mutex, os delay) that are only valid after
 * osKernelStart().  Idempotent — implementations should ignore repeated
 * calls after a successful instantiation.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver
 *         is mounted or the slot has no inst hook, other values on driver
 *         failure (logged by the adapter).
 */
wp_display_status_t display_drv_inst(void);

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
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_draw_pixel    (uint16_t x,  uint16_t y,
                                           uint16_t color);

/**
 * @brief Fill the entire screen with a single colour.
 *
 * @param[in] color : 16-bit RGB565 fill colour.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_fill_color    (uint16_t color);

/**
 * @brief Fill a rectangular region with a single colour.
 *
 * @param[in] x0,y0 : Top-left corner.
 * @param[in] x1,y1 : Bottom-right corner.
 * @param[in] color : 16-bit RGB565 fill colour.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_fill_region   (uint16_t x0, uint16_t y0,
                                           uint16_t x1, uint16_t y1,
                                           uint16_t color);

/**
 * @brief Draw a line between two points.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_draw_line     (uint16_t x0, uint16_t y0,
                                           uint16_t x1, uint16_t y1,
                                           uint16_t color);

/**
 * @brief Draw a rectangle outline.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_draw_rectangle(uint16_t x0, uint16_t y0,
                                           uint16_t x1, uint16_t y1,
                                           uint16_t color);

/**
 * @brief Draw a circle outline.
 *
 * @param[in] x,y       : Centre coordinates.
 * @param[in] radius    : Circle radius in pixels.
 * @param[in] color     : 16-bit RGB565 colour.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_draw_circle   (uint16_t x,  uint16_t y,
                                           uint16_t radius,
                                           uint16_t color);

/**
 * @brief Draw a bitmap image at the specified position.
 *
 * @param[in] x0,y0 : Top-left corner.
 * @param[in] w,h   : Image width and height in pixels.
 * @param[in] bitmap: Pointer to 16-bit RGB565 pixel data.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_draw_image    (uint16_t x0, uint16_t y0,
                                           uint16_t  w, uint16_t  h,
                                           uint16_t const* bitmap);

/**
 * @brief Invert or restore display colours.
 *
 * @param[in] invert : true to invert, false to restore.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_invert_colors (bool invert);

/**
 * @brief Draw a single character using the built-in font.
 *
 * @param[in] x,y       : Character top-left position.
 * @param[in] c         : Character to draw.
 * @param[in] color     : Foreground colour (RGB565).
 * @param[in] bg_color  : Background colour (RGB565).
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_draw_char            (uint16_t x, uint16_t y,
                                                      char c,
                                                  uint16_t color,
                                                  uint16_t bg_color);

/**
 * @brief Draw a null-terminated string using the built-in font.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_draw_string          (uint16_t x, uint16_t y,
                                                const char* str,
                                                  uint16_t color,
                                                  uint16_t bg_color);

/**
 * @brief Draw a filled rectangle.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_draw_filled_rectangle(uint16_t x0, uint16_t y0,
                                                  uint16_t x1, uint16_t y1,
                                                  uint16_t color);

/**
 * @brief Draw a filled triangle.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_draw_filled_triangle (uint16_t x0, uint16_t y0,
                                                  uint16_t x1, uint16_t y1,
                                                  uint16_t x2, uint16_t y2,
                                                  uint16_t color);

/**
 * @brief Draw a filled circle.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_draw_filled_circle   (uint16_t x,   uint16_t y,
                                                  uint16_t radius,
                                                  uint16_t color);

/**
 * @brief Enable or disable the display tear-effect (TE) output.
 *
 * @param[in] enable : true to enable, false to disable.
 *
 * @return WP_DISPLAY_OK on success, WP_DISPLAY_ERRORRESOURCE if no driver mounted.
 */
wp_display_status_t display_tear_effect(bool enable);

//******************************* Functions *********************************//

#endif /* __BSP_WRAPPER_DISPLAY_H__ */
