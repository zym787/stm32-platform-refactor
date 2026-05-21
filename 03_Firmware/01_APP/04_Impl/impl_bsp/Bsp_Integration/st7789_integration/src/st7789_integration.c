/******************************************************************************
 * @file st7789_integration.c
 *
 * @par dependencies
 * - st7789_integration.h
 * - bsp_st7789_driver.h
 * - spi_port.h     (MCU-port SPI abstraction)
 * - gpio_port.h    (MCU-port GPIO abstraction)
 * - systick_port.h (MCU-port ms timebase abstraction)
 * - osal_wrapper_adapter.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection bundle for the ST7789 LCD driver.
 *
 * Provides concrete implementations for every interface the ST7789 driver
 * needs (SPI / DMA / GPIO, ms timebase, OS-aware delay) and wires them
 * into st7789_input_arg, consumed by the display adapter port at startup.
 *
 * The visible window is 240x280 inside a 240x320 panel; gui_guider screens
 * are authored at 240x280, so y_offset = 20 keeps the active area aligned
 * with the GRAM origin reported by the driver.
 *
 * @version V1.0 2026-04-25
 * @version V2.0 2026-04-26
 * @version V3.0 2026-04-26
 * @upgrade 2.0: SPI path now goes through DISPLAY_HARDWARE_SPI_* macros
 *               (CORE_SPI_BUS_1 / hspi1) instead of HAL_SPI_* directly.
 * @upgrade 3.0: GPIO CS/DC/RST now route through DISPLAY_GPIO_WRITE_*
 *               macros (gpio_port.h) — no more direct HAL_GPIO_WritePin.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "st7789_integration.h"

#include "spi_port.h"
#include "gpio_port.h"
#include "systick_port.h"
#include "osal_wrapper_adapter.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define ST7789_HAL_PANEL_WIDTH       240U
#define ST7789_HAL_PANEL_HEIGHT      284U
#define ST7789_HAL_PANEL_X_OFFSET    0U
#define ST7789_HAL_PANEL_Y_OFFSET    0U
#define ST7789_HAL_SPI_TX_TIMEOUT_MS 100U
//******************************** Defines **********************************//

//******************************* Functions *********************************//

/* ---- SPI bus (HAL passthrough) ------------------------------------------ */
/**
 * @brief  SPI port init hook.  CubeMX MX_SPI1_Init() already runs from
 *         main() before the scheduler starts, so this is a no-op.  Kept
 *         non-NULL so driver-side input validation accepts the vtable.
 *
 * @return ST7789_OK
 */
static st7789_status_t st7789_spi_init(void)
{
    /**
     * Init handled by MX_SPI1_Init() before the scheduler starts.
     **/
    return ST7789_OK;
}

/**
 * @brief  SPI port deinit hook (no-op for HAL passthrough).
 *
 * @return ST7789_OK
 */
static st7789_status_t st7789_spi_deinit(void)
{
    return ST7789_OK;
}

/**
 * @brief  Blocking SPI transmit through the MCU port abstraction.
 *
 * @param[in] p_data       : Source buffer.
 * @param[in] data_length  : Number of bytes to send.
 *
 * @return ST7789_OK on success, ST7789_ERRORPARAMETER if length exceeds
 *         the HAL uint16_t Size cap, ST7789_ERRORTIMEOUT if the bus mutex
 *         times out, ST7789_ERROR on bus failure.
 */
static st7789_status_t st7789_spi_transmit(uint8_t const *p_data,
                                           uint32_t       data_length)
{
    /**
     * HAL_SPI_Transmit Size is uint16_t; bail loudly if a future caller
     * exceeds that.
     **/
    if (data_length > UINT16_MAX)
    {
        return ST7789_ERRORPARAMETER;
    }

    core_spi_status_t st = DISPLAY_HARDWARE_SPI_TRANSMIT(
        (uint8_t *)p_data, (uint16_t)data_length, ST7789_HAL_SPI_TX_TIMEOUT_MS);
    if (CORE_SPI_TIMEOUT == st)
    {
        return ST7789_ERRORTIMEOUT;
    }
    return (CORE_SPI_OK == st) ? ST7789_OK : ST7789_ERROR;
}

/**
 * @brief  Non-blocking DMA SPI transmit through the MCU port abstraction.
 *         Caller MUST follow up with st7789_spi_wait_dma_complete() to
 *         release the bus mutex held by this call.
 *
 * @param[in] p_data       : Source buffer (caller-owned until completion).
 * @param[in] data_length  : Number of bytes to send.
 *
 * @return ST7789_OK on dispatch, ST7789_ERRORPARAMETER on size overflow,
 *         ST7789_ERRORTIMEOUT if the bus mutex times out, ST7789_ERROR on
 *         bus failure.
 */
static st7789_status_t st7789_spi_transmit_dma(uint8_t const *p_data,
                                               uint32_t       data_length)
{
    if (data_length > UINT16_MAX)
    {
        return ST7789_ERRORPARAMETER;
    }

    core_spi_status_t st = DISPLAY_HARDWARE_SPI_TRANSMIT_DMA(
        (uint8_t *)p_data, (uint16_t)data_length);
    if (CORE_SPI_TIMEOUT == st)
    {
        return ST7789_ERRORTIMEOUT;
    }
    return (CORE_SPI_OK == st) ? ST7789_OK : ST7789_ERROR;
}

