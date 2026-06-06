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
#include "platform_error.h"
#include "platform_type.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* This header is MCU-agnostic: the hardware/software port descriptor and all
 * HAL handle types live in the implementation (i2c_port.c).  Callers see only
 * the logical bus index and the platform_err_t API below. */
typedef enum
{
    /* Bus index naming follows MCU peripheral numbering for traceability:
     * BUS_1 == MPU6050  bus (currently &hi2c3 in i2c_port.c — kept legacy)
     * BUS_2 == CST816T  bus (&hi2c1) — added 2026-04-26 for touch     */
    MCU_I2C_BUS_1 = 0,
    MCU_I2C_BUS_2,
    MCU_I2C_BUS_MAX
} mcu_i2c_bus_t;

/* Max time to wait for the bus mutex. Must cover the worst-case transfer
   duration of any peer device on the same bus (400 kHz, ~14 bytes ≈ 0.5 ms),
   plus one FreeRTOS tick of scheduling jitter. */
#define MCU_I2C_BUS_MUTEX_TIMEOUT_MS   8U

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

// I2C port init (initializes mutex for the given bus)
platform_err_t mcu_i2c_port_init           (mcu_i2c_bus_t               bus);

// hardware I2C bus primitives
platform_err_t mcu_hard_i2c_send_byte      (mcu_i2c_bus_t               bus,
                                                   uint16_t          dev_addr,
                                                   uint8_t          *    data,
                                                   uint16_t              size,
                                                   uint32_t           timeout);
platform_err_t mcu_hard_i2c_receive_byte   (mcu_i2c_bus_t               bus,
                                                   uint16_t          dev_addr,
                                                   uint8_t          *    data,
                                                   uint16_t              size,
                                                   uint32_t           timeout);
platform_err_t mcu_hard_i2c_mem_write      (mcu_i2c_bus_t               bus,
                                                   uint16_t          dev_addr,
                                                   uint16_t          mem_addr,
                                                   uint16_t      mem_add_size,
                                                   uint8_t          *    data,
                                                   uint16_t              size,
                                                   uint32_t           timeout);
platform_err_t mcu_hard_i2c_mem_read       (mcu_i2c_bus_t               bus,
                                                   uint16_t          dev_addr,
                                                   uint16_t          mem_addr,
                                                   uint16_t      mem_add_size,
                                                   uint8_t          *    data,
                                                   uint16_t              size,
                                                   uint32_t           timeout);
platform_err_t mcu_hard_i2c_mem_read_dma   (mcu_i2c_bus_t               bus,
                                                   uint16_t          dev_addr,
                                                   uint16_t          mem_addr,
                                                   uint16_t      mem_add_size,
                                                   uint8_t          *    data,
                                                   uint16_t              size,
                                                   uint32_t           timeout);

// Software I2C bus primitives
platform_err_t mcu_soft_i2c_start          (mcu_i2c_bus_t               bus);
platform_err_t mcu_soft_i2c_stop           (mcu_i2c_bus_t               bus);
platform_err_t mcu_soft_i2c_send_byte      (mcu_i2c_bus_t               bus, 
                                                    uint8_t              data);
platform_err_t mcu_soft_i2c_wait_ack       (mcu_i2c_bus_t               bus);
platform_err_t mcu_soft_i2c_receive_byte   (mcu_i2c_bus_t               bus, 
                                                    uint8_t             *data);
platform_err_t mcu_soft_i2c_send_ack       (mcu_i2c_bus_t               bus);
platform_err_t mcu_soft_i2c_send_nack      (mcu_i2c_bus_t               bus);

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/* Hardware I2C bus primitives (MCU_I2C_BUS_1) */
#define SENSOR_HARDWARE_I2C_SEND_BYTE(dev_addr, data, size, timeout)          \
    mcu_hard_i2c_send_byte(MCU_I2C_BUS_1, (dev_addr), (data), (size),       \
                            (timeout))

#define SENSOR_HARDWARE_I2C_RECEIVE_BYTE(dev_addr, data, size, timeout)       \
    mcu_hard_i2c_receive_byte(MCU_I2C_BUS_1, (dev_addr), (data), (size),    \
                               (timeout))

#define SENSOR_HARDWARE_I2C_MEM_WRITE(dev_addr, mem_addr, mem_add_size, data, \
                                      size, timeout)                          \
    mcu_hard_i2c_mem_write(MCU_I2C_BUS_1, (dev_addr), (mem_addr),           \
                            (mem_add_size), (data), (size), (timeout))

#define SENSOR_HARDWARE_I2C_MEM_READ(dev_addr, mem_addr, mem_add_size, data,  \
                                     size, timeout)                           \
    mcu_hard_i2c_mem_read(MCU_I2C_BUS_1, (dev_addr), (mem_addr),            \
                           (mem_add_size), (data), (size), (timeout))

#define SENSOR_HARDWARE_I2C_MEM_READ_DMA(dev_addr, mem_addr, mem_add_size,    \
                                         data, size, timeout)                 \
    mcu_hard_i2c_mem_read_dma(MCU_I2C_BUS_1, (dev_addr), (mem_addr),        \
                               (mem_add_size), (data), (size), (timeout))

/* Hardware I2C bus primitives (MCU_I2C_BUS_2 — touch / CST816T) */
#define TOUCH_HARDWARE_I2C_MEM_WRITE(dev_addr, mem_addr, mem_add_size, data,  \
                                     size, timeout)                           \
    mcu_hard_i2c_mem_write(MCU_I2C_BUS_2, (dev_addr), (mem_addr),           \
                            (mem_add_size), (data), (size), (timeout))

#define TOUCH_HARDWARE_I2C_MEM_READ(dev_addr, mem_addr, mem_add_size, data,   \
                                    size, timeout)                            \
    mcu_hard_i2c_mem_read(MCU_I2C_BUS_2, (dev_addr), (mem_addr),            \
                           (mem_add_size), (data), (size), (timeout))

/* Software I2C bus primitives */
#define SENSOR_SOFTWARE_I2C_START() \
                    mcu_soft_i2c_start(MCU_I2C_BUS_2);

#define SENSOR_SOFTWARE_I2C_STOP() \
                    mcu_soft_i2c_stop(MCU_I2C_BUS_2);  

#define SENSOR_SOFTWARE_I2C_SEND_BYTE(data) \
                    mcu_soft_i2c_send_byte(MCU_I2C_BUS_2, data);

#define SENSOR_SOFTWARE_I2C_WAIT_ACK() \
                    mcu_soft_i2c_wait_ack(MCU_I2C_BUS_2);

#define SENSOR_SOFTWARE_I2C_RECEIVE_BYTE(data) \
                    mcu_soft_i2c_receive_byte(MCU_I2C_BUS_2, data);                    

#define SENSOR_SOFTWARE_I2C_SEND_ACK() \
                    mcu_soft_i2c_send_ack(MCU_I2C_BUS_2);

#define SENSOR_SOFTWARE_I2C_SEND_NACK() \
                    mcu_soft_i2c_send_nack(MCU_I2C_BUS_2);                    

//******************************* Functions *********************************//

#endif /* __I2C_PORT_H__ */
