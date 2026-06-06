/******************************************************************************
 * @file    spi_port.c
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief  MCU-level SPI port layer.
 *         Wraps both hardware SPI (HAL) and software SPI (spi_hal) behind a
 *         unified platform_err_t interface.
 *
 * Processing flow:
 *   Select bus by mcu_spi_bus_t index; dispatch to hard or soft driver.
 *
 * @version V1.0 2026-4-15
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "board_types.h"
#include "spi_port.h"

/* HAL handles, CS pin defines, soft-SPI driver and the OSAL mutex type live
 * here in the implementation, not in the public port header, so upper layers
 * including spi_port.h stay MCU-agnostic. */
#include "main.h"
#include "spi.h"
#include "spi_hal.h"
#include "osal_mutex.h"
#include "osal_error.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* Hardware vs software SPI selector for each bus descriptor. */
typedef enum
{
    MCU_SPI_HARDWARE = 0,
    MCU_SPI_SOFTWARE = 1
} spi_port_type_t;

/* Per-bus descriptor: binds a logical mcu_spi_bus_t to its HAL handle /
 * CS pin / mutex (hardware) or bit-bang instance (software). */
typedef struct
{
    spi_port_type_t      mcu_spi_state;
    spi_bus_t            soft_spi_bus_inst;
#ifdef HAL_SPI_MODULE_ENABLED
    SPI_HandleTypeDef   *hard_spi_handle;
    GPIO_TypeDef        *hard_spi_cs_port;
    UINT16_t             hard_spi_cs_pin;
    osal_mutex_handle_t  os_mutexid;
#endif
} spi_port_t;
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static spi_port_t spi_port[MCU_SPI_BUS_MAX] =
{
    /* BUS_1: ST7789 display on SPI1 (hardware).  CS / DC / RST GPIOs are
     * still driven directly from the integration layer for now (HAL GPIO
     * abstraction is deferred); the SPI bus itself routes through the
     * MCU port mutex + HAL_SPI_Transmit / HAL_SPI_Transmit_DMA. */
    [MCU_SPI_BUS_1] = {
        .mcu_spi_state   = MCU_SPI_HARDWARE,
        .hard_spi_handle  = &hspi1,
        .hard_spi_cs_port = NULL,
        .hard_spi_cs_pin  = 0u,
        .os_mutexid       = NULL,
    },

    /* BUS_2: W25Q64 NOR flash on SPI2 (hardware).  CS line (PB13) is
     * exclusive to the flash, so it is registered here and driven
     * through mcu_hard_spi_cs_select / mcu_hard_spi_cs_deselect
     * rather than from the integration layer.  Pin macros come from
     * CubeMX (Core/Inc/main.h, FLASH_SPI2_CS_*). */
    [MCU_SPI_BUS_2] = {
        .mcu_spi_state   = MCU_SPI_HARDWARE,
        .hard_spi_handle  = &hspi2,
        .hard_spi_cs_port = FLASH_SPI2_CS_GPIO_Port,
        .hard_spi_cs_pin  = FLASH_SPI2_CS_Pin,
        .os_mutexid       = NULL,
    },
};
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
platform_err_t mcu_spi_port_init(mcu_spi_bus_t bus)
{
    if (bus >= MCU_SPI_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    if (spi_port[bus].mcu_spi_state != MCU_SPI_HARDWARE)
    {
        spi_init(&spi_port[bus].soft_spi_bus_inst);
        return PLATFORM_OK;
    }

#ifdef HAL_SPI_MODULE_ENABLED
    /* OSAL_SUCCESS == OSAL_FALSE == 0, so compare to OSAL_SUCCESS by name
       rather than bare 0 (see [[osal-success-vs-false-collision]]). */
    INT32_t ret = osal_mutex_init(&spi_port[bus].os_mutexid);
    return (OSAL_SUCCESS == ret) ? PLATFORM_OK : PLATFORM_ERR_GENERAL;
#else
    return PLATFORM_ERR_GENERAL;
#endif
}

#ifdef HAL_SPI_MODULE_ENABLED
/* ---------- Hardware SPI CS control ---------- */

platform_err_t mcu_hard_spi_cs_select(mcu_spi_bus_t bus)
{
    if (bus >= MCU_SPI_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    HAL_GPIO_WritePin(spi_port[bus].hard_spi_cs_port,
                      spi_port[bus].hard_spi_cs_pin,
                      GPIO_PIN_RESET);
    return PLATFORM_OK;
}

platform_err_t mcu_hard_spi_cs_deselect(mcu_spi_bus_t bus)
{
    if (bus >= MCU_SPI_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    HAL_GPIO_WritePin(spi_port[bus].hard_spi_cs_port,
                      spi_port[bus].hard_spi_cs_pin,
                      GPIO_PIN_SET);
    return PLATFORM_OK;
}

/* ---------- Hardware SPI bus primitives ---------- */

platform_err_t mcu_hard_spi_transmit(mcu_spi_bus_t bus,
                                              UINT8_t *data,
                                              UINT16_t  size,
                                              UINT32_t  timeout)
{
    if (bus >= MCU_SPI_BUS_MAX || NULL == data)
    {
        return PLATFORM_ERR_PARAM;
    }

    if (osal_mutex_take(spi_port[bus].os_mutexid,
                        (osal_tick_type_t)timeout) != OSAL_SUCCESS)
    {
        return PLATFORM_ERR_TIMEOUT;
    }

    HAL_StatusTypeDef ret = HAL_SPI_Transmit(
        spi_port[bus].hard_spi_handle, data, size, timeout);

    osal_mutex_give(spi_port[bus].os_mutexid);

    return (ret == HAL_OK) ? PLATFORM_OK : PLATFORM_ERR_GENERAL;
}

platform_err_t mcu_hard_spi_receive(mcu_spi_bus_t bus,
                                             UINT8_t *data,
                                             UINT16_t  size,
                                             UINT32_t  timeout)
{
    if (bus >= MCU_SPI_BUS_MAX || NULL == data)
    {
        return PLATFORM_ERR_PARAM;
    }

    if (osal_mutex_take(spi_port[bus].os_mutexid,
                        (osal_tick_type_t)timeout) != OSAL_SUCCESS)
    {
        return PLATFORM_ERR_TIMEOUT;
    }

    HAL_StatusTypeDef ret = HAL_SPI_Receive(
        spi_port[bus].hard_spi_handle, data, size, timeout);

    osal_mutex_give(spi_port[bus].os_mutexid);

    return (ret == HAL_OK) ? PLATFORM_OK : PLATFORM_ERR_GENERAL;
}

platform_err_t mcu_hard_spi_transmit_receive(mcu_spi_bus_t bus,
                                                       UINT8_t *tx_data,
                                                       UINT8_t *rx_data,
                                                       UINT16_t   size,
                                                       UINT32_t timeout)
{
    if (bus >= MCU_SPI_BUS_MAX || NULL == tx_data || NULL == rx_data)
    {
        return PLATFORM_ERR_PARAM;
    }

    if (osal_mutex_take(spi_port[bus].os_mutexid,
                        (osal_tick_type_t)timeout) != OSAL_SUCCESS)
    {
        return PLATFORM_ERR_TIMEOUT;
    }

    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(
        spi_port[bus].hard_spi_handle, tx_data, rx_data, size, timeout);

    osal_mutex_give(spi_port[bus].os_mutexid);

    return (ret == HAL_OK) ? PLATFORM_OK : PLATFORM_ERR_GENERAL;
}

platform_err_t mcu_hard_spi_transmit_dma(mcu_spi_bus_t bus,
                                                   UINT8_t *data,
                                                   UINT16_t  size)
{
    if (bus >= MCU_SPI_BUS_MAX || NULL == data)
    {
        return PLATFORM_ERR_PARAM;
    }

    /* DMA transfer is non-blocking; take mutex here, caller releases via
       mcu_hard_spi_dma_complete() or the DMA-complete callback. */
    if (osal_mutex_take(spi_port[bus].os_mutexid, OSAL_MAX_DELAY) !=
        OSAL_SUCCESS)
    {
        return PLATFORM_ERR_TIMEOUT;
    }

    HAL_StatusTypeDef ret = HAL_SPI_Transmit_DMA(
        spi_port[bus].hard_spi_handle, data, size);

    if (ret != HAL_OK)
    {
        osal_mutex_give(spi_port[bus].os_mutexid);
        return PLATFORM_ERR_GENERAL;
    }

    return PLATFORM_OK;
}

platform_err_t mcu_hard_spi_receive_dma(mcu_spi_bus_t bus,
                                                  UINT8_t *data,
                                                  UINT16_t  size)
{
    if (bus >= MCU_SPI_BUS_MAX || NULL == data)
    {
        return PLATFORM_ERR_PARAM;
    }

    if (osal_mutex_take(spi_port[bus].os_mutexid, OSAL_MAX_DELAY) !=
        OSAL_SUCCESS)
    {
        return PLATFORM_ERR_TIMEOUT;
    }

    HAL_StatusTypeDef ret = HAL_SPI_Receive_DMA(
        spi_port[bus].hard_spi_handle, data, size);

    if (ret != HAL_OK)
    {
        osal_mutex_give(spi_port[bus].os_mutexid);
        return PLATFORM_ERR_GENERAL;
    }

    return PLATFORM_OK;
}

void mcu_hard_spi_dma_complete(mcu_spi_bus_t bus)
{
    if (bus >= MCU_SPI_BUS_MAX)
    {
        return;
    }
    osal_mutex_give(spi_port[bus].os_mutexid);
}

/**
 * @brief  Block-poll the SPI peripheral until it returns to READY, then
 *         release the bus mutex taken by the matching transmit_dma /
 *         receive_dma call.
 *
 * @param[in] bus     : Bus index.
 * @param[in] timeout : Max wait in ms (uses HAL tick).
 *
 * @return PLATFORM_OK on completion, PLATFORM_ERR_TIMEOUT if the peripheral
 *         did not idle in time (mutex is still released so the bus can
 *         recover), PLATFORM_ERR_PARAM on bad arguments.
 *
 * @note   Polls HAL_SPI_GetState() instead of the DMA TC flag so that the
 *         CR2_TXDMAEN clear and shift-register drain inside the HAL TxCplt
 *         callback are guaranteed before the caller releases CS.
 */
platform_err_t mcu_hard_spi_wait_complete(mcu_spi_bus_t bus,
                                              UINT32_t       timeout)
{
    if (bus >= MCU_SPI_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    /**
     * Poll HAL state machine; HAL TxCpltCallback transitions it to READY
     * after both DMA TC and SPI shift-register drain.
     **/
    UINT32_t start = HAL_GetTick();
    platform_err_t result = PLATFORM_OK;
    while (HAL_SPI_GetState(spi_port[bus].hard_spi_handle) !=
           HAL_SPI_STATE_READY)
    {
        if ((HAL_GetTick() - start) > timeout)
        {
            result = PLATFORM_ERR_TIMEOUT;
            break;
        }
    }

    osal_mutex_give(spi_port[bus].os_mutexid);
    return result;
}
#endif /* HAL_SPI_MODULE_ENABLED */

/* ---------- Software SPI bus primitives ---------- */

platform_err_t mcu_soft_spi_cs_select(mcu_spi_bus_t bus)
{
    if (bus >= MCU_SPI_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    spi_cs_select(&spi_port[bus].soft_spi_bus_inst);
    return PLATFORM_OK;
}

platform_err_t mcu_soft_spi_cs_deselect(mcu_spi_bus_t bus)
{
    if (bus >= MCU_SPI_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    spi_cs_deselect(&spi_port[bus].soft_spi_bus_inst);
    return PLATFORM_OK;
}

platform_err_t mcu_soft_spi_write_byte(mcu_spi_bus_t bus, UINT8_t byte)
{
    if (bus >= MCU_SPI_BUS_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    spi_write_byte(&spi_port[bus].soft_spi_bus_inst, byte);
    return PLATFORM_OK;
}

platform_err_t mcu_soft_spi_read_byte(mcu_spi_bus_t bus, UINT8_t *byte)
{
    if (bus >= MCU_SPI_BUS_MAX || NULL == byte)
    {
        return PLATFORM_ERR_PARAM;
    }

    *byte = spi_read_byte(&spi_port[bus].soft_spi_bus_inst);
    return PLATFORM_OK;
}

platform_err_t mcu_soft_spi_readwrite_byte(mcu_spi_bus_t bus,
                                                    UINT8_t  tx_byte,
                                                    UINT8_t *rx_byte)
{
    if (bus >= MCU_SPI_BUS_MAX || NULL == rx_byte)
    {
        return PLATFORM_ERR_PARAM;
    }

    *rx_byte = spi_readwrite_byte(&spi_port[bus].soft_spi_bus_inst, tx_byte);
    return PLATFORM_OK;
}

//******************************* Functions *********************************//
