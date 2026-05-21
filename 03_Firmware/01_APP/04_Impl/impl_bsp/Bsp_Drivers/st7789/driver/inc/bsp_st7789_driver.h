/******************************************************************************
 * @file bsp_st7789_driver.h
 *
 * @par dependencies
 * - stdint.h
 * - stdbool.h
 *
 * @author Ethan-Hang
 *
 * @brief ST7789 TFT-LCD controller driver interface definitions.
 *
 * Defines the SPI, timebase, and OS interface structures that must be
 * provided by the integration layer, together with the driver instance
 * struct and public API surface.
 *
 * Processing flow:
 *   1. Integration layer populates st7789_driver_input_arg_t.
 *   2. bsp_st7789_driver_inst() binds interfaces and initializes HW.
 *   3. Application draws pixels/shapes/text via vtable function pointers.
 *
 * @version V1.0 2026-04-23
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#ifndef __BSP_ST7789_DRIVER_H__
#define __BSP_ST7789_DRIVER_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>

//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef struct bsp_st7789_driver bsp_st7789_driver_t;

typedef enum
{
    ST7789_OK               = 0,       /* Operation successful               */
    ST7789_ERROR            = 1,       /* General error                      */
    ST7789_ERRORTIMEOUT     = 2,       /* Timeout error                      */
    ST7789_ERRORRESOURCE    = 3,       /* Resource unavailable               */
    ST7789_ERRORPARAMETER   = 4,       /* Invalid parameter                  */
    ST7789_ERRORNOMEMORY    = 5,       /* Out of memory                      */
    ST7789_ERRORUNSUPPORTED = 6,       /* Unsupported feature                */
    ST7789_ERRORISR         = 7,       /* ISR context error                  */
    ST7789_RESERVED         = 0xFF,    /* ST7789 Reserved                    */
} st7789_status_t;

typedef struct
{
    st7789_status_t (*pf_spi_init             )(void);
    st7789_status_t (*pf_spi_deinit           )(void);
    st7789_status_t (*pf_spi_transmit         )( uint8_t const *p_data, 
                                                uint32_t   data_length);
    st7789_status_t (*pf_spi_transmit_dma     )( uint8_t const *p_data, 
                                                uint32_t   data_length);
    st7789_status_t (*pf_spi_wait_dma_complete)(uint32_t    timeout_ms);
    st7789_status_t (*pf_spi_write_cs_pin     )( uint8_t         state);
    st7789_status_t (*pf_spi_write_dc_pin     )( uint8_t         state);
    st7789_status_t (*pf_spi_write_rst_pin    )( uint8_t         state);
} st7789_spi_interface_t;

typedef struct 
{
    uint32_t (*pf_get_tick_ms)(void);
    void     (*pf_delay_ms   )(uint32_t ms);
} st7789_timebase_interface_t;

typedef struct
{
    void (*pf_os_delay_ms)(uint32_t ms);
} st7789_os_interface_t;

typedef struct
{
    uint16_t    width;        /*  Active width  in current orientation (px)  */
    uint16_t   height;        /*  Active height in current orientation (px)  */
    uint16_t x_offset;        /*  CASET offset for the specific panel        */
    uint16_t y_offset;        /*  RASET offset for the specific panel        */
} st7789_panel_config_t;

typedef struct
{
    st7789_panel_config_t        panel;
    st7789_spi_interface_t      *p_spi_interface;
    st7789_timebase_interface_t *p_timebase_interface;
    st7789_os_interface_t       *p_os_interface;
} st7789_driver_input_arg_t;

struct bsp_st7789_driver
{
    /************* Target of Internal Status *************/
    bool                       is_init;

    /************ Panel geometry / orientation ***********/
    st7789_panel_config_t                            panel;
    st7789_spi_interface_t        *        p_spi_interface;
    st7789_timebase_interface_t   *   p_timebase_interface;
    st7789_os_interface_t         *         p_os_interface;

    // basic funcionts
    st7789_status_t (*pf_st7789_init           )(
                                   bsp_st7789_driver_t *const driver_instance);
    st7789_status_t (*pf_st7789_deinit         )(
                                   bsp_st7789_driver_t *const driver_instance);
    st7789_status_t (*pf_st7789_fill_color     )(
                                   bsp_st7789_driver_t *const driver_instance, 
                                              uint16_t                  color);
    st7789_status_t (*pf_st7789_draw_pixel     )(
                                   bsp_st7789_driver_t *const driver_instance, 
                                              uint16_t                      x,
                                              uint16_t                      y,
                                              uint16_t                  color);
    st7789_status_t (*pf_st7789_fill_region    )(
                                   bsp_st7789_driver_t *const driver_instance, 
                                              uint16_t                x_start,
                                              uint16_t                y_start,
                                              uint16_t                  x_end,
                                              uint16_t                  y_end,
                                              uint16_t                  color);
    st7789_status_t (*pf_st7789_draw_pixel_4px )(
                                   bsp_st7789_driver_t *const driver_instance,
                                              uint16_t                      x,
                                              uint16_t                      y,
                                              uint16_t                  color);

