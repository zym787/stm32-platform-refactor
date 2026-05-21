/******************************************************************************
 * @file bsp_mpuxxxx_driver.h
 *
 * @par dependencies
 * - stdint.h
 * - stdbool.h
 * - string.h
 * 
 * - circular_buffer.h
 *
 * - bsp_mpu6050_reg.h
 * - bsp_mpu6050_reg_bit.h
 * 
 * - Debug.h
 * 
 * @author Ethan-Hang
 *
 * @brief Define MPUXXXX driver interfaces, data structures, and API surface.
 *
 * Processing flow:
 * Collect HAL/OS adapters and expose unified MPUXXXX driver operations.
 * @version V1.0 2025-1-6
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once

#ifndef __BSP_MPUXXXX_DRIVER_H__
#define __BSP_MPUXXXX_DRIVER_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "circular_buffer.h"

#include "bsp_mpu6050_reg.h"
#include "bsp_mpu6050_reg_bit.h"

#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define OS_SUPPORTING                (1)

typedef enum
{
    MPUXXXX_OK                      = 0,    /* Operation successful          */
    MPUXXXX_ERROR                   = 1,    /* General error                 */
    MPUXXXX_ERRORTIMEOUT            = 2,    /* Timeout error                 */
    MPUXXXX_ERRORRESOURCE           = 3,    /* Resource unavailable          */
    MPUXXXX_ERRORPARAMETER          = 4,    /* Invalid parameter             */
    MPUXXXX_ERRORNOMEMORY           = 5,    /* Out of memory                 */
    MPUXXXX_ERRORUNSUPPORTED        = 6,    /* Unsupported feature           */
    MPUXXXX_ERRORISR                = 7,    /* ISR context error             */
    MPUXXXX_RESERVED                = 0xFF, /* MPUXXXX Reserved              */
} mpuxxxx_status_t;

/*********************** Core Layer **********************/
/*      From HAL Layer: IIC Port     */
typedef struct 
{
    /*       hi2c pointer to a i2c handle structure      */
    void                     *                        hi2c;

    /*         i2c init and deinit interfaces            */
    mpuxxxx_status_t (*pf_iic_init        )               \
                                      (void const * const);
    mpuxxxx_status_t (*pf_iic_deinit      )               \
                                      (void const * const);

    /**/
    mpuxxxx_status_t (*pf_iic_mem_write   ) (void    *i2c,
                                        uint16_t des_addr,
                                        uint16_t mem_addr,
                                        uint16_t mem_size,
                                        uint8_t   *p_data,
                                        uint16_t     size,
                                        uint32_t  timeout);
    mpuxxxx_status_t (*pf_iic_mem_read    ) (void    *i2c,
                                        uint16_t des_addr,
                                        uint16_t mem_addr,
                                        uint16_t mem_size,
                                        uint8_t   *p_data,
                                        uint16_t     size,
                                        uint32_t  timeout);                              
    mpuxxxx_status_t (*pf_iic_mem_read_dma) (void    *i2c,
                                        uint16_t des_addr,
                                        uint16_t mem_addr,
                                        uint16_t mem_size,
                                        uint8_t   *p_data,
                                        uint16_t     size);
} iic_driver_interface_t;

/*       Interrupt Of MPUXXXX        */
typedef struct
{
    mpuxxxx_status_t (*pf_irq_init         )        (void);
    mpuxxxx_status_t (*pf_irq_deinit       )        (void);
    mpuxxxx_status_t (*pf_irq_enable       )        (void);
    mpuxxxx_status_t (*pf_irq_disable      )        (void);
    mpuxxxx_status_t (*pf_irq_clear_pending)        (void);
    mpuxxxx_status_t (*pf_irq_enable_clock )        (void);
    mpuxxxx_status_t (*pf_irq_disable_clock)        (void);
} hardware_interrupt_interface_t;

/*        Timebase Interface         */
typedef struct
{
    uint32_t         (*pf_get_tick_count   )        (void);
} timebase_interface_t;

/*        Delay Interface            */
typedef struct
{
    void             (*pf_delay_init       )        (void);
    void             (*pf_delay_ms         ) (uint32_t \
                                                 const ms);
    void             (*pf_delay_us         ) (uint32_t \
                                                 const us);
} delay_interface_t;

