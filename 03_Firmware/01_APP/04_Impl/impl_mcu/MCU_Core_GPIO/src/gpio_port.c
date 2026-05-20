/******************************************************************************
 * @file gpio_port.c
 *
 * @par dependencies
 * - gpio_port.h
 * - main.h
 *
 * @author Ethan-Hang
 *
 * @brief MCU-port GPIO layer — centralized pin registry with write/read
 *        /toggle operations and per-pin ownership tracking.
 *
 * Every manually-controlled GPIO pin in the system is registered in the
 * gpio_pin_table[] below.  Integration layers reference pins by their
 * core_gpio_bus_t index, keeping all pin assignments in one place for
 * auditability.
 *
 * Processing flow:
 *   core_gpio_write_pin(pin_id, state) → validate index → HAL_GPIO_WritePin
 *
 * @version V1.0 2026-04-26
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "gpio_port.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

/**
 * @brief Central GPIO pin registry — one entry per core_gpio_bus_t.
 *
 *        | Bus index                | PORT  | PIN | Mode              | Owner    |
 *        |--------------------------|-------|-----|-------------------|----------|
 *        | CORE_GPIO_LCD_PIN_RST    | PA    | 4   | OUTPUT            | ST7789   |
 *        | CORE_GPIO_LCD_PIN_DC     | PA    | 6   | OUTPUT            | ST7789   |
 *        | CORE_GPIO_LCD_PIN_CS     | PA    | 3   | OUTPUT            | ST7789   |
 *        | CORE_GPIO_TP_PIN_TINT    | PB    | 2   | EXTI_RISING       | CST816T  |
 *        | CORE_GPIO_WT_PIN_BUSY    | PA    | 12  | INPUT             | WT588F02 |
 *        | CORE_GPIO_MPU_PIN_INT    | PB    | 5   | EXTI_RISING       | MPU6050  |
 *        | CORE_GPIO_LED_PIN        | PC    | 13  | OUTPUT            | System   |
 *        | CORE_GPIO_SOFT_SPI_PIN_SCK| PA   | 5   | OUTPUT (SPI alt)  | SoftSPI  |
 *        | CORE_GPIO_SOFT_SPI_PIN_MISO| PA  | 6   | INPUT  (SPI alt)  | SoftSPI  |
 *        | CORE_GPIO_SOFT_SPI_PIN_MOSI| PA  | 7   | OUTPUT (SPI alt)  | SoftSPI  |
 *        | CORE_GPIO_SOFT_SPI_PIN_CS  | PA  | 4   | OUTPUT (SPI alt)  | SoftSPI  |
 *
 * @note  PA6 (SPI1_DC) and PA4 (SPI1_RST) are shared between LCD and soft SPI
 *        depending on whether hardware or software SPI is active.  The enum
 *        lists both roles separately for documentation; the hardware
 *        configuration (CubeMX) determines the actual function at runtime.
 */
static core_gpio_port_t gpio_pin_table[CORE_GPIO_MAX_NUM] =
{
    /* ---- Display (ST7789) ---- */
    [CORE_GPIO_LCD_PIN_RST] = {
        .port  = DISPALY_SPI1_RST_GPIO_Port,
        .pin   = DISPALY_SPI1_RST_Pin,
        .mode  = CORE_GPIO_MODE_OUTPUT,
        .owner = "ST7789",
    },
    [CORE_GPIO_LCD_PIN_DC] = {
        .port  = DISPALY_SPI1_DC_GPIO_Port,
        .pin   = DISPALY_SPI1_DC_Pin,
        .mode  = CORE_GPIO_MODE_OUTPUT,
        .owner = "ST7789",
    },
    [CORE_GPIO_LCD_PIN_CS] = {
        .port  = DISPALY_SPI1_CS_GPIO_Port,
        .pin   = DISPALY_SPI1_CS_Pin,
        .mode  = CORE_GPIO_MODE_OUTPUT,
        .owner = "ST7789",
    },

    /* ---- Touch panel (CST816T) ---- */
    [CORE_GPIO_TP_PIN_TINT] = {
        .port  = TP_TINT_GPIO_Port,
        .pin   = TP_TINT_Pin,
        .mode  = CORE_GPIO_MODE_EXTI_RISING,
        .owner = "CST816T",
    },

    /* ---- Audio (WT588F02) ---- */
    [CORE_GPIO_WT_PIN_BUSY] = {
        .port  = WT_BUSY_GPIO_Port,
        .pin   = WT_BUSY_Pin,
        .mode  = CORE_GPIO_MODE_INPUT,
        .owner = "WT588F02",
    },

    /* ---- Motion (MPU6050) ---- */
    [CORE_GPIO_MPU_PIN_INT] = {
        .port  = GPIOB,
        .pin   = GPIO_PIN_5,
        .mode  = CORE_GPIO_MODE_EXTI_RISING,
        .owner = "MPU6050",
    },

    /* ---- Onboard LED ---- */
    [CORE_GPIO_LED_PIN] = {
        .port  = GPIOC,
        .pin   = GPIO_PIN_13,
        .mode  = CORE_GPIO_MODE_OUTPUT,
        .owner = "System",
    },
};

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/**
 * @brief  No-op init — GPIO init is handled by MX_GPIO_Init() in gpio.c
 *         before the scheduler starts.  Kept for API symmetry with
 *         core_i2c_port_init / core_spi_port_init.
 *
 * @param[in] pin_id : Pin index (bounds-checked only).
 *
 * @return CORE_GPIO_OK if pin_id is valid, CORE_GPIO_ERROR otherwise.
 */
