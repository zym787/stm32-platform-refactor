/******************************************************************************
 * @file i2c_port.h
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Define hardware/software I2C port abstraction and bus primitives.
 *
 * Processing flow:
 * Expose core I2C types, status codes, and software-I2C access macros.
 * @version V1.0 2026--
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __I2C_PORT_H__
#define __I2C_PORT_H__

//******************************** Includes *********************************//
#include "main.h"
#include "i2c.h"
#include "iic_hal.h"

#include "osal_mutex.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    CORE_I2C_OK = 0,
    CORE_I2C_ERROR = 1,
    CORE_I2C_BUSY = 2,
    CORE_I2C_TIMEOUT = 3
} core_i2c_status_t;

typedef enum
{
    HARDWARE_I2C = 0,
    SOFTWARE_I2C = 1
} i2c_port_type_t;
    
typedef struct
{
    i2c_port_type_t        core_iic_state;
    iic_bus_t           soft_iic_bus_inst;
#ifdef HAL_I2C_MODULE_ENABLED
    I2C_HandleTypeDef  *  hard_iic_handle;
    osal_mutex_handle_t        os_mutexid;
#endif
} i2c_port_t;

typedef enum
{
    /* Bus index naming follows MCU peripheral numbering for traceability:
     * BUS_1 == MPU6050  bus (currently &hi2c3 in i2c_port.c — kept legacy)
     * BUS_2 == CST816T  bus (&hi2c1) — added 2026-04-26 for touch     */
    CORE_I2C_BUS_1 = 0,
    CORE_I2C_BUS_2,
    CORE_I2C_BUS_MAX
} core_i2c_bus_t;

/* Max time to wait for the bus mutex. Must cover the worst-case transfer
   duration of any peer device on the same bus (400 kHz, ~14 bytes ≈ 0.5 ms),
   plus one FreeRTOS tick of scheduling jitter. */
#define CORE_I2C_BUS_MUTEX_TIMEOUT_MS   8U

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

// I2C port init (initializes mutex for the given bus)
core_i2c_status_t core_i2c_port_init        (core_i2c_bus_t               bus);

#ifdef HAL_I2C_MODULE_ENABLED
// hardware I2C bus primitives
core_i2c_status_t core_hard_i2c_send_byte   (core_i2c_bus_t               bus,
                                                   uint16_t          dev_addr,
                                                   uint8_t          *    data,
                                                   uint16_t              size,
                                                   uint32_t           timeout);
core_i2c_status_t core_hard_i2c_receive_byte(core_i2c_bus_t               bus,
                                                   uint16_t          dev_addr,
                                                   uint8_t          *    data,
                                                   uint16_t              size,
                                                   uint32_t           timeout);
core_i2c_status_t core_hard_i2c_mem_write   (core_i2c_bus_t               bus,
                                                   uint16_t          dev_addr,
                                                   uint16_t          mem_addr,
                                                   uint16_t      mem_add_size,
                                                   uint8_t          *    data,
                                                   uint16_t              size,
                                                   uint32_t           timeout);
core_i2c_status_t core_hard_i2c_mem_read    (core_i2c_bus_t               bus,
                                                   uint16_t          dev_addr,
                                                   uint16_t          mem_addr,
                                                   uint16_t      mem_add_size,
                                                   uint8_t          *    data,
                                                   uint16_t              size,
                                                   uint32_t           timeout);
core_i2c_status_t core_hard_i2c_mem_read_dma(core_i2c_bus_t               bus,
                                                   uint16_t          dev_addr,
                                                   uint16_t          mem_addr,
                                                   uint16_t      mem_add_size,
                                                   uint8_t          *    data,
                                                   uint16_t              size,
                                                   uint32_t           timeout);

#endif /* HAL_I2C_MODULE_ENABLED */

// Software I2C bus primitives
core_i2c_status_t core_soft_i2c_start       (core_i2c_bus_t               bus);
core_i2c_status_t core_soft_i2c_stop        (core_i2c_bus_t               bus);
core_i2c_status_t core_soft_i2c_send_byte   (core_i2c_bus_t               bus, 
                                                    uint8_t              data);
