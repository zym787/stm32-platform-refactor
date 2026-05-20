/******************************************************************************
 * @file    i2c_port.c
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief  MCU-level I2C port layer.
 *         Wraps both hardware I2C (HAL) and software I2C (iic_hal) behind a
 *         unified core_i2c_status_t interface.
 *
 * Processing flow:
 *   Select bus by core_i2c_bus_t index; dispatch to hard or soft driver.
 *
 * @version V1.0 2026-4-12
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "i2c_port.h"
#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static i2c_port_t i2c_port[CORE_I2C_BUS_MAX] = {
    /* BUS_1: MPU6050 motion sensor on I2C3.  Index kept for legacy. */
    [CORE_I2C_BUS_1] = {.core_iic_state  = HARDWARE_I2C,
                        .hard_iic_handle = &hi2c3,
                        .os_mutexid      = NULL},

    /* BUS_2: CST816T touch controller on I2C1.  Added 2026-04-26. */
    [CORE_I2C_BUS_2] = {.core_iic_state  = HARDWARE_I2C,
                        .hard_iic_handle = &hi2c1,
                        .os_mutexid      = NULL},
};
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
static core_i2c_status_t core_hard_i2c_bus_lock(core_i2c_bus_t bus,
                                         uint32_t       timeout_ms)
{
    if (bus >= CORE_I2C_BUS_MAX)
    {
        return CORE_I2C_ERROR;
    }

    if (osal_mutex_take(i2c_port[bus].os_mutexid,
                        (osal_tick_type_t)timeout_ms) != 0)
    {
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "BusLock mutex timeout bus=%d timeout=%u", (int)bus,
                  (unsigned int)timeout_ms);
        return CORE_I2C_TIMEOUT;
    }
    return CORE_I2C_OK;
}

static core_i2c_status_t core_hard_i2c_bus_unlock(core_i2c_bus_t bus)
{
    if (bus >= CORE_I2C_BUS_MAX)
    {
        return CORE_I2C_ERROR;
    }

    osal_mutex_give(i2c_port[bus].os_mutexid);
    return CORE_I2C_OK;
}

core_i2c_status_t core_i2c_port_init(core_i2c_bus_t bus)
{
    if (bus >= CORE_I2C_BUS_MAX)
    {
        return CORE_I2C_ERROR;
    }

    if (i2c_port[bus].core_iic_state != HARDWARE_I2C)
    {
        return CORE_I2C_OK;
    }

    int32_t ret = osal_mutex_init(&i2c_port[bus].os_mutexid);
    return (ret == 0) ? CORE_I2C_OK : CORE_I2C_ERROR;
}

core_i2c_status_t core_hard_i2c_send_byte(core_i2c_bus_t bus, uint16_t dev_addr,
                                          uint8_t *data, uint16_t size,
                                          uint32_t timeout)
{
    if (bus >= CORE_I2C_BUS_MAX || NULL == data)
    {
        return CORE_I2C_ERROR;
    }

    core_i2c_status_t lock_ret =
        core_hard_i2c_bus_lock(bus, CORE_I2C_BUS_MUTEX_TIMEOUT_MS);
    if (lock_ret != CORE_I2C_OK)
    {
        return lock_ret;
    }

    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        i2c_port[bus].hard_iic_handle, dev_addr, data, size, timeout);

    core_hard_i2c_bus_unlock(bus);

    if (ret != HAL_OK)
    {
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "Transmit fail bus=%d dev=0x%02X halerrcode=0x%08X",
                  (int)bus, dev_addr, (int)ret);
        return CORE_I2C_ERROR;
    }
    return CORE_I2C_OK;
}

