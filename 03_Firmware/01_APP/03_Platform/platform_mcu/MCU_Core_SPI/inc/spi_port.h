/******************************************************************************
 * @file spi_port.h
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Define hardware/software SPI port abstraction and bus primitives.
 *
 * Processing flow:
 * Expose core SPI types, status codes, and software-SPI access macros.
 * @version V1.0 2026--
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __SPI_PORT_H__
#define __SPI_PORT_H__

//******************************** Includes *********************************//
#include "main.h"
#include "spi.h"
#include "spi_hal.h"

#include "osal_mutex.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    CORE_SPI_OK      = 0,
    CORE_SPI_ERROR   = 1,
    CORE_SPI_BUSY    = 2,
    CORE_SPI_TIMEOUT = 3
} core_spi_status_t;

typedef enum
{
    HARDWARE_SPI = 0,
    SOFTWARE_SPI = 1
} spi_port_type_t;

typedef struct
{
    spi_port_type_t      core_spi_state;
    spi_bus_t            soft_spi_bus_inst;
#ifdef HAL_SPI_MODULE_ENABLED
    SPI_HandleTypeDef   *hard_spi_handle;
    GPIO_TypeDef        *hard_spi_cs_port;
    uint16_t             hard_spi_cs_pin;
    osal_mutex_handle_t  os_mutexid;
#endif
} spi_port_t;

typedef enum
{
    CORE_SPI_BUS_1 = 0,    /* SPI1 -- ST7789 display          */
    CORE_SPI_BUS_2,        /* SPI2 -- W25Q64 NOR flash        */
    CORE_SPI_BUS_MAX
} core_spi_bus_t;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

// SPI port init (initializes mutex for hardware SPI bus, or GPIO for soft SPI)
core_spi_status_t core_spi_port_init           (core_spi_bus_t              bus);

#ifdef HAL_SPI_MODULE_ENABLED
// Hardware SPI CS control
core_spi_status_t core_hard_spi_cs_select      (core_spi_bus_t              bus);
core_spi_status_t core_hard_spi_cs_deselect    (core_spi_bus_t              bus);

// Hardware SPI bus primitives
core_spi_status_t core_hard_spi_transmit       (core_spi_bus_t              bus,
                                                      uint8_t          *   data,
                                                      uint16_t             size,
                                                      uint32_t          timeout);
core_spi_status_t core_hard_spi_receive        (core_spi_bus_t              bus,
                                                      uint8_t          *   data,
                                                      uint16_t             size,
                                                      uint32_t          timeout);
core_spi_status_t core_hard_spi_transmit_receive(core_spi_bus_t             bus,
                                                      uint8_t          *tx_data,
                                                      uint8_t          *rx_data,
                                                      uint16_t             size,
                                                      uint32_t          timeout);
core_spi_status_t core_hard_spi_transmit_dma   (core_spi_bus_t              bus,
                                                      uint8_t          *   data,
                                                      uint16_t             size);
core_spi_status_t core_hard_spi_receive_dma    (core_spi_bus_t              bus,
                                                      uint8_t          *   data,
                                                      uint16_t             size);

/* Block-poll until the previous DMA transfer drains the SPI shift
 * register, then release the bus mutex.  Pairs with the
 * core_hard_spi_*_dma calls above for synchronous flush patterns. */
core_spi_status_t core_hard_spi_wait_complete  (core_spi_bus_t              bus,
                                                      uint32_t          timeout);

/* Release the bus mutex after a DMA transfer completes (call from ISR) */
void core_hard_spi_dma_complete                (core_spi_bus_t              bus);
#endif /* HAL_SPI_MODULE_ENABLED */

// Software SPI bus primitives
core_spi_status_t core_soft_spi_cs_select      (core_spi_bus_t              bus);
core_spi_status_t core_soft_spi_cs_deselect    (core_spi_bus_t              bus);
core_spi_status_t core_soft_spi_write_byte     (core_spi_bus_t              bus,
                                                      uint8_t             byte);
core_spi_status_t core_soft_spi_read_byte      (core_spi_bus_t              bus,
                                                      uint8_t          *  byte);
core_spi_status_t core_soft_spi_readwrite_byte (core_spi_bus_t              bus,
                                                      uint8_t          tx_byte,
                                                      uint8_t          *rx_byte);

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/* Software SPI bus primitives (CORE_SPI_BUS_1) */
#define SENSOR_SOFTWARE_SPI_CS_SELECT()                                        \
    core_soft_spi_cs_select(CORE_SPI_BUS_1)

#define SENSOR_SOFTWARE_SPI_CS_DESELECT()                                      \
    core_soft_spi_cs_deselect(CORE_SPI_BUS_1)

#define SENSOR_SOFTWARE_SPI_WRITE_BYTE(data)                                   \
    core_soft_spi_write_byte(CORE_SPI_BUS_1, (data))

#define SENSOR_SOFTWARE_SPI_READ_BYTE(p_byte)                                  \
    core_soft_spi_read_byte(CORE_SPI_BUS_1, (p_byte))

#define SENSOR_SOFTWARE_SPI_READWRITE_BYTE(tx, p_rx)                           \
    core_soft_spi_readwrite_byte(CORE_SPI_BUS_1, (tx), (p_rx))

#ifdef HAL_SPI_MODULE_ENABLED
/* Hardware SPI bus primitives (CORE_SPI_BUS_1 — display / ST7789) */
#define DISPLAY_HARDWARE_SPI_TRANSMIT(data, size, timeout)                     \
    core_hard_spi_transmit(CORE_SPI_BUS_1, (data), (size), (timeout))

#define DISPLAY_HARDWARE_SPI_TRANSMIT_DMA(data, size)                          \
    core_hard_spi_transmit_dma(CORE_SPI_BUS_1, (data), (size))

#define DISPLAY_HARDWARE_SPI_WAIT_COMPLETE(timeout)                            \
    core_hard_spi_wait_complete(CORE_SPI_BUS_1, (timeout))

/* Hardware SPI bus primitives (CORE_SPI_BUS_2 — flash / W25Q64) */
#define FLASH_HARDWARE_SPI_TRANSMIT(data, size, timeout)                       \
    core_hard_spi_transmit(CORE_SPI_BUS_2, (data), (size), (timeout))

#define FLASH_HARDWARE_SPI_RECEIVE(data, size, timeout)                        \
    core_hard_spi_receive(CORE_SPI_BUS_2, (data), (size), (timeout))

#define FLASH_HARDWARE_SPI_CS_SELECT()                                         \
    core_hard_spi_cs_select(CORE_SPI_BUS_2)

#define FLASH_HARDWARE_SPI_CS_DESELECT()                                       \
    core_hard_spi_cs_deselect(CORE_SPI_BUS_2)
#endif /* HAL_SPI_MODULE_ENABLED */

//******************************* Functions *********************************//

#endif /* __SPI_PORT_H__ */