core_i2c_status_t core_soft_i2c_wait_ack    (core_i2c_bus_t               bus);
core_i2c_status_t core_soft_i2c_receive_byte(core_i2c_bus_t               bus, 
                                                    uint8_t             *data);
core_i2c_status_t core_soft_i2c_send_ack    (core_i2c_bus_t               bus);
core_i2c_status_t core_soft_i2c_send_nack   (core_i2c_bus_t               bus);

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

#ifdef HAL_I2C_MODULE_ENABLED
/* Hardware I2C bus primitives (CORE_I2C_BUS_1) */
#define SENSOR_HARDWARE_I2C_SEND_BYTE(dev_addr, data, size, timeout)          \
    core_hard_i2c_send_byte(CORE_I2C_BUS_1, (dev_addr), (data), (size),       \
                            (timeout))

#define SENSOR_HARDWARE_I2C_RECEIVE_BYTE(dev_addr, data, size, timeout)       \
    core_hard_i2c_receive_byte(CORE_I2C_BUS_1, (dev_addr), (data), (size),    \
                               (timeout))

#define SENSOR_HARDWARE_I2C_MEM_WRITE(dev_addr, mem_addr, mem_add_size, data, \
                                      size, timeout)                          \
    core_hard_i2c_mem_write(CORE_I2C_BUS_1, (dev_addr), (mem_addr),           \
                            (mem_add_size), (data), (size), (timeout))

#define SENSOR_HARDWARE_I2C_MEM_READ(dev_addr, mem_addr, mem_add_size, data,  \
                                     size, timeout)                           \
    core_hard_i2c_mem_read(CORE_I2C_BUS_1, (dev_addr), (mem_addr),            \
                           (mem_add_size), (data), (size), (timeout))

#define SENSOR_HARDWARE_I2C_MEM_READ_DMA(dev_addr, mem_addr, mem_add_size,    \
                                         data, size, timeout)                 \
    core_hard_i2c_mem_read_dma(CORE_I2C_BUS_1, (dev_addr), (mem_addr),        \
                               (mem_add_size), (data), (size), (timeout))

/* Hardware I2C bus primitives (CORE_I2C_BUS_2 — touch / CST816T) */
#define TOUCH_HARDWARE_I2C_MEM_WRITE(dev_addr, mem_addr, mem_add_size, data,  \
                                     size, timeout)                           \
    core_hard_i2c_mem_write(CORE_I2C_BUS_2, (dev_addr), (mem_addr),           \
                            (mem_add_size), (data), (size), (timeout))

#define TOUCH_HARDWARE_I2C_MEM_READ(dev_addr, mem_addr, mem_add_size, data,   \
                                    size, timeout)                            \
    core_hard_i2c_mem_read(CORE_I2C_BUS_2, (dev_addr), (mem_addr),            \
                           (mem_add_size), (data), (size), (timeout))
#endif /* HAL_I2C_MODULE_ENABLED */

/* Software I2C bus primitives */
#define SENSOR_SOFTWARE_I2C_START() \
                    core_soft_i2c_start(CORE_I2C_BUS_2);

#define SENSOR_SOFTWARE_I2C_STOP() \
                    core_soft_i2c_stop(CORE_I2C_BUS_2);  

#define SENSOR_SOFTWARE_I2C_SEND_BYTE(data) \
                    core_soft_i2c_send_byte(CORE_I2C_BUS_2, data);

#define SENSOR_SOFTWARE_I2C_WAIT_ACK() \
                    core_soft_i2c_wait_ack(CORE_I2C_BUS_2);

#define SENSOR_SOFTWARE_I2C_RECEIVE_BYTE(data) \
                    core_soft_i2c_receive_byte(CORE_I2C_BUS_2, data);                    

#define SENSOR_SOFTWARE_I2C_SEND_ACK() \
                    core_soft_i2c_send_ack(CORE_I2C_BUS_2);

#define SENSOR_SOFTWARE_I2C_SEND_NACK() \
                    core_soft_i2c_send_nack(CORE_I2C_BUS_2);                    

//******************************* Functions *********************************//

#endif /* __I2C_PORT_H__ */