/************************ Os Layer ***********************/
#if OS_SUPPORTING
/*        Yield Interface            */
typedef struct
{
    void             (*pf_rtos_yield       ) (uint32_t \
                                                 const ms);
} yield_interface_t;

/*          Os Interface             */
typedef struct
{
    /*       os queue interface      */
    mpuxxxx_status_t (*pf_os_queue_create    ) (uint32_t const   queue_length,
                                                uint32_t const     queue_size,
                                                void  ** const  queue_handler);
    mpuxxxx_status_t (*pf_os_queue_send      ) (void  *  const  queue_handler,
                                                void  *  const       item_ptr,
                                                uint32_t const      wait_time);
    mpuxxxx_status_t (*pf_os_queue_receive   ) (void  *  const  queue_handler,
                                                void  *  const       item_ptr,
                                                uint32_t const      wait_time);
    mpuxxxx_status_t (*pf_os_queue_send_isr  ) (void  *  const  queue_handler, 
                                                void  *  const       item_ptr, 
                                                long  *  const \
                                                      HigherPriorityTaskWoken);
    mpuxxxx_status_t (*pf_os_queue_delete    ) (void  *  const  queue_handler);

    /* os semaphore mutex interface  */
    mpuxxxx_status_t (*pf_os_mutex_create    ) (void  ** const  mutex_handler);
    mpuxxxx_status_t (*pf_os_mutex_lock      ) (void  *  const  mutex_handler,
                                                uint32_t const      wait_time);
    mpuxxxx_status_t (*pf_os_mutex_unlock    ) (void  *  const  mutex_handler);
    mpuxxxx_status_t (*pf_os_mutex_delete    ) (void  *  const  mutex_handler);

    /* os semaphore binary interface  */
    mpuxxxx_status_t (*pf_os_semaphore_create) (void  ** const binary_handler);
    mpuxxxx_status_t (*pf_os_semaphore_take  ) (void  *  const binary_handler);
    mpuxxxx_status_t (*pf_os_semaphore_take_isr)\
                                               (void  *  const binary_handler,
                                                long  *  const \
                                                      HigherPriorityTaskWoken);
    mpuxxxx_status_t (*pf_os_semaphore_wait_notify)\
                                               (uint32_t ulBitsToClearOnEntry, 
                                                uint32_t  ulBitsToClearOnExit, 
                                                uint32_t*pulNotificationValue,
                                                uint32_t              timeout);                                                      
    mpuxxxx_status_t (*pf_os_semaephore_notify_isr)\
                                               (void  *  const binary_handler,
                                                uint32_t const        ulValue,
                                                uint32_t const        eAction,
                                                long  *  const \
                                                      HigherPriorityTaskWoken);
    mpuxxxx_status_t (*pf_os_semaphore_give  ) (void  *  const binary_handler);
    mpuxxxx_status_t (*pf_os_semaphore_delete) (void  *  const binary_handler);

    void *           (*pf_os_get_task_handle )                          (void);
} os_interface_t;
#endif // OS_SUPPORTING

/*      Data Structure of MPUXXXX    */
typedef struct 
{
    /*     raw accelerometer data    */
    int16_t                accel_x_raw;
    int16_t                accel_y_raw;
    int16_t                accel_z_raw;
    
    /*  processed accelerometer data */
    double                   accel_x_g;
    double                   accel_y_g;
    double                   accel_z_g;

    /*      raw gyroscope data       */
    int16_t                 gyro_x_raw;
    int16_t                 gyro_y_raw;
    int16_t                 gyro_z_raw;

    /*    processed gyroscope data   */
    double                  gyro_x_dps;
    double                  gyro_y_dps;
    double                  gyro_z_dps;

    /*     processed temperature     */
    float                temperature_c;

    /* kalman filter processed angles*/
    double              kalman_angle_x;
    double              kalman_angle_y;
} mpuxxxx_data_t;

/*  Store Data From MPUXXXX Driver   */
typedef struct
{
    uint8_t *        (*pf_buffer_init      )(uint8_t size);
    uint8_t *        (*pf_get_rbuffer_addr )        (void);
    uint8_t *        (*pf_get_wbuffer_addr )        (void);
} buffer_interface_t;

