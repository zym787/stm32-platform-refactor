/******************************************************************************
 * @file    i2c_port.c
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief  MCU-level I2C port layer.
 *         Wraps both hardware I2C (HAL) and software I2C (iic_hal) behind a
 *         unified platform_err_t interface.
 *
 * Processing flow:
 *   Select bus by mcu_i2c_bus_t index; dispatch to hard or soft driver.
 *
 * @version V1.0 2026-4-12
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "i2c_port.h"
#include "Debug.h"

/* HAL handles, soft-I2C driver and the OSAL mutex type live here in the
 * implementation, not in the public port header, so upper layers including
 * i2c_port.h stay MCU-agnostic. */
#include "main.h"
#include "i2c.h"
#include "iic_hal.h"
#include "osal_mutex.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* Hardware vs software I2C selector for each bus descriptor. */
typedef enum
{
    MCU_I2C_HARDWARE = 0,
    MCU_I2C_SOFTWARE = 1
} i2c_port_type_t;

/* Per-bus descriptor: binds a logical mcu_i2c_bus_t to its HAL handle +
 * mutex (hardware) or bit-bang instance (software). */
typedef struct
{
    i2c_port_type_t      mcu_iic_state;
    iic_bus_t            soft_iic_bus_inst;
#ifdef HAL_I2C_MODULE_ENABLED
    I2C_HandleTypeDef   *hard_iic_handle;
    osal_mutex_handle_t  os_mutexid;
#endif
} i2c_port_t;
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static i2c_port_t i2c_port[MCU_I2C_BUS_MAX] = {
    /* BUS_1: MPU6050 motion sensor on I2C3.  Index kept for legacy. */
    [MCU_I2C_BUS_1] = {.mcu_iic_state  = MCU_I2C_HARDWARE,
                        .hard_iic_handle = &hi2c3,
                        .os_mutexid      = NULL},

    /* BUS_2: CST816T touch controller on I2C1.  Added 2026-04-26. */
    [MCU_I2C_BUS_2] = {.mcu_iic_state  = MCU_I2C_HARDWARE,
                        .hard_iic_handle = &hi2c1,
                        .os_mutexid      = NULL},
};
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
static platform_err_t mcu_hard_i2c_bus_lock(mcu_i2c_bus_t bus,
                                         uint32_t       timeout_ms)
{
    if (bus >= MCU_I2C_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    if (osal_mutex_take(i2c_port[bus].os_mutexid,
                        (osal_tick_type_t)timeout_ms) != 0)
    {
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "BusLock mutex timeout bus=%d timeout=%u", (int)bus,
                  (unsigned int)timeout_ms);
        return PLATFORM_ERR_TIMEOUT;
    }
    return PLATFORM_OK;
}

static platform_err_t mcu_hard_i2c_bus_unlock(mcu_i2c_bus_t bus)
{
    if (bus >= MCU_I2C_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    osal_mutex_give(i2c_port[bus].os_mutexid);
    return PLATFORM_OK;
}

platform_err_t mcu_i2c_port_init(mcu_i2c_bus_t bus)
{
    if (bus >= MCU_I2C_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    if (i2c_port[bus].mcu_iic_state != MCU_I2C_HARDWARE)
    {
        return PLATFORM_OK;
    }

    int32_t ret = osal_mutex_init(&i2c_port[bus].os_mutexid);
    return (ret == 0) ? PLATFORM_OK : PLATFORM_ERR_GENERAL;
}

platform_err_t mcu_hard_i2c_send_byte(mcu_i2c_bus_t bus, uint16_t dev_addr,
                                          uint8_t *data, uint16_t size,
                                          uint32_t timeout)
{
    if (bus >= MCU_I2C_BUS_MAX || NULL == data)
    {
        return PLATFORM_ERR_PARAM;
    }

    platform_err_t lock_ret =
        mcu_hard_i2c_bus_lock(bus, MCU_I2C_BUS_MUTEX_TIMEOUT_MS);
    if (lock_ret != PLATFORM_OK)
    {
        return lock_ret;
    }

    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        i2c_port[bus].hard_iic_handle, dev_addr, data, size, timeout);

    mcu_hard_i2c_bus_unlock(bus);

    if (ret != HAL_OK)
    {
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "Transmit fail bus=%d dev=0x%02X halerrcode=0x%08X",
                  (int)bus, dev_addr, (int)ret);
        return PLATFORM_ERR_GENERAL;
    }
    return PLATFORM_OK;
}

