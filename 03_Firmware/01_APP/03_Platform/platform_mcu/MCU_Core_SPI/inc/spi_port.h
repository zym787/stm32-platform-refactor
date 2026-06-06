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
#include "platform_error.h"
#include "platform_type.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* This header is MCU-agnostic: hardware/software port descriptors and all
 * HAL handle types live in the implementation (spi_port.c).  Callers see
 * only the logical bus index and the platform_err_t API below. */
typedef enum
{
    MCU_SPI_BUS_1 = 0,    /* SPI1 -- ST7789 display          */
    MCU_SPI_BUS_2,        /* SPI2 -- W25Q64 NOR flash        */
    MCU_SPI_BUS_MAX
} mcu_spi_bus_t;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

// SPI port init (initializes mutex for hardware SPI bus, or GPIO for soft SPI)
platform_err_t mcu_spi_port_init            (mcu_spi_bus_t              bus);

// Hardware SPI CS control
platform_err_t mcu_hard_spi_cs_select       (mcu_spi_bus_t              bus);
platform_err_t mcu_hard_spi_cs_deselect     (mcu_spi_bus_t              bus);

// Hardware SPI bus primitives
platform_err_t mcu_hard_spi_transmit         (mcu_spi_bus_t             bus,
                                                     uint8_t         *   data,
                                                     uint16_t            size,
                                                     uint32_t         timeout);
platform_err_t mcu_hard_spi_receive          (mcu_spi_bus_t             bus,
                                                     uint8_t         *   data,
                                                     uint16_t            size,
                                                     uint32_t         timeout);
platform_err_t mcu_hard_spi_transmit_receive (mcu_spi_bus_t             bus,
                                                    uint8_t          *tx_data,
                                                    uint8_t          *rx_data,
                                                    uint16_t             size,
                                                    uint32_t          timeout);
platform_err_t mcu_hard_spi_transmit_dma    (mcu_spi_bus_t              bus,
                                                    uint8_t          *   data,
                                                    uint16_t             size);
platform_err_t mcu_hard_spi_receive_dma     (mcu_spi_bus_t              bus,
                                                    uint8_t          *   data,
                                                    uint16_t             size);

/* Block-poll until the previous DMA transfer drains the SPI shift
 * register, then release the bus mutex.  Pairs with the
 * mcu_hard_spi_*_dma calls above for synchronous flush patterns. */
platform_err_t mcu_hard_spi_wait_complete   (mcu_spi_bus_t              bus,
                                                    uint32_t          timeout);

/* Release the bus mutex after a DMA transfer completes (call from ISR) */
void mcu_hard_spi_dma_complete              (mcu_spi_bus_t              bus);

// Software SPI bus primitives
platform_err_t mcu_soft_spi_cs_select       (mcu_spi_bus_t              bus);
platform_err_t mcu_soft_spi_cs_deselect     (mcu_spi_bus_t              bus);
platform_err_t mcu_soft_spi_write_byte      (mcu_spi_bus_t              bus,
                                                     uint8_t             byte);
platform_err_t mcu_soft_spi_read_byte       (mcu_spi_bus_t              bus,
                                                     uint8_t          *  byte);
platform_err_t mcu_soft_spi_readwrite_byte  (mcu_spi_bus_t              bus,
                                                     uint8_t          tx_byte,
                                                     uint8_t         *rx_byte);

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/* Software SPI bus primitives (MCU_SPI_BUS_1) */
#define SENSOR_SOFTWARE_SPI_CS_SELECT()                                        \
    mcu_soft_spi_cs_select(MCU_SPI_BUS_1)

#define SENSOR_SOFTWARE_SPI_CS_DESELECT()                                      \
    mcu_soft_spi_cs_deselect(MCU_SPI_BUS_1)

#define SENSOR_SOFTWARE_SPI_WRITE_BYTE(data)                                   \
    mcu_soft_spi_write_byte(MCU_SPI_BUS_1, (data))

#define SENSOR_SOFTWARE_SPI_READ_BYTE(p_byte)                                  \
    mcu_soft_spi_read_byte(MCU_SPI_BUS_1, (p_byte))

#define SENSOR_SOFTWARE_SPI_READWRITE_BYTE(tx, p_rx)                           \
    mcu_soft_spi_readwrite_byte(MCU_SPI_BUS_1, (tx), (p_rx))

/* Hardware SPI bus primitives (MCU_SPI_BUS_1 — display / ST7789) */
#define DISPLAY_HARDWARE_SPI_TRANSMIT(data, size, timeout)                     \
    mcu_hard_spi_transmit(MCU_SPI_BUS_1, (data), (size), (timeout))

#define DISPLAY_HARDWARE_SPI_TRANSMIT_DMA(data, size)                          \
    mcu_hard_spi_transmit_dma(MCU_SPI_BUS_1, (data), (size))

#define DISPLAY_HARDWARE_SPI_WAIT_COMPLETE(timeout)                            \
    mcu_hard_spi_wait_complete(MCU_SPI_BUS_1, (timeout))

/* Hardware SPI bus primitives (MCU_SPI_BUS_2 — flash / W25Q64) */
#define FLASH_HARDWARE_SPI_TRANSMIT(data, size, timeout)                       \
    mcu_hard_spi_transmit(MCU_SPI_BUS_2, (data), (size), (timeout))

#define FLASH_HARDWARE_SPI_RECEIVE(data, size, timeout)                        \
    mcu_hard_spi_receive(MCU_SPI_BUS_2, (data), (size), (timeout))

#define FLASH_HARDWARE_SPI_CS_SELECT()                                         \
    mcu_hard_spi_cs_select(MCU_SPI_BUS_2)

#define FLASH_HARDWARE_SPI_CS_DESELECT()                                       \
    mcu_hard_spi_cs_deselect(MCU_SPI_BUS_2)

//******************************* Functions *********************************//

#endif /* __SPI_PORT_H__ */