typedef struct bsp_mpuxxxx_driver
{
    /*      Driver Private Data     */
    bool                                                          private_data;

    /*          core layer          */
    iic_driver_interface_t              const  *         p_iic_driver_instance;
    hardware_interrupt_interface_t      const  *          p_interrupt_instance;
    timebase_interface_t                const  *           p_timebase_instance;
    delay_interface_t                   const  *              p_delay_instance;

    /*          os layer            */
#if OS_SUPPORTING
    yield_interface_t                   const  *              p_yield_instance;
    os_interface_t                      const  *                 p_os_instance;
    buffer_interface_t                  const  *             p_buffer_instance;
    void                                const  *                 queue_hanlder;
    void                                const  *                 mutex_handler;
    void                                const  *             semaphore_handler;
    void                                const  *                notify_handler;
#endif // OS_SUPPORTING

    /*       callback functions      */
    void (*pf_dma_completed_callback)                                   (void);
    void (*pf_int_interrupt_callback)                                   (void);

    /*  interface of mpuxxxx driver  */
    mpuxxxx_status_t (*pf_deinit               ) (struct bsp_mpuxxxx_driver   \
                                                                const * const);
    mpuxxxx_status_t (*pf_sleep                ) (struct bsp_mpuxxxx_driver   \
                                                                const * const);
    mpuxxxx_status_t (*pf_wakeup               ) (struct bsp_mpuxxxx_driver   \
                                                                const * const);
    mpuxxxx_status_t (*pf_set_gyro_fsr         ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                                      uint8_t);
    mpuxxxx_status_t (*pf_set_accel_fsr        ) (struct bsp_mpuxxxx_driver   \
                                                                const * const,
                                                                      uint8_t);
    mpuxxxx_status_t (*pf_set_lpf              ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                                      uint8_t);
    mpuxxxx_status_t (*pf_set_rate             ) (struct bsp_mpuxxxx_driver   \
                                                                const * const,
                                                                      uint8_t);
    mpuxxxx_status_t (*pf_set_interrupt_enable ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                                      uint8_t);
    mpuxxxx_status_t (*pf_set_motion_threshold ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                                      uint8_t);
    mpuxxxx_status_t (*pf_set_INT_level        ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                                      uint8_t);
    mpuxxxx_status_t (*pf_set_user_ctrl        ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                                      uint8_t);
    mpuxxxx_status_t (*pf_set_pwr_mgmt1_reg    ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                                      uint8_t);
    mpuxxxx_status_t (*pf_set_pwr_mgmt2_reg    ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                                      uint8_t);
    mpuxxxx_status_t (*pf_set_fifo_en_reg      ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                                      uint8_t);    
    mpuxxxx_status_t (*pf_get_temperature      ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                       mpuxxxx_data_t * const);
    mpuxxxx_status_t (*pf_get_accel            ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                       mpuxxxx_data_t * const);
    mpuxxxx_status_t (*pf_get_gyro             ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                       mpuxxxx_data_t * const);
    mpuxxxx_status_t (*pf_get_all_data         ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                       mpuxxxx_data_t * const);
    mpuxxxx_status_t (*pf_get_interrupt_status_reg)                           \
                                                 (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                              uint8_t * const);
    mpuxxxx_status_t (*pf_read_fifo_packet     ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                       mpuxxxx_data_t * const);
    mpuxxxx_status_t (*pf_read_fifo_isr_occur  ) (struct bsp_mpuxxxx_driver   \
                                                                const * const, 
                                                       mpuxxxx_data_t * const);                         

} bsp_mpuxxxx_driver_t;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
mpuxxxx_status_t bsp_mpuxxxx_driver_inst(
           bsp_mpuxxxx_driver_t                 * const       p_mpuxxxx_driver,

           iic_driver_interface_t         const * const p_iic_driver_interface,
           hardware_interrupt_interface_t const * const  p_interrupt_interface,
           timebase_interface_t           const * const   p_timebase_interface,
           delay_interface_t              const * const      p_delay_interface,

#if OS_SUPPORTING
           yield_interface_t              const * const      p_yield_interface,
           os_interface_t                 const * const         p_os_interface,
#endif // OS_SUPPORTING
           void (*callback_register    )
                           (void (*callback)(void const * const, void* const)),
           void (*callback_register_dma)
                           (void (*callback)(void const * const, void* const))
);

//******************************* Declaring *********************************//

#endif // end of __BSP_MPUXXXX_DRIVER_H__
