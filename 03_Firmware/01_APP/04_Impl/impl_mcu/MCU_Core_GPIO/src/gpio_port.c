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
 * mcu_gpio_bus_t index, keeping all pin assignments in one place for
 * auditability.
 *
 * Processing flow:
 *   mcu_gpio_write_pin(pin_id, state) → validate index → HAL_GPIO_WritePin
 *
 * @version V1.0 2026-04-26
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "gpio_port.h"

/* HAL types/pin defines live here, not in the public port header, so that
 * upper layers including gpio_port.h stay MCU-agnostic. */
#include "main.h"
#include "stm32f4xx_hal.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

/* Pin configuration mode — descriptive metadata for the registry below. */
typedef enum
{
    MCU_GPIO_MODE_OUTPUT = 0,
    MCU_GPIO_MODE_INPUT,
    MCU_GPIO_MODE_EXTI_RISING,
    MCU_GPIO_MODE_EXTI_FALLING,
} mcu_gpio_mode_t;

/**
 * @brief One registry entry per GPIO pin enumeration (mcu_gpio_bus_t).
 *        Binds a logical pin index to its concrete HAL port/pin + owner.
 */
typedef struct
{
    GPIO_TypeDef      *port;
    uint16_t           pin;
    mcu_gpio_mode_t   mode;
    char const        *owner;   /**< Owning subsystem / peripheral */
} mcu_gpio_port_t;
//******************************** Defines **********************************//

//******************************* Declaring *********************************//

/**
 * @brief Central GPIO pin registry — one entry per mcu_gpio_bus_t.
 *
 *        | Bus index                | PORT  | PIN | Mode              | Owner    |
 *        |--------------------------|-------|-----|-------------------|----------|
 *        | MCU_GPIO_LCD_PIN_RST    | PA    | 4   | OUTPUT            | ST7789   |
 *        | MCU_GPIO_LCD_PIN_DC     | PA    | 6   | OUTPUT            | ST7789   |
 *        | MCU_GPIO_LCD_PIN_CS     | PA    | 3   | OUTPUT            | ST7789   |
 *        | MCU_GPIO_TP_PIN_TINT    | PB    | 2   | EXTI_RISING       | CST816T  |
 *        | MCU_GPIO_WT_PIN_BUSY    | PA    | 12  | INPUT             | WT588F02 |
 *        | MCU_GPIO_MPU_PIN_INT    | PB    | 5   | EXTI_RISING       | MPU6050  |
 *        | MCU_GPIO_LED_PIN        | PC    | 13  | OUTPUT            | System   |
 *        | MCU_GPIO_SOFT_SPI_PIN_SCK| PA   | 5   | OUTPUT (SPI alt)  | SoftSPI  |
 *        | MCU_GPIO_SOFT_SPI_PIN_MISO| PA  | 6   | INPUT  (SPI alt)  | SoftSPI  |
 *        | MCU_GPIO_SOFT_SPI_PIN_MOSI| PA  | 7   | OUTPUT (SPI alt)  | SoftSPI  |
 *        | MCU_GPIO_SOFT_SPI_PIN_CS  | PA  | 4   | OUTPUT (SPI alt)  | SoftSPI  |
 *
 * @note  PA6 (SPI1_DC) and PA4 (SPI1_RST) are shared between LCD and soft SPI
 *        depending on whether hardware or software SPI is active.  The enum
 *        lists both roles separately for documentation; the hardware
 *        configuration (CubeMX) determines the actual function at runtime.
 */