platform_err_t mcu_hard_i2c_receive_byte(mcu_i2c_bus_t bus,
                                             uint16_t dev_addr, uint8_t *data,
                                             uint16_t size, uint32_t timeout)
{
    if (bus >= MCU_I2C_BUS_MAX || NULL == data)
    {
        return PLATFORM_ERR_PARAM;
    }

    platform_err_t lock_ret =
        mcu_hard_i2c_bus_lock(bus, MCU_I2C_BUS_MUTEX_TIMEOUT_MS);
    if (lock_ret != PLATFORM_OK)
    {
        return lock_ret;
    }

    HAL_StatusTypeDef ret = HAL_I2C_Master_Receive(
        i2c_port[bus].hard_iic_handle, dev_addr, data, size, timeout);

    mcu_hard_i2c_bus_unlock(bus);

    if (ret != HAL_OK)
    {
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "Receive fail bus=%d dev=0x%02X halerrcode=0x%08X",
                  (int)bus, dev_addr, (int)ret);
        return PLATFORM_ERR_GENERAL;
    }
    return PLATFORM_OK;
}

platform_err_t mcu_hard_i2c_mem_write(mcu_i2c_bus_t bus, uint16_t dev_addr,
                                          uint16_t mem_addr,
                                          uint16_t mem_add_size, uint8_t *data,
                                          uint16_t size, uint32_t timeout)
{
    if (bus >= MCU_I2C_BUS_MAX || NULL == data)
    {
        return PLATFORM_ERR_PARAM;
    }

    platform_err_t lock_ret =
        mcu_hard_i2c_bus_lock(bus, MCU_I2C_BUS_MUTEX_TIMEOUT_MS);
    if (lock_ret != PLATFORM_OK)
    {
        return lock_ret;
    }

    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Write(i2c_port[bus].hard_iic_handle, dev_addr, mem_addr,
                          mem_add_size, data, size, timeout);

    mcu_hard_i2c_bus_unlock(bus);

    if (ret != HAL_OK)
    {
        DEBUG_OUT(
            e, HAL_IIC_ERR_LOG_TAG,
            "MemWrite fail bus=%d dev=0x%02X mem=0x%04X halerrcode=%d",
            (int)bus, dev_addr, mem_addr, (int)ret);
        return PLATFORM_ERR_GENERAL;
    }
    return PLATFORM_OK;
}

platform_err_t mcu_hard_i2c_mem_read(mcu_i2c_bus_t bus, uint16_t dev_addr,
                                         uint16_t mem_addr,
                                         uint16_t mem_add_size, uint8_t *data,
                                         uint16_t size, uint32_t timeout)
{
    if (bus >= MCU_I2C_BUS_MAX || NULL == data)
    {
        return PLATFORM_ERR_PARAM;
    }

    platform_err_t lock_ret =
        mcu_hard_i2c_bus_lock(bus, MCU_I2C_BUS_MUTEX_TIMEOUT_MS);
    if (lock_ret != PLATFORM_OK)
    {
        return lock_ret;
    }

    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Read(i2c_port[bus].hard_iic_handle, dev_addr, mem_addr,
                         mem_add_size, data, size, timeout);

    mcu_hard_i2c_bus_unlock(bus);

    if (ret != HAL_OK)
    {
        DEBUG_OUT(
            e, HAL_IIC_ERR_LOG_TAG,
            "MemRead fail bus=%d dev=0x%02X mem=0x%04X halerrcode=%d",
            (int)bus, dev_addr, mem_addr, (int)ret);
        return PLATFORM_ERR_GENERAL;
    }
    return PLATFORM_OK;
}