core_i2c_status_t core_hard_i2c_receive_byte(core_i2c_bus_t bus,
                                             uint16_t dev_addr, uint8_t *data,
                                             uint16_t size, uint32_t timeout)
{
    if (bus >= CORE_I2C_BUS_MAX || NULL == data)
    {
        return CORE_I2C_ERROR;
    }

    core_i2c_status_t lock_ret =
        core_hard_i2c_bus_lock(bus, CORE_I2C_BUS_MUTEX_TIMEOUT_MS);
    if (lock_ret != CORE_I2C_OK)
    {
        return lock_ret;
    }

    HAL_StatusTypeDef ret = HAL_I2C_Master_Receive(
        i2c_port[bus].hard_iic_handle, dev_addr, data, size, timeout);

    core_hard_i2c_bus_unlock(bus);

    if (ret != HAL_OK)
    {
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "Receive fail bus=%d dev=0x%02X halerrcode=0x%08X",
                  (int)bus, dev_addr, (int)ret);
        return CORE_I2C_ERROR;
    }
    return CORE_I2C_OK;
}

core_i2c_status_t core_hard_i2c_mem_write(core_i2c_bus_t bus, uint16_t dev_addr,
                                          uint16_t mem_addr,
                                          uint16_t mem_add_size, uint8_t *data,
                                          uint16_t size, uint32_t timeout)
{
    if (bus >= CORE_I2C_BUS_MAX || NULL == data)
    {
        return CORE_I2C_ERROR;
    }

    core_i2c_status_t lock_ret =
        core_hard_i2c_bus_lock(bus, CORE_I2C_BUS_MUTEX_TIMEOUT_MS);
    if (lock_ret != CORE_I2C_OK)
    {
        return lock_ret;
    }

    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Write(i2c_port[bus].hard_iic_handle, dev_addr, mem_addr,
                          mem_add_size, data, size, timeout);

    core_hard_i2c_bus_unlock(bus);

    if (ret != HAL_OK)
    {
        DEBUG_OUT(
            e, HAL_IIC_ERR_LOG_TAG,
            "MemWrite fail bus=%d dev=0x%02X mem=0x%04X halerrcode=%d",
            (int)bus, dev_addr, mem_addr, (int)ret);
        return CORE_I2C_ERROR;
    }
    return CORE_I2C_OK;
}

core_i2c_status_t core_hard_i2c_mem_read(core_i2c_bus_t bus, uint16_t dev_addr,
                                         uint16_t mem_addr,
                                         uint16_t mem_add_size, uint8_t *data,
                                         uint16_t size, uint32_t timeout)
{
    if (bus >= CORE_I2C_BUS_MAX || NULL == data)
    {
        return CORE_I2C_ERROR;
    }

    core_i2c_status_t lock_ret =
        core_hard_i2c_bus_lock(bus, CORE_I2C_BUS_MUTEX_TIMEOUT_MS);
    if (lock_ret != CORE_I2C_OK)
    {
        return lock_ret;
    }

    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Read(i2c_port[bus].hard_iic_handle, dev_addr, mem_addr,
                         mem_add_size, data, size, timeout);

    core_hard_i2c_bus_unlock(bus);

    if (ret != HAL_OK)
    {
        DEBUG_OUT(
            e, HAL_IIC_ERR_LOG_TAG,
            "MemRead fail bus=%d dev=0x%02X mem=0x%04X halerrcode=%d",
            (int)bus, dev_addr, mem_addr, (int)ret);
        return CORE_I2C_ERROR;
    }
    return CORE_I2C_OK;
}

static core_i2c_status_t core_hard_i2c_wait_dma_done(core_i2c_bus_t bus,
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
            return CORE_I2C_TIMEOUT;
        }
    }

    if (HAL_I2C_GetError(i2c_port[bus].hard_iic_handle) != HAL_I2C_ERROR_NONE)
    {
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "MemReadDMA done with error bus=%d err=0x%lX",
                  (int)bus,
                  (unsigned long)HAL_I2C_GetError(i2c_port[bus].hard_iic_handle));
        return CORE_I2C_ERROR;
    }

    return CORE_I2C_OK;
}