static mcu_gpio_port_t gpio_pin_table[MCU_GPIO_MAX_NUM] =
{
    /* ---- Display (ST7789) ---- */
    [MCU_GPIO_LCD_PIN_RST] = {
        .port  = DISPALY_SPI1_RST_GPIO_Port,
        .pin   = DISPALY_SPI1_RST_Pin,
        .mode  = MCU_GPIO_MODE_OUTPUT,
        .owner = "ST7789",
    },
    [MCU_GPIO_LCD_PIN_DC] = {
        .port  = DISPALY_SPI1_DC_GPIO_Port,
        .pin   = DISPALY_SPI1_DC_Pin,
        .mode  = MCU_GPIO_MODE_OUTPUT,
        .owner = "ST7789",
    },
    [MCU_GPIO_LCD_PIN_CS] = {
        .port  = DISPALY_SPI1_CS_GPIO_Port,
        .pin   = DISPALY_SPI1_CS_Pin,
        .mode  = MCU_GPIO_MODE_OUTPUT,
        .owner = "ST7789",
    },

    /* ---- Touch panel (CST816T) ---- */
    [MCU_GPIO_TP_PIN_TINT] = {
        .port  = TP_TINT_GPIO_Port,
        .pin   = TP_TINT_Pin,
        .mode  = MCU_GPIO_MODE_EXTI_RISING,
        .owner = "CST816T",
    },

    /* ---- Audio (WT588F02) ---- */
    [MCU_GPIO_WT_PIN_BUSY] = {
        .port  = WT_BUSY_GPIO_Port,
        .pin   = WT_BUSY_Pin,
        .mode  = MCU_GPIO_MODE_INPUT,
        .owner = "WT588F02",
    },

    /* ---- Motion (MPU6050) ---- */
    [MCU_GPIO_MPU_PIN_INT] = {
        .port  = GPIOB,
        .pin   = GPIO_PIN_5,
        .mode  = MCU_GPIO_MODE_EXTI_RISING,
        .owner = "MPU6050",
    },

    /* ---- Onboard LED ---- */
    [MCU_GPIO_LED_PIN] = {
        .port  = GPIOC,
        .pin   = GPIO_PIN_13,
        .mode  = MCU_GPIO_MODE_OUTPUT,
        .owner = "System",
    },
};

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/**
 * @brief  No-op init — GPIO init is handled by MX_GPIO_Init() in gpio.c
 *         before the scheduler starts.  Kept for API symmetry with
 *         mcu_i2c_port_init / mcu_spi_port_init.
 *
 * @param[in] pin_id : Pin index (bounds-checked only).
 *
 * @return PLATFORM_OK if pin_id is valid, PLATFORM_ERR_PARAM otherwise.
 */
platform_err_t mcu_gpio_port_init(mcu_gpio_bus_t pin_id)
{
    if (pin_id >= MCU_GPIO_MAX_NUM)
    {
        return PLATFORM_ERR_PARAM;
    }
    return PLATFORM_OK;
}

/**
 * @brief  Write a GPIO pin to the given state.
 *
 * @param[in] pin_id : Pin index from mcu_gpio_bus_t.
 * @param[in] state  : MCU_GPIO_SET (high) or MCU_GPIO_RESET (low).
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_PARAM on invalid index.
 */
platform_err_t mcu_gpio_write_pin(mcu_gpio_bus_t       pin_id,
                                       mcu_gpio_pin_state_t state)
{
    if (pin_id >= MCU_GPIO_MAX_NUM)
    {
        return PLATFORM_ERR_PARAM;
    }

    mcu_gpio_port_t const *p = &gpio_pin_table[pin_id];

    HAL_GPIO_WritePin(p->port,
                      p->pin,
                      (MCU_GPIO_SET == state) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return PLATFORM_OK;
}

/**
 * @brief  Read the current logic level of a GPIO pin.
 *
 * @param[in]  pin_id  : Pin index from mcu_gpio_bus_t.
 * @param[out] p_state : Receives MCU_GPIO_SET or MCU_GPIO_RESET.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_PARAM on invalid args.
 */
platform_err_t mcu_gpio_read_pin(mcu_gpio_bus_t        pin_id,
                                      mcu_gpio_pin_state_t *p_state)
{
    if (pin_id >= MCU_GPIO_MAX_NUM || NULL == p_state)
    {
        return PLATFORM_ERR_PARAM;
    }

    mcu_gpio_port_t const *p = &gpio_pin_table[pin_id];

    GPIO_PinState hw = HAL_GPIO_ReadPin(p->port, p->pin);
    *p_state = (GPIO_PIN_SET == hw) ? MCU_GPIO_SET : MCU_GPIO_RESET;
    return PLATFORM_OK;
}

/**
 * @brief  Toggle the output of a GPIO pin.
 *
 * @param[in] pin_id : Pin index from mcu_gpio_bus_t.
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_PARAM on invalid index.
 */
platform_err_t mcu_gpio_toggle_pin(mcu_gpio_bus_t pin_id)
{
    if (pin_id >= MCU_GPIO_MAX_NUM)
    {
        return PLATFORM_ERR_PARAM;
    }

    mcu_gpio_port_t const *p = &gpio_pin_table[pin_id];

    HAL_GPIO_TogglePin(p->port, p->pin);
    return PLATFORM_OK;
}

//******************************* Functions *********************************//
