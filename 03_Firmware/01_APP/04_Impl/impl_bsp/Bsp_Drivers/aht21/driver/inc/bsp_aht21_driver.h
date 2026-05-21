/******************************************************************************
 * @file bsp_aht21_driver.h
 *
 * @par dependencies
 * - stdio.h
 * - stdint.h
 *
 * @author  Ethan-Hang
 *
 * @brief  AHT21 Temperature and Humidity Sensor Driver Header File.
 * This file provides the interface definitions and data structures
 * for the AHT21 sensor driver, including:
 * - IIC communication interface (Hardware/Software selectable)
 * - Timebase interface for delay operations
 * - RTOS yield interface for non-blocking operations
 * - Sensor operation APIs (init, read, sleep, wakeup)
 *
 * Processing flow:
 * 1. Create driver instance using aht21_inst()
 * 2. Initialize sensor with pf_init()
 * 3. Read temperature/humidity with pf_read_temp_humi()
 * 4. Optionally use pf_sleep()/pf_wakeup() for power management
 *
 * @version V1.0 2025-11-25
 *
 * @note    1 tab == 4 spaces!
 *
 *****************************************************************************/

#ifndef __BSP_AHT21_DRIVER_H__
#define __BSP_AHT21_DRIVER_H__

//******************************** Includes *********************************//
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "bsp_aht21_reg.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define OS_SUPPORTING       (1)
#define USE_HARDWARE_I2C    (1)

/*   Return values from functions    */
typedef enum
{
    AHT21_OK               = 0,       /* Operation successful                */
    AHT21_ERROR            = 1,       /* General error                       */
    AHT21_ERRORTIMEOUT     = 2,       /* Timeout error                       */
    AHT21_ERRORRESOURCE    = 3,       /* Resource unavailable                */
    AHT21_ERRORPARAMETER   = 4,       /* Invalid parameter                   */
    AHT21_ERRORNOMEMORY    = 5,       /* Out of memory                       */
    AHT21_ERRORUNSUPPORTED = 6,       /* Unsupported feature                 */
    AHT21_ERRORISR         = 7,       /* ISR context error                   */
    AHT21_RESERVED         = 0xFF,    /* AHT21 Reserved                      */
} aht21_status_t;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

/*     From Core Layer: IIC Port     */
#if USE_HARDWARE_I2C /*   HARDWARE IIC   */
typedef struct
{
    aht21_status_t (*pf_iic_init        ) (void *);/* IIC init     interface */
    aht21_status_t (*pf_iic_deinit      ) (void *);/* IIC deinit   interface */
    aht21_status_t (*pf_i2c_master_write) (void *,
                                           uint16_t       dev_addr,
                                           uint8_t           *data,
                                           uint16_t           size);
    aht21_status_t (*pf_i2c_master_read ) (void *,
                                           uint16_t       dev_addr,
                                           uint8_t           *data,
                                           uint16_t           size);

} aht21_iic_driver_interface_t;
#else            /*   SOFTWARE IIC   */
typedef struct
{
    aht21_status_t (*pf_iic_init        )( void *);/* IIC init     interface */
    aht21_status_t (*pf_iic_deinit      )( void *);/* IIC deinit   interface */
    aht21_status_t (*pf_iic_start       )( void *);/* IIC start    interface */
    aht21_status_t (*pf_iic_stop        )( void *);/* IIC stop     interface */
    aht21_status_t (*pf_iic_wait_ack    )( void *);/* IIC w-ack    interface */
    aht21_status_t (*pf_iic_send_ack    )( void *);/* IIC s-ack    interface */
    aht21_status_t (*pf_iic_send_no_ack )( void *);/* IIC s-n-ack  interface */
                                                   /* IIC s-byte   interface */
    aht21_status_t (*pf_iic_send_byte   )( void *,
                                           uint8_t   const addr );
                                                   /* IIC r-byte   interface */
    aht21_status_t (*pf_iic_receive_byte)( void *,
                                           uint8_t * const data );
                                                    
#if OS_SUPPORTING                                          
    aht21_status_t (*pf_critical_enter  )(void);  /* Enter critical section */
    aht21_status_t (*pf_critical_exit   )(void);  /* Exit  critical section */
#endif // OS_SUPPORTING

} aht21_iic_driver_interface_t;

