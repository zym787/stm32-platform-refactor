/******************************************************************************
 * @file st7789_hal_test_task.c
 *
 * @par dependencies
 * - main.h (pin defines)
 * - spi.h  (hspi1)
 * - bsp_st7789_driver.h
 * - bsp_st7789_reg.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief ST7789 driver smoke test against real SPI1 hardware.
 *
 * Processing flow:
 * Mount 7 SPI vtable entries that forward directly to the CubeMX-generated
 * HAL_SPI_Transmit / HAL_SPI_Transmit_DMA / HAL_SPI_GetState / HAL_GPIO
 * primitives, skipping the MCU_Core_SPI platform layer for a shortest-path
 * bring-up.  Once the driver is instanced and initialized, loop a full-panel
 * fill_region through BLACK / RED / GREEN / BLUE / WHITE so the screen can
 * be verified visually.
 *
 * @note  pf_spi_wait_dma_complete is a tight poll on HAL_SPI_GetState with a
 *        HAL_GetTick timeout.  Each 512-byte tile stalls the task for ~80 us
 *        at 50 MHz SPI; acceptable for bring-up.  Swap to a FreeRTOS
 *        semaphore given in HAL_SPI_TxCpltCallback() once we care about the
 *        wasted cycles (300 tiles x 80 us = 24 ms per full-panel fill).
 *
 * @version V1.0 2026-04-24
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_type.h"
#include <stddef.h>
#include <stdbool.h>

#include "main.h"
#include "spi.h"
#include "stm32f4xx_hal.h"

#include "user_task_reso_config.h"
#include "bsp_st7789_driver.h"
#include "bsp_st7789_reg.h"
#include "platform_def.h"
#include "Debug.h"
//******************************** Includes *********************************//

/**
 * Whole-file gate. Default OFF — the task is never registered. Flip
 * USER_TASK_ST7789_HAL_TEST to 1 in user_task_reso_config.h to compile
 * the body (no g_user_task_cfg[] entry exists; this file is invoked
 * from manual experimentation only).
 */
#if USER_TASK_ST7789_HAL_TEST

//******************************** Defines **********************************//
#define ST7789_HAL_PANEL_WIDTH       240U
#define ST7789_HAL_PANEL_HEIGHT      320U
#define ST7789_HAL_PANEL_X_OFFSET    0U
#define ST7789_HAL_PANEL_Y_OFFSET    0U

/**
 * Mock task currently owns the first ~4 s after boot (2 s boot wait + six
 * cases with ~300 ms gaps).  The HAL task waits long enough for mock to
 * finish before touching the driver, since both share the driver's private
 * s_fill_tile_buf.  Lengthen this if mock cases grow.
 */
#define ST7789_HAL_BOOT_WAIT_MS      2000U
#define ST7789_HAL_COLOR_HOLD_MS     500U
#define ST7789_HAL_INVERT_HOLD_MS    120U

#define ST7789_HAL_SPI_TX_TIMEOUT_MS 100U
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static bsp_st7789_driver_t         s_hal_driver;
static st7789_spi_interface_t      s_hal_spi;
static st7789_timebase_interface_t s_hal_timebase;
static st7789_os_interface_t       s_hal_os;
static const st7789_panel_config_t s_hal_panel = {
    .width    = ST7789_HAL_PANEL_WIDTH,
    .height   = ST7789_HAL_PANEL_HEIGHT,
    .x_offset = ST7789_HAL_PANEL_X_OFFSET,
    .y_offset = ST7789_HAL_PANEL_Y_OFFSET,
};
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/* ---- SPI bus (HAL passthrough) ------------------------------------------ */
static st7789_status_t hal_spi_init(void)
{
    /* CubeMX MX_SPI1_Init() already ran from main() before the scheduler
     * started; nothing to do here.  Kept non-NULL so driver validation
     * accepts the interface. */
    return ST7789_OK;
}

static st7789_status_t hal_spi_deinit(void)
{
    return ST7789_OK;
}

static st7789_status_t hal_spi_transmit(UINT8_T const *p_data,
                                        UINT32_T       data_length)
{
    /* HAL caps Size at UINT16_T.  All blocking transfers in the driver are
     * short (command byte + up to ~16 B of register data), so the cast is
     * safe, but bail loudly if that ever stops being true. */
    if (data_length > UINT16_MAX)
    {
        return ST7789_ERRORPARAMETER;
    }

    HAL_StatusTypeDef hs =
        HAL_SPI_Transmit(&hspi1, (UINT8_T *)p_data, (UINT16_T)data_length,
                         ST7789_HAL_SPI_TX_TIMEOUT_MS);
    return (HAL_OK == hs) ? ST7789_OK : ST7789_ERROR;
}

static st7789_status_t hal_spi_transmit_dma(UINT8_T const *p_data,
                                            UINT32_T       data_length)
{
    if (data_length > UINT16_MAX)
    {
        return ST7789_ERRORPARAMETER;
    }

    HAL_StatusTypeDef hs =
        HAL_SPI_Transmit_DMA(&hspi1, (UINT8_T *)p_data, (UINT16_T)data_length);
    return (HAL_OK == hs) ? ST7789_OK : ST7789_ERROR;
}