core_gpio_status_t core_gpio_port_init(core_gpio_bus_t pin_id)
{
    if (pin_id >= CORE_GPIO_MAX_NUM)
    {
        return CORE_GPIO_ERROR;
    }
    return CORE_GPIO_OK;
}

/**
 * @brief  Write a GPIO pin to the given state.
 *
 * @param[in] pin_id : Pin index from core_gpio_bus_t.
 * @param[in] state  : CORE_GPIO_SET (high) or CORE_GPIO_RESET (low).
 *
 * @return CORE_GPIO_OK on success, CORE_GPIO_ERROR on invalid index.
 */
core_gpio_status_t core_gpio_write_pin(core_gpio_bus_t       pin_id,
                                       core_gpio_pin_state_t state)
{
    if (pin_id >= CORE_GPIO_MAX_NUM)
    {
        return CORE_GPIO_ERROR;
    }

    core_gpio_port_t const *p = &gpio_pin_table[pin_id];

    HAL_GPIO_WritePin(p->port,
                      p->pin,
                      (CORE_GPIO_SET == state) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return CORE_GPIO_OK;
}

/**
 * @brief  Read the current logic level of a GPIO pin.
 *
 * @param[in]  pin_id  : Pin index from core_gpio_bus_t.
 * @param[out] p_state : Receives CORE_GPIO_SET or CORE_GPIO_RESET.
 *
 * @return CORE_GPIO_OK on success, CORE_GPIO_ERROR on invalid args.
 */
core_gpio_status_t core_gpio_read_pin(core_gpio_bus_t        pin_id,
                                      core_gpio_pin_state_t *p_state)
{
    if (pin_id >= CORE_GPIO_MAX_NUM || NULL == p_state)
    {
        return CORE_GPIO_ERROR;
    }

    core_gpio_port_t const *p = &gpio_pin_table[pin_id];

    GPIO_PinState hw = HAL_GPIO_ReadPin(p->port, p->pin);
    *p_state = (GPIO_PIN_SET == hw) ? CORE_GPIO_SET : CORE_GPIO_RESET;
    return CORE_GPIO_OK;
}

/**
 * @brief  Toggle the output of a GPIO pin.
 *
 * @param[in] pin_id : Pin index from core_gpio_bus_t.
 *
 * @return CORE_GPIO_OK on success, CORE_GPIO_ERROR on invalid index.
 */
core_gpio_status_t core_gpio_toggle_pin(core_gpio_bus_t pin_id)
{
    if (pin_id >= CORE_GPIO_MAX_NUM)
    {
        return CORE_GPIO_ERROR;
    }

    core_gpio_port_t const *p = &gpio_pin_table[pin_id];

    HAL_GPIO_TogglePin(p->port, p->pin);
    return CORE_GPIO_OK;
}

/**
 * @brief  Reverse-lookup a pin by PORT+PIN.  Used for debug/statistics dumps.
 *
 * @param[in] port : GPIO port base.
 * @param[in] pin  : Pin bitmask.
 *
 * @return Matching core_gpio_bus_t index, or CORE_GPIO_MAX_NUM if unmatched.
 */
core_gpio_bus_t core_gpio_find_pin(GPIO_TypeDef *port, uint16_t pin)
{
    for (core_gpio_bus_t i = 0; i < CORE_GPIO_MAX_NUM; i++)
    {
        if (gpio_pin_table[i].port == port && gpio_pin_table[i].pin == pin)
        {
            return i;
        }
    }
    return CORE_GPIO_MAX_NUM;
}

//******************************* Functions *********************************//