#endif // USE_HARDWARE_I2C

/*     From Core Layer: TimeBase     */
typedef struct
{
    uint32_t (*pf_get_tick_count_ms) (void);       /*   delay ms interface   */
} aht21_timebase_interface_t;

#if OS_SUPPORTING
/*      From OS Layer: TimeBase      */
typedef struct
{
    void (*pf_rtos_yield)(uint32_t const);         /* OS None-Blocking Delay */
} aht21_yield_interface_t;
#endif // OS_SUPPORTING

// typedef struct 
// {
// #if OS_SUPPORTING
//     aht21_status_t (*pf_lock       ) (void);       /* Add    Thread Lock     */
//     aht21_status_t (*pf_unlock     ) (void);       /* Remove Thread Lock     */
// #endif // OS_SUPPORTING

//     aht21_status_t (*pf_disable_irq) (void);       /* Disable IRQs           */
//     aht21_status_t (*pf_enable_irq ) (void);       /* Enable  IRQs           */
// } irq_interface_t;

/************ Aht21_hal_driver instance class ************/
typedef struct bsp_aht21_driver
{
    /************* Target of Internal Status *************/
    bool                                     aht21_is_init;

    /*********** The interface from core layer ***********/
    aht21_iic_driver_interface_t  *  p_iic_driver_instance;
    aht21_timebase_interface_t    *    p_timebase_instance;

    /************ The interface from OS layer ************/
#if OS_SUPPORTING
    aht21_yield_interface_t       *       p_yield_instance;
#endif // OS_SUPPORTING

    /****************** Target of APIs *******************/
    aht21_status_t (*pf_init          ) (struct bsp_aht21_driver  *const);
    aht21_status_t (*pf_deinit        ) (struct bsp_aht21_driver  *const);
    aht21_status_t (*pf_read_id       ) (
                                         struct bsp_aht21_driver  *const ,
                                                        uint32_t  *const
                                        );
    aht21_status_t (*pf_read_temp_humi) (
                                         struct bsp_aht21_driver  *const , 
                                                            float *const temp,
                                                            float *const humi
                                        );
    aht21_status_t (*pf_read_humi     ) (
                                         struct bsp_aht21_driver  *const , 
                                                            float *const
                                        );
    aht21_status_t (*pf_read_temp     ) (
                                         struct bsp_aht21_driver  *const , 
                                                            float *const
                                        );
    aht21_status_t (*pf_sleep         ) (struct bsp_aht21_driver  *const);
    aht21_status_t (*pf_wakeup        ) (struct bsp_aht21_driver  *const);


} bsp_aht21_driver_t;

/*     Aht21_hal_driver instance class inst function     */
/**
 * @brief Initialize an AHT21 driver instance
 *
 * This function initializes an existing AHT21 driver instance and sets up
 * the driver's function pointers for sensor operations using the provided
 * interface implementations.
 *
 * @param[out] p_aht21_inst         Pointer to AHT21 driver instance
 * @param[in]  p_iic_driver_inst    Pointer to IIC driver interface
 * @param[in]  p_yield_inst         Pointer to RTOS yield interface
 *                                      (if OS_SUPPORTING)
 * @param[in]  p_timebase_inst      Pointer to timebase interface
 * @param[in]  p_irq_instance       Pointer to IRQ interface
 *
 * @return AHT21_OK on success, error code on failure:
 *         - AHT21_ERRORPARAMETER: Invalid parameter
 *         - AHT21_ERRORNOMEMORY: Memory allocation failed
 *         - AHT21_ERROR: General initialization error
 *
 * @note Call this to populate the instance before calling the instance's
 *       `pf_init()` method to complete sensor initialization.
 */
aht21_status_t aht21_inst(
        bsp_aht21_driver_t            *const        p_aht21_inst,
        aht21_iic_driver_interface_t  *const   p_iic_driver_inst,
#if OS_SUPPORTING
        aht21_yield_interface_t       *const        p_yield_inst,
#endif // OS_SUPPORTING
        aht21_timebase_interface_t    *const     p_timebase_inst
                         );

//******************************* Declaring *********************************//

#endif // __BSP_AHT21_DRIVER_H__