static platform_err_t mcu_hard_i2c_wait_dma_done(mcu_i2c_bus_t bus,
                                                      uint32_t timeout)
{
    uint32_t start_tick = HAL_GetTick();

    while (HAL_I2C_GetState(i2c_port[bus].hard_iic_handle) != HAL_I2C_STATE_READY)
    {
        if ((HAL_GetTick() - start_tick) >= timeout)
        {
            DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                      "MemReadDMA wait timeout bus=%d timeout=%u state=0x%lX err=0x%lX",
                      (int)bus, (unsigned int)timeout,
                      (unsigned long)HAL_I2C_GetState(i2c_port[bus].hard_iic_handle),
                      (unsigned long)HAL_I2C_GetError(i2c_port[bus].hard_iic_handle));
            return PLATFORM_ERR_TIMEOUT;
        }
    }

    if (HAL_I2C_GetError(i2c_port[bus].hard_iic_handle) != HAL_I2C_ERROR_NONE)
    {
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "MemReadDMA done with error bus=%d err=0x%lX",
                  (int)bus,
                  (unsigned long)HAL_I2C_GetError(i2c_port[bus].hard_iic_handle));
        return PLATFORM_ERR_GENERAL;
    }

    return PLATFORM_OK;
}

platform_err_t mcu_hard_i2c_mem_read_dma(mcu_i2c_bus_t bus,
                                             uint16_t       dev_addr,
                                             uint16_t       mem_addr,
                                             uint16_t       mem_add_size,
                                             uint8_t *data, uint16_t size,
                                             uint32_t timeout)
{
    if (bus >= MCU_I2C_BUS_MAX || NULL == data)
    {
        return PLATFORM_ERR_PARAM;
    }

    /* Serialize DMA transfer with all peer users on the same bus. */
    platform_err_t lock_ret =
        mcu_hard_i2c_bus_lock(bus, MCU_I2C_BUS_MUTEX_TIMEOUT_MS);
    if (lock_ret != PLATFORM_OK)
    {
        return lock_ret;
    }

    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Read_DMA(i2c_port[bus].hard_iic_handle, dev_addr, mem_addr,
                             mem_add_size, data, size);

    if (ret != HAL_OK)
    {
        mcu_hard_i2c_bus_unlock(bus);
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "MemReadDMA fail bus=%d dev=0x%02X mem=0x%04X halerrcode=%d",
                  (int)bus, dev_addr, mem_addr, (int)ret);
        return PLATFORM_ERR_GENERAL;
    }

    platform_err_t wait_ret = mcu_hard_i2c_wait_dma_done(bus, timeout);
    mcu_hard_i2c_bus_unlock(bus);
    return wait_ret;
}

platform_err_t mcu_soft_i2c_start(mcu_i2c_bus_t bus)
{
    if (bus >= MCU_I2C_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    i2c_start(&i2c_port[bus].soft_iic_bus_inst);
    return PLATFORM_OK;
}

platform_err_t mcu_soft_i2c_stop(mcu_i2c_bus_t bus)
{
    if (bus >= MCU_I2C_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    i2c_stop(&i2c_port[bus].soft_iic_bus_inst);
    return PLATFORM_OK;
}

platform_err_t mcu_soft_i2c_send_byte(mcu_i2c_bus_t bus, uint8_t data)
{
    if (bus >= MCU_I2C_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    i2c_send_byte(&i2c_port[bus].soft_iic_bus_inst, data);
    return PLATFORM_OK;
}

platform_err_t mcu_soft_i2c_wait_ack(mcu_i2c_bus_t bus)
{
    if (bus >= MCU_I2C_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    iic_hal_status_t ack = i2c_wait_ack(&i2c_port[bus].soft_iic_bus_inst);
    if (IIC_HAL_OK == ack)
    {
        return PLATFORM_OK;
    }
    return PLATFORM_ERR_GENERAL;
}

platform_err_t mcu_soft_i2c_receive_byte(mcu_i2c_bus_t bus, uint8_t *data)
{
    if (bus >= MCU_I2C_BUS_MAX || NULL == data)
    {
        return PLATFORM_ERR_PARAM;
    }

    *data = i2c_receive_byte(&i2c_port[bus].soft_iic_bus_inst);
    return PLATFORM_OK;
}

platform_err_t mcu_soft_i2c_send_ack(mcu_i2c_bus_t bus)
{
    if (bus >= MCU_I2C_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    i2c_send_ack(&i2c_port[bus].soft_iic_bus_inst);
    return PLATFORM_OK;
}

platform_err_t mcu_soft_i2c_send_nack(mcu_i2c_bus_t bus)
{
    if (bus >= MCU_I2C_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    i2c_send_not_ack(&i2c_port[bus].soft_iic_bus_inst);
    return PLATFORM_OK;
}

//******************************* Functions *********************************//
