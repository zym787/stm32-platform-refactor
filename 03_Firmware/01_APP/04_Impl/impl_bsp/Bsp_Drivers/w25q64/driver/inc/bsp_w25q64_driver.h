/******************************************************************************
 * @file
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief
 *
 * Processing flow:
 *
 *
 * @version V1.0 2026--
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_W25Q64_DRIVER_H__
#define __BSP_W25Q64_DRIVER_H__

//******************************** Includes *********************************//
#include <stdint.h>

//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    W25Q64_OK               = 0,       /* Operation successful               */
    W25Q64_ERROR            = 1,       /* General error                      */
    W25Q64_ERRORTIMEOUT     = 2,       /* Timeout error                      */
    W25Q64_ERRORRESOURCE    = 3,       /* Resource unavailable               */
    W25Q64_ERRORPARAMETER   = 4,       /* Invalid parameter                  */
    W25Q64_ERRORNOMEMORY    = 5,       /* Out of memory                      */
    W25Q64_ERRORUNSUPPORTED = 6,       /* Unsupported feature                */
    W25Q64_ERRORISR         = 7,       /* ISR context error                  */
    W25Q64_BUSY             = 8,       /* Device is busy                     */
    W25Q64_SLEEP            = 9,       /* Device is in sleep mode            */
    W25Q64_WAKEUP           = 10,      /* Device is waking up from sleep     */
    W25Q64_RESERVED         = 0xFF,    /* W25Q64 Reserved                    */
} w25q64_status_t;

typedef struct
{
    w25q64_status_t (*pf_spi_init             )(void);
    w25q64_status_t (*pf_spi_deinit           )(void);
    w25q64_status_t (*pf_spi_transmit         )( uint8_t const *p_data, 
                                                uint32_t   data_length);
    w25q64_status_t (*pf_spi_read             )( uint8_t     *p_buffer, 
                                                uint32_t buffer_length);
    w25q64_status_t (*pf_spi_transmit_dma     )( uint8_t const *p_data, 
                                                uint32_t   data_length);
    w25q64_status_t (*pf_spi_wait_dma_complete)(uint32_t    timeout_ms);
    w25q64_status_t (*pf_spi_write_cs_pin     )(uint8_t         state);
    w25q64_status_t (*pf_spi_write_dc_pin     )( uint8_t         state);
} w25q64_spi_interface_t;

typedef struct 
{
    uint32_t (*pf_get_tick_ms)(void);
    void     (*pf_delay_ms   )(uint32_t ms);
} w25q64_timebase_interface_t;

typedef struct
{
    void (*pf_os_delay_ms)(uint32_t ms);
} w25q64_os_delay_t;

typedef struct bsp_w25q64_driver bsp_w25q64_driver_t;
struct bsp_w25q64_driver
{
    bsp_w25q64_driver_t           *      p_driver_instance;

    w25q64_spi_interface_t        *        p_spi_interface;
    w25q64_timebase_interface_t   *   p_timebase_interface;
    w25q64_os_delay_t             *         p_os_interface;

    w25q64_status_t (*pf_w25q64_init)(
                                   bsp_w25q64_driver_t *const driver_instance);
    w25q64_status_t (*pf_w25q64_deinit)(
                                   bsp_w25q64_driver_t *const driver_instance);
    w25q64_status_t (*pf_w25q64_read_id)(
                                   bsp_w25q64_driver_t *const driver_instance,
                                   uint8_t             *          p_id_buffer,
                                   uint32_t                     buffer_length);
    w25q64_status_t (*pf_w25q64_read_data)(
                                   bsp_w25q64_driver_t *const driver_instance, 
                                   uint32_t                           address,
                                   uint8_t             *        p_data_buffer, 
                                   uint32_t                     buffer_length);
    w25q64_status_t (*pf_w25q64_write_data_noerase)(
                                   bsp_w25q64_driver_t *const driver_instance, 
                                   uint32_t                           address,
                                   uint8_t              const*  p_data_buffer, 
                                   uint32_t                       data_length);
    w25q64_status_t (*pf_w25q64_write_data_erase)(
                                   bsp_w25q64_driver_t *const driver_instance, 
                                   uint32_t                           address,
                                   uint8_t              const*  p_data_buffer, 
                                   uint32_t                       data_length);
    w25q64_status_t (*pf_w25q64_erase_chip)(
                                   bsp_w25q64_driver_t *const driver_instance);
    w25q64_status_t (*pf_w25q64_erase_sector)(
                                   bsp_w25q64_driver_t *const driver_instance,
                                   uint32_t                           address);
    w25q64_status_t (*pf_w25q64_sleep)(
                                   bsp_w25q64_driver_t *const driver_instance);
    w25q64_status_t (*pf_w25q64_wakeup)(
                                   bsp_w25q64_driver_t *const driver_instance);
};
//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

w25q64_status_t w25q64_driver_inst(
                     bsp_w25q64_driver_t         * const        p_w25q64_inst,
                     w25q64_spi_interface_t      * const      p_spi_interface,
                     w25q64_timebase_interface_t * const p_timebase_interface,
                     w25q64_os_delay_t           * const       p_os_interface);

//******************************* Functions *********************************//

#endif /*__BSP_W25Q64_DRIVER_H__ */