    // graphic functions
    st7789_status_t (*pf_st7789_draw_line      )(
                                   bsp_st7789_driver_t *const driver_instance,
                                              uint16_t                     x0,
                                              uint16_t                     y0,
                                              uint16_t                     x1,
                                              uint16_t                     y1,
                                              uint16_t                  color);
    st7789_status_t (*pf_st7789_draw_rectangle )(
                                   bsp_st7789_driver_t *const driver_instance,
                                              uint16_t                     x0,
                                              uint16_t                     y0,
                                              uint16_t                     x1,
                                              uint16_t                     y1,
                                              uint16_t                  color);
    st7789_status_t (*pf_st7789_draw_circle    )(
                                   bsp_st7789_driver_t *const driver_instance,
                                              uint16_t               x_center,
                                              uint16_t               y_center,
                                              uint16_t                 radius,
                                              uint16_t                  color);
    st7789_status_t (*pf_st7789_draw_image     )(
                                   bsp_st7789_driver_t *const driver_instance,
                                              uint16_t                x_start,
                                              uint16_t                y_start,
                                              uint16_t                      w,
                                              uint16_t                      h,
                                              uint16_t  const*         bitmap);
    st7789_status_t (*pf_invert_colors         )(
                                   bsp_st7789_driver_t *const driver_instance,
                                               uint8_t                 invert);

    // text functions
    st7789_status_t (*pf_st7789_draw_char      )(
                                   bsp_st7789_driver_t *const driver_instance,
                                              uint16_t                      x,
                                              uint16_t                      y,
                                                  char                     ch,
                                              uint16_t                  color, 
                                              uint16_t               bg_color);
    st7789_status_t (*pf_st7789_draw_string    )(
                                   bsp_st7789_driver_t *const driver_instance,
                                              uint16_t                      x,
                                              uint16_t                      y,
                                                  char  const*            str, 
                                              uint16_t                  color, 
                                              uint16_t               bg_color);

    // extended functions
    st7789_status_t (*pf_st7789_draw_filled_rectangle)(
                                   bsp_st7789_driver_t *const driver_instance,
                                              uint16_t                     x0,
                                              uint16_t                     y0,
                                              uint16_t                     x1,
                                              uint16_t                     y1, 
                                              uint16_t                  color);
    st7789_status_t (*pf_st7789_draw_filled_triangle )(
                                   bsp_st7789_driver_t *const driver_instance,
                                              uint16_t                     x0,
                                              uint16_t                     y0,
                                              uint16_t                     x1,
                                              uint16_t                     y1, 
                                              uint16_t                     x2, 
                                              uint16_t                     y2, 
                                              uint16_t                  color);
    st7789_status_t (*pf_st7789_draw_filled_circle   )(
                                   bsp_st7789_driver_t *const driver_instance,
                                              uint16_t               x_center,
                                              uint16_t               y_center,
                                              uint16_t                 radius,
                                              uint16_t                  color);

    st7789_status_t (*pf_st7789_tear_effect          )(
                                   bsp_st7789_driver_t *const driver_instance,
                                               uint8_t                 enable);
};

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief  Assemble a st7789 driver instance with caller-supplied interfaces
 *         and panel geometry.
 *
 * @param[out] driver_instance       Driver instance to populate.
 * @param[in]  p_spi_interface       Raw SPI / CS / DC / RST / DMA-wait vtable.
 * @param[in]  p_spi_driver_interface  ST7789 SPI framing vtable
 *                                     (write_cmd / write_data wrappers).
 * @param[in]  p_timebase_interface  ms tick / busy-wait delay vtable.
 * @param[in]  p_os_interface        OS-aware delay vtable (nullable in no-OS).
 * @param[in]  p_panel               Panel geometry config (width/height/offsets).
 *
 * @return ST7789_OK on success, error code otherwise.
 */
st7789_status_t bsp_st7789_driver_inst(
                                   bsp_st7789_driver_t * const driver_instance,
                                st7789_spi_interface_t *       p_spi_interface,
                           st7789_timebase_interface_t *  p_timebase_interface,
                                 st7789_os_interface_t *        p_os_interface,
                                 st7789_panel_config_t const *         p_panel
                                        );
//******************************* Functions *********************************//

#endif /* __BSP_ST7789_DRIVER_H__ */
