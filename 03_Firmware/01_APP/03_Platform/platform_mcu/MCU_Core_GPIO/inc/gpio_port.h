/******************************************************************************
 * @file gpio_port.h
 *
 * @par dependencies
 * - main.h (GPIO pin defines from CubeMX)
 * - stm32f4xx_hal.h
 *
 * @author Ethan-Hang
 *
 * @brief MCU-port GPIO abstraction — unified pin write/read/toggle with
 *        per-pin ownership tracking.
 *
 * All manually-controlled GPIO pins are registered in core_gpio_bus_t.
 * Integration layers call core_gpio_write_pin / core_gpio_read_pin instead
 * of raw HAL_GPIO_WritePin / HAL_GPIO_ReadPin so that pin assignment is
 * centralized and auditable.
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
#include "main.h"
#include "stm32f4xx_hal.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    CORE_GPIO_OK    = 0,
    CORE_GPIO_ERROR = 1,
} core_gpio_status_t;

typedef enum
{
    CORE_GPIO_RESET = 0,
    CORE_GPIO_SET   = 1,
} core_gpio_pin_state_t;

typedef enum
{
    CORE_GPIO_MODE_OUTPUT = 0,
    CORE_GPIO_MODE_INPUT,
    CORE_GPIO_MODE_EXTI_RISING,
    CORE_GPIO_MODE_EXTI_FALLING,
} core_gpio_mode_t;

/**
 * @brief Pin ownership — used for usage tracking.
 *        One entry per GPIO pin enumeration below.
 */
typedef struct
{
    GPIO_TypeDef      *port;
    uint16_t           pin;
    core_gpio_mode_t   mode;
    char const        *owner;   /**< Owning subsystem / peripheral */
} core_gpio_port_t;

/**
 * @brief Central GPIO pin enumeration — every manually-controlled pin.
 *        Integration code references these symbolic names instead of raw
 *        PORT+PIN pairs, so all pin assignments are visible here.
 */
typedef enum
{
    /* ---- Display (ST7789 via SPI1) ---- */
    CORE_GPIO_LCD_PIN_RST = 0,
    CORE_GPIO_LCD_PIN_DC,
    CORE_GPIO_LCD_PIN_CS,

    /* ---- Touch panel (CST816T) ---- */
    CORE_GPIO_TP_PIN_TINT,

    /* ---- Audio (WT588F02) ---- */
    CORE_GPIO_WT_PIN_BUSY,

    /* ---- Motion (MPU6050) ---- */
    CORE_GPIO_MPU_PIN_INT,

    /* ---- Onboard LED ---- */
    CORE_GPIO_LED_PIN,

    CORE_GPIO_MAX_NUM
} core_gpio_bus_t;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

core_gpio_status_t core_gpio_port_init  (core_gpio_bus_t     pin_id);

core_gpio_status_t core_gpio_write_pin  (core_gpio_bus_t     pin_id,
                                         core_gpio_pin_state_t   state);

core_gpio_status_t core_gpio_read_pin   (core_gpio_bus_t     pin_id,
                                         core_gpio_pin_state_t  *p_state);

core_gpio_status_t core_gpio_toggle_pin (core_gpio_bus_t     pin_id);

/** @brief Look up a pin by PORT+PIN and return its bus index, or
 *         CORE_GPIO_MAX_NUM if not found.  Used for debug/dump only. */
core_gpio_bus_t    core_gpio_find_pin   (GPIO_TypeDef       *port,
                                         uint16_t             pin);

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/* Convenience macros — peripheral-scoped aliases to core_gpio_write_pin.
   Integration layers use these so the pin binding lives exclusively in
   gpio_port.c. */

/** @brief Display LCD CS  — PA3, active-low */
#define DISPLAY_GPIO_WRITE_CS(state)                                          \
    core_gpio_write_pin(CORE_GPIO_LCD_PIN_CS, (state))

/** @brief Display LCD RST — PA4, active-low */
#define DISPLAY_GPIO_WRITE_RST(state)                                         \
    core_gpio_write_pin(CORE_GPIO_LCD_PIN_RST, (state))

/** @brief Display LCD DC  — PA6, CMD=low / DATA=high */
#define DISPLAY_GPIO_WRITE_DC(state)                                          \
    core_gpio_write_pin(CORE_GPIO_LCD_PIN_DC, (state))

/** @brief Touch panel INT — PB2 (input, read-only) */
#define TOUCH_GPIO_READ_TINT(p_state)                                         \
    core_gpio_read_pin(CORE_GPIO_TP_PIN_TINT, (p_state))

/** @brief WT588 busy — PA12 (input, read-only) */
#define AUDIO_GPIO_READ_BUSY(p_state)                                         \
    core_gpio_read_pin(CORE_GPIO_WT_PIN_BUSY, (p_state))

/** @brief MPU6050 INT — PB5 (input, read-only) */
#define MOTION_GPIO_READ_INT(p_state)                                         \
    core_gpio_read_pin(CORE_GPIO_MPU_PIN_INT, (p_state))

/** @brief Onboard LED — PC13 (active-low) */
#define LED_GPIO_WRITE(state)                                                 \
    core_gpio_write_pin(CORE_GPIO_LED_PIN, (state))

#define LED_GPIO_TOGGLE()                                                     \
    core_gpio_toggle_pin(CORE_GPIO_LED_PIN)

//******************************* Functions *********************************//

#endif /* __GPIO_PORT_H__ */