static st7789_status_t hal_spi_wait_dma_complete(UINT32_T timeout_ms)
{
    /* After HAL_SPI_Transmit_DMA the SPI state machine sits in
     * HAL_SPI_STATE_BUSY_TX until the DMA2_Stream2 IRQ fires TxCpltCallback
     * which returns the state to READY.  Poll on that rather than on the
     * DMA TC flag directly -- the HAL callback also clears CR2_TXDMAEN and
     * waits for the SPI shift register to drain, which is exactly the
     * "safe to release CS" condition we need. */
    UINT32_T start = HAL_GetTick();
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY)
    {
        if ((HAL_GetTick() - start) > timeout_ms)
        {
            return ST7789_ERRORTIMEOUT;
        }
    }
    return ST7789_OK;
}

static st7789_status_t hal_spi_write_cs_pin(UINT8_T state)
{
    HAL_GPIO_WritePin(DISPALY_SPI1_CS_GPIO_Port, DISPALY_SPI1_CS_Pin,
                      (0U != state) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return ST7789_OK;
}

static st7789_status_t hal_spi_write_dc_pin(UINT8_T state)
{
    HAL_GPIO_WritePin(DISPALY_SPI1_DC_GPIO_Port, DISPALY_SPI1_DC_Pin,
                      (0U != state) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return ST7789_OK;
}

static st7789_status_t hal_spi_write_rst_pin(UINT8_T state)
{
    HAL_GPIO_WritePin(DISPALY_SPI1_RST_GPIO_Port, DISPALY_SPI1_RST_Pin,
                      (0U != state) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return ST7789_OK;
}

/* ---- Timebase / OS ------------------------------------------------------ */
static UINT32_T hal_tb_get_tick_ms(void)
{
    return HAL_GetTick();
}

static void hal_tb_delay_ms(UINT32_T ms)
{
    /* The driver calls this for datasheet-mandated reset / SLPOUT timings.
     * Using osal_task_delay (not HAL_Delay) lets other tasks run during
     * the ~120 ms waits; this task only runs after the scheduler is up. */
    osal_task_delay(ms);
}

static void hal_os_delay_ms(UINT32_T ms)
{
    osal_task_delay(ms);
}

/* ---- Wire-up ------------------------------------------------------------ */
static st7789_status_t hal_driver_bind(void)
{
    s_hal_spi.pf_spi_init              = hal_spi_init;
    s_hal_spi.pf_spi_deinit            = hal_spi_deinit;
    s_hal_spi.pf_spi_transmit          = hal_spi_transmit;
    s_hal_spi.pf_spi_transmit_dma      = hal_spi_transmit_dma;
    s_hal_spi.pf_spi_wait_dma_complete = hal_spi_wait_dma_complete;
    s_hal_spi.pf_spi_write_cs_pin      = hal_spi_write_cs_pin;
    s_hal_spi.pf_spi_write_dc_pin      = hal_spi_write_dc_pin;
    s_hal_spi.pf_spi_write_rst_pin     = hal_spi_write_rst_pin;

    s_hal_timebase.pf_get_tick_ms      = hal_tb_get_tick_ms;
    s_hal_timebase.pf_delay_ms         = hal_tb_delay_ms;

    s_hal_os.pf_os_delay_ms            = hal_os_delay_ms;

    return bsp_st7789_driver_inst(&s_hal_driver, &s_hal_spi, &s_hal_timebase,
                                  &s_hal_os, &s_hal_panel);
}

/* ---- Task entry --------------------------------------------------------- */
void st7789_hal_test_task(void *argument)
{
    (void)argument;

    /* Let the mock test task (and any other boot-time initialisers) finish
     * before we start talking to the real bus -- the driver keeps a single
     * static tile buffer that both tasks would otherwise race on. */
    osal_task_delay(ST7789_HAL_BOOT_WAIT_MS);

    DEBUG_OUT(i, ST7789_LOG_TAG, "st7789_hal_test_task start");

    st7789_status_t ret = hal_driver_bind();
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG, "hal driver bind failed = %d",
                  (int)ret);
        for (;;)
        {
            osal_task_delay(1000U);
        }
    }

    ret = s_hal_driver.pf_st7789_init(&s_hal_driver);
    if (ST7789_OK != ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG, "hal st7789_init failed = %d",
                  (int)ret);
        for (;;)
        {
            osal_task_delay(1000U);
        }
    }

    DEBUG_OUT(i, ST7789_LOG_TAG,
              "st7789_hal_test_task: panel ready, cycling colors");

    static const UINT16_T s_color_cycle[] = {BLACK, RED, GREEN, BLUE, WHITE};
    const UINT32_T cycle_len = ARRAY_SIZE(s_color_cycle);
    UINT32_T       idx       = 0U;

    // s_hal_driver.pf_st7789_fill_region(&s_hal_driver,
    //                               0U, 0U,
    //                               s_hal_panel.width - 1U,
    //                               s_hal_panel.height - 1U,
    //                               BLACK);
    s_hal_driver.pf_st7789_fill_color(&s_hal_driver, BLACK);

    for (;;)
    {
        const UINT16_T color    = s_color_cycle[idx];
        const UINT16_T fg_color = (BLACK == color) ? WHITE : color;
        do
        {
            ret = s_hal_driver.pf_st7789_fill_color(&s_hal_driver, BLACK);
            if (ST7789_OK != ret)
            {
                DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                          "hal draw fill_color failed = %d", (int)ret);
                break;
            }

            // ret = s_hal_driver.pf_st7789_draw_line(
            //     &s_hal_driver, 0U, 160U, s_hal_panel.width - 1U, 160U,
            //     color);
            // if (ST7789_OK != ret)
            // {
            //     DEBUG_OUT(e, ST7789_ERR_LOG_TAG, "hal draw line-h failed =
            //     %d",
            //               (int)ret);
            //     break;
            // }

            // ret = s_hal_driver.pf_st7789_draw_line(
            //     &s_hal_driver, 120U, 0U, 120U, s_hal_panel.height - 1U,
            //     color);
            // if (ST7789_OK != ret)
            // {
            //     DEBUG_OUT(e, ST7789_ERR_LOG_TAG, "hal draw line-v failed =
            //     %d",
            //               (int)ret);
            //     break;
            // }

            // ret = s_hal_driver.pf_st7789_draw_circle(&s_hal_driver, 120U,
            // 160U,
            //                                          70U, color);
            // if (ST7789_OK != ret)
            // {
            //     DEBUG_OUT(e, ST7789_ERR_LOG_TAG, "hal draw circle failed =
            //     %d",
            //               (int)ret);
            //     break;
            // }

            // ret = s_hal_driver.pf_st7789_draw_filled_rectangle(
            //     &s_hal_driver, 20U, 20U, 90U, 80U, color);
            // if (ST7789_OK != ret)
            // {
            //     DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
            //               "hal draw filled_rect failed = %d", (int)ret);
            //     break;
            // }

            // ret = s_hal_driver.pf_st7789_draw_filled_triangle(
            //     &s_hal_driver, 170U, 40U, 140U, 110U, 220U, 110U, color);
            // if (ST7789_OK != ret)
            // {
            //     DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
            //               "hal draw filled_triangle failed = %d", (int)ret);
            //     break;
            // }

            // ret = s_hal_driver.pf_st7789_draw_filled_circle(&s_hal_driver,
            // 60U,
            //                                                 250U, 30U,
            //                                                 color);
            // if (ST7789_OK != ret)
            // {
            //     DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
            //               "hal draw filled_circle failed = %d", (int)ret);
            //     break;
            // }

            {
                UINT16_T image_24x24[24U * 24U];

                for (UINT32_T row = 0U; row < 24U; row++)
                {
                    for (UINT32_T col = 0U; col < 24U; col++)
                    {
                        image_24x24[(row * 24U) + col] =
                            (((row + col) & 0x01U) != 0U) ? fg_color : BLUE;
                    }
                }

                ret = s_hal_driver.pf_st7789_draw_image(
                    &s_hal_driver, 108U, 120U, 24U, 24U, image_24x24);
                if (ST7789_OK != ret)
                {
                    DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                              "hal draw image failed = %d", (int)ret);
                    break;
                }
            }
            osal_task_delay(2000U);

            ret = s_hal_driver.pf_st7789_draw_char(&s_hal_driver, 108U, 152U,
                                                   'A', fg_color, BLUE);
            if (ST7789_OK != ret)
            {
                DEBUG_OUT(e, ST7789_ERR_LOG_TAG, "hal draw char failed = %d",
                          (int)ret);
                break;
            }
            osal_task_delay(2000U);


            ret = s_hal_driver.pf_st7789_draw_string(&s_hal_driver, 70U, 176U,
                                                     "HELLO", fg_color, BLUE);
            if (ST7789_OK != ret)
            {
                DEBUG_OUT(e, ST7789_ERR_LOG_TAG, "hal draw string failed = %d",
                          (int)ret);
                break;
            }
            osal_task_delay(2000U);

            // ret = s_hal_driver.pf_invert_colors(&s_hal_driver, 1U);
            // if (ST7789_OK != ret)
            // {
            //     DEBUG_OUT(e, ST7789_ERR_LOG_TAG, "hal invert on failed = %d",
            //               (int)ret);
            //     break;
            // }

            // osal_task_delay(ST7789_HAL_INVERT_HOLD_MS);

            // ret = s_hal_driver.pf_invert_colors(&s_hal_driver, 0U);
            // if (ST7789_OK != ret)
            // {
            //     DEBUG_OUT(e, ST7789_ERR_LOG_TAG, "hal invert off failed =
            //     %d",
            //               (int)ret);
            //     break;
            // }
        } while (0);

        DEBUG_OUT(i, ST7789_LOG_TAG, "shape cycle color=0x%04X ret=%d",
                  (unsigned)color, (int)ret);

        idx = (idx + 1U) % cycle_len;
        osal_task_delay(ST7789_HAL_COLOR_HOLD_MS);
    }
}

#endif /* USER_TASK_ST7789_HAL_TEST */
//******************************* Functions *********************************//