/**
 * @brief  Wait until the SPI peripheral becomes idle after a DMA xfer
 *         dispatched via st7789_spi_transmit_dma, then release the
 *         bus mutex.
 *
 * @param[in] timeout_ms : Max wait in ms.
 *
 * @return ST7789_OK on completion, ST7789_ERRORTIMEOUT if the peripheral
 *         did not idle in time.
 */
static st7789_status_t st7789_spi_wait_dma_complete(uint32_t timeout_ms)
{
    core_spi_status_t st = DISPLAY_HARDWARE_SPI_WAIT_COMPLETE(timeout_ms);
    if (CORE_SPI_TIMEOUT == st)
    {
        return ST7789_ERRORTIMEOUT;
    }
    return (CORE_SPI_OK == st) ? ST7789_OK : ST7789_ERROR;
}

/**
 * @brief  Drive the panel CS line.
 *
 * @param[in] state : 0 -> active low (asserted), non-zero -> released.
 *
 * @return ST7789_OK
 */
static st7789_status_t st7789_spi_write_cs_pin(uint8_t state)
{
    DISPLAY_GPIO_WRITE_CS((0U != state) ? CORE_GPIO_SET : CORE_GPIO_RESET);
    return ST7789_OK;
}

/**
 * @brief  Drive the panel D/C line.
 *
 * @param[in] state : 0 -> command, non-zero -> data.
 *
 * @return ST7789_OK
 */
static st7789_status_t st7789_spi_write_dc_pin(uint8_t state)
{
    DISPLAY_GPIO_WRITE_DC((0U != state) ? CORE_GPIO_SET : CORE_GPIO_RESET);
    return ST7789_OK;
}

/**
 * @brief  Drive the panel RST line.
 *
 * @param[in] state : 0 -> reset asserted, non-zero -> released.
 *
 * @return ST7789_OK
 */
static st7789_status_t st7789_spi_write_rst_pin(uint8_t state)
{
    DISPLAY_GPIO_WRITE_RST((0U != state) ? CORE_GPIO_SET : CORE_GPIO_RESET);
    return ST7789_OK;
}

/* ---- Timebase / OS ------------------------------------------------------ */
/**
 * @brief  Monotonic ms tick provider — routed through MCU systick port so
 *         no HAL_* call leaks into the integration layer.
 *
 * @return Current ms tick.
 */
static uint32_t st7789_tb_get_tick_ms(void)
{
    return core_systick_get_ms();
}

/**
 * @brief  Blocking ms delay used by the driver for datasheet-mandated
 *         reset / SLPOUT timings (~120 ms).  Routes through OSAL so other
 *         tasks can run during the wait — this hook only fires after the
 *         scheduler is running.
 *
 * @param[in] ms : Milliseconds to wait.
 */
static void st7789_tb_delay_ms(uint32_t ms)
{
    osal_task_delay(ms);
}

/**
 * @brief  OS-aware delay used by the driver in cooperative paths.
 *
 * @param[in] ms : Milliseconds to wait.
 */
static void st7789_os_delay_ms(uint32_t ms)
{
    osal_task_delay(ms);
}

/* ---- Assembled interface vtables ----------------------------------------- */

static st7789_spi_interface_t s_spi_interface = {
    .pf_spi_init              = st7789_spi_init,
    .pf_spi_deinit            = st7789_spi_deinit,
    .pf_spi_transmit          = st7789_spi_transmit,
    .pf_spi_transmit_dma      = st7789_spi_transmit_dma,
    .pf_spi_wait_dma_complete = st7789_spi_wait_dma_complete,
    .pf_spi_write_cs_pin      = st7789_spi_write_cs_pin,
    .pf_spi_write_dc_pin      = st7789_spi_write_dc_pin,
    .pf_spi_write_rst_pin     = st7789_spi_write_rst_pin,
};

static st7789_timebase_interface_t s_timebase_interface = {
    .pf_get_tick_ms = st7789_tb_get_tick_ms,
    .pf_delay_ms    = st7789_tb_delay_ms,
};

static st7789_os_interface_t s_os_interface = {
    .pf_os_delay_ms = st7789_os_delay_ms,
};

/* ---- Driver input arg ---------------------------------------------------- */

st7789_driver_input_arg_t st7789_input_arg = {
    .panel =
        {
            .width    = ST7789_HAL_PANEL_WIDTH,
            .height   = ST7789_HAL_PANEL_HEIGHT,
            .x_offset = ST7789_HAL_PANEL_X_OFFSET,
            .y_offset = ST7789_HAL_PANEL_Y_OFFSET,
        },
    .p_spi_interface      = &s_spi_interface,
    .p_timebase_interface = &s_timebase_interface,
    .p_os_interface       = &s_os_interface,
};
//******************************* Functions *********************************//