core_i2c_status_t core_hard_i2c_mem_read_dma(core_i2c_bus_t bus,
                                             uint16_t       dev_addr,
                                             uint16_t       mem_addr,
                                             uint16_t       mem_add_size,
                                             uint8_t *data, uint16_t size,
                                             uint32_t timeout)
{
    if (bus >= CORE_I2C_BUS_MAX || NULL == data)
    {
        return CORE_I2C_ERROR;
    }

    /* Serialize DMA transfer with all peer users on the same bus. */
    core_i2c_status_t lock_ret =
        core_hard_i2c_bus_lock(bus, CORE_I2C_BUS_MUTEX_TIMEOUT_MS);
    if (lock_ret != CORE_I2C_OK)
    {
        return lock_ret;
    }

    HAL_StatusTypeDef ret =
        HAL_I2C_Mem_Read_DMA(i2c_port[bus].hard_iic_handle, dev_addr, mem_addr,
                             mem_add_size, data, size);

    if (ret != HAL_OK)
    {
        core_hard_i2c_bus_unlock(bus);
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "MemReadDMA fail bus=%d dev=0x%02X mem=0x%04X halerrcode=%d",
                  (int)bus, dev_addr, mem_addr, (int)ret);
        return CORE_I2C_ERROR;
    }

    core_i2c_status_t wait_ret = core_hard_i2c_wait_dma_done(bus, timeout);
    core_hard_i2c_bus_unlock(bus);
    return wait_ret;
}

core_i2c_status_t core_soft_i2c_start(core_i2c_bus_t bus)
{
    if (bus >= CORE_I2C_BUS_MAX)
    {
        return CORE_I2C_ERROR;
    }

    i2c_start(&i2c_port[bus].soft_iic_bus_inst);
    return CORE_I2C_OK;
}

core_i2c_status_t core_soft_i2c_stop(core_i2c_bus_t bus)
{
    if (bus >= CORE_I2C_BUS_MAX)
    {
        return CORE_I2C_ERROR;
    }

    i2c_stop(&i2c_port[bus].soft_iic_bus_inst);
    return CORE_I2C_OK;
}

core_i2c_status_t core_soft_i2c_send_byte(core_i2c_bus_t bus, uint8_t data)
{
    if (bus >= CORE_I2C_BUS_MAX)
    {
        return CORE_I2C_ERROR;
    }

    i2c_send_byte(&i2c_port[bus].soft_iic_bus_inst, data);
    return CORE_I2C_OK;
}

core_i2c_status_t core_soft_i2c_wait_ack(core_i2c_bus_t bus)
{
    if (bus >= CORE_I2C_BUS_MAX)
    {
        return CORE_I2C_ERROR;
    }

    iic_hal_status_t ack = i2c_wait_ack(&i2c_port[bus].soft_iic_bus_inst);
    if (IIC_HAL_OK == ack)
    {
        return CORE_I2C_OK;
    }
    return CORE_I2C_ERROR;
}

core_i2c_status_t core_soft_i2c_receive_byte(core_i2c_bus_t bus, uint8_t *data)
{
    if (bus >= CORE_I2C_BUS_MAX || NULL == data)
    {
        return CORE_I2C_ERROR;
    }

    *data = i2c_receive_byte(&i2c_port[bus].soft_iic_bus_inst);
    return CORE_I2C_OK;
}

core_i2c_status_t core_soft_i2c_send_ack(core_i2c_bus_t bus)
{
    if (bus >= CORE_I2C_BUS_MAX)
    {
        return CORE_I2C_ERROR;
    }

    i2c_send_ack(&i2c_port[bus].soft_iic_bus_inst);
    return CORE_I2C_OK;
}

core_i2c_status_t core_soft_i2c_send_nack(core_i2c_bus_t bus)
{
    if (bus >= CORE_I2C_BUS_MAX)
    {
        return CORE_I2C_ERROR;
    }

    i2c_send_not_ack(&i2c_port[bus].soft_iic_bus_inst);
    return CORE_I2C_OK;
}

//******************************* Functions *********************************//
