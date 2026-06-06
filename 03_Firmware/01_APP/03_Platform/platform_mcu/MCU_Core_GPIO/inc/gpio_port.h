/******************************************************************************
 * @file gpio_port.h
 *
 * @par dependencies
 * - platform_error.h
 * - platform_type.h
 *
 * @author Ethan-Hang
 *
 * @brief MCU-port GPIO abstraction — unified pin write/read/toggle with
 *        per-pin ownership tracking.
 *
 * All manually-controlled GPIO pins are registered in mcu_gpio_bus_t.
 * Integration layers call mcu_gpio_write_pin / mcu_gpio_read_pin instead
 * of raw HAL_GPIO_WritePin / HAL_GPIO_ReadPin so that pin assignment is
 * centralized and auditable.  This header is MCU-agnostic: the concrete
 * pin table and all HAL types live in the implementation (gpio_port.c).
 *
 * @version V1.0 2026-04-26
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __GPIO_PORT_H__
#define __GPIO_PORT_H__

//******************************** Includes *********************************//
#include "platform_error.h"
#include "platform_type.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    MCU_GPIO_RESET = 0,
    MCU_GPIO_SET   = 1,
} mcu_gpio_pin_state_t;

/**
 * @brief Central GPIO pin enumeration — every manually-controlled pin.
 *        Integration code references these symbolic names instead of raw
 *        PORT+PIN pairs, so all pin assignments are visible here.
 */
typedef enum
{
    /* ---- Display (ST7789 via SPI1) ---- */
    MCU_GPIO_LCD_PIN_RST = 0,
    MCU_GPIO_LCD_PIN_DC,
    MCU_GPIO_LCD_PIN_CS,

    /* ---- Touch panel (CST816T) ---- */
    MCU_GPIO_TP_PIN_TINT,

    /* ---- Audio (WT588F02) ---- */
    MCU_GPIO_WT_PIN_BUSY,

    /* ---- Motion (MPU6050) ---- */
    MCU_GPIO_MPU_PIN_INT,

    /* ---- Onboard LED ---- */
    MCU_GPIO_LED_PIN,

    MCU_GPIO_MAX_NUM
} mcu_gpio_bus_t;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

platform_err_t mcu_gpio_port_init  (mcu_gpio_bus_t     pin_id);

platform_err_t mcu_gpio_write_pin  (mcu_gpio_bus_t     pin_id,
                                         mcu_gpio_pin_state_t   state);

platform_err_t mcu_gpio_read_pin   (mcu_gpio_bus_t     pin_id,
                                         mcu_gpio_pin_state_t  *p_state);

platform_err_t mcu_gpio_toggle_pin (mcu_gpio_bus_t     pin_id);

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/* Convenience macros — peripheral-scoped aliases to mcu_gpio_write_pin.
   Integration layers use these so the pin binding lives exclusively in
   gpio_port.c. */

/** @brief Display LCD CS  — PA3, active-low */
#define DISPLAY_GPIO_WRITE_CS(state)                                          \
    mcu_gpio_write_pin(MCU_GPIO_LCD_PIN_CS, (state))

/** @brief Display LCD RST — PA4, active-low */
#define DISPLAY_GPIO_WRITE_RST(state)                                         \
    mcu_gpio_write_pin(MCU_GPIO_LCD_PIN_RST, (state))

/** @brief Display LCD DC  — PA6, CMD=low / DATA=high */
#define DISPLAY_GPIO_WRITE_DC(state)                                          \
    mcu_gpio_write_pin(MCU_GPIO_LCD_PIN_DC, (state))

/** @brief Touch panel INT — PB2 (input, read-only) */
#define TOUCH_GPIO_READ_TINT(p_state)                                         \
    mcu_gpio_read_pin(MCU_GPIO_TP_PIN_TINT, (p_state))

/** @brief WT588 busy — PA12 (input, read-only) */
#define AUDIO_GPIO_READ_BUSY(p_state)                                         \
    mcu_gpio_read_pin(MCU_GPIO_WT_PIN_BUSY, (p_state))

/** @brief MPU6050 INT — PB5 (input, read-only) */
#define MOTION_GPIO_READ_INT(p_state)                                         \
    mcu_gpio_read_pin(MCU_GPIO_MPU_PIN_INT, (p_state))

/** @brief Onboard LED — PC13 (active-low) */
#define LED_GPIO_WRITE(state)                                                 \
    mcu_gpio_write_pin(MCU_GPIO_LED_PIN, (state))

#define LED_GPIO_TOGGLE()                                                     \
    mcu_gpio_toggle_pin(MCU_GPIO_LED_PIN)

//******************************* Functions *********************************//

#endif /* __GPIO_PORT_H__ */
