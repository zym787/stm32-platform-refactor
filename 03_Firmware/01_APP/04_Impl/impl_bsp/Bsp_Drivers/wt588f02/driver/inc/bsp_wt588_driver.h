/******************************************************************************
 * @file bsp_wt588_driver.h
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Define WT588 low-level driver interfaces and control APIs.
 *
 *
 *
 * @version V1.0 2026-4-6
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_WT588_DRIVER_H__
#define __BSP_WT588_DRIVER_H__
//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>

#include "wt588_cmd.h"

#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    WT588_OK               = 0,       /* Operation successful                */
    WT588_ERROR            = 1,       /* General error                       */
    WT588_ERRORTIMEOUT     = 2,       /* Timeout error                       */
    WT588_ERRORRESOURCE    = 3,       /* Resource unavailable                */
    WT588_ERRORPARAMETER   = 4,       /* Invalid parameter                   */
    WT588_ERRORNOMEMORY    = 5,       /* Out of memory                       */
    WT588_ERRORUNSUPPORTED = 6,       /* Unsupported feature                 */
    WT588_ERRORISR         = 7,       /* ISR context error                   */
    WT588_RESERVED         = 0xFF,    /* WT588 Reserved                      */
} wt588_status_t;

typedef struct 
{
    wt588_status_t (*pf_wt_gpio_init     )(void);
    void           (*pf_wt_gpio_deinit   )(void);
} wt_gpio_interface_t;

typedef struct 
{
    void (*pf_delay_ms  )(uint32_t ms);
} wt_sys_interface_t;

typedef struct 
{
    wt588_status_t (*pf_pwm_dma_init       )(void        );
    void           (*pf_pwm_dma_deinit     )(void        );
    wt588_status_t (*pf_pwm_dma_send_byte  )(uint8_t data);
} wt_pwm_dma_interface_t;

typedef struct
{
    bool (*pf_is_busy)(void);
} wt_busy_interface_t;

typedef struct bsp_wt588_driver
{
    wt_gpio_interface_t         *         p_gpio_interface;
    wt_sys_interface_t          *          p_sys_interface;
    wt_pwm_dma_interface_t      *      p_pwm_dma_interface;
    wt_busy_interface_t         *         p_busy_interface;

    wt588_status_t (*pf_start_play     )(struct bsp_wt588_driver *,  uint8_t );
    wt588_status_t (*pf_stop_play      )(struct bsp_wt588_driver *);
    wt588_status_t (*pf_set_volume     )(struct bsp_wt588_driver *,  uint8_t );
    bool           (*pf_is_busy        )(struct bsp_wt588_driver *);
} bsp_wt588_driver_t;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief Initialize WT588 driver instance
 *
 * @param[in] p_wt588_inst        : Pointer to WT588 driver instance
 * @param[in] p_sys_interface     : Pointer to system delay interface
 * @param[in] p_busy_interface    : Pointer to busy detection interface
 * @param[in] p_gpio_interface    : Pointer to GPIO control interface
 * @param[in] p_pwm_dma_interface : Pointer to PWM DMA interface
 *
 * @return wt588_status_t WT588_OK if successful, error code otherwise
 *
 * */
wt588_status_t wt588_driver_inst(
                          bsp_wt588_driver_t      * const        p_wt588_inst,
                          wt_sys_interface_t      * const     p_sys_interface,
                         wt_busy_interface_t      * const    p_busy_interface,
                         wt_gpio_interface_t      * const    p_gpio_interface,
                      wt_pwm_dma_interface_t      * const p_pwm_dma_interface);

//******************************* Functions *********************************//


#endif /* __BSP_WT588_DRIVER_H__ */
