/******************************************************************************
 * @file ota_uart_listener.c
 *
 * @par dependencies
 * - firmware_upgrade.h
 * - usart.h, stm32f4xx_hal.h
 * - FreeRTOS.h, event_groups.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief OTA trigger source. Polls UART1 for two magic-byte sequences:
 *
 *          0x11 0x22 0x33  →  signal ymodem_recv_task to start staging
 *          0x77 0x88 0x99  →  apply the staged image (NVIC_SystemReset)
 *
 *        UART1 ownership rules:
 *          - In IDLE state the listener owns UART1 RX via 1-byte polled
 *            HAL_UART_Receive with a short timeout.
 *          - When the start magic matches, the listener BLOCKS on
 *            UPGRADE_EVENT_OTA_STAGED so it doesn't fight Ymodem_Receive
 *            (which arms HAL_UARTEx_ReceiveToIdle_DMA on the same UART).
 *          - After the staged bit is set, ymodem_recv_task is back in
 *            its wait-for-OTA_START loop and the listener resumes
 *            ownership, this time scanning for the apply magic.
 *
 * @version V1.0 2026-05-14
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include <string.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "event_groups.h"
#include "semphr.h"

#include "stm32f4xx_hal.h"
#include "usart.h"

#include "firmware_upgrade.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* Magic-byte sequences shared with the PC-side OTA driver. */
static const uint8_t MAGIC_START[3] = { 0x11U, 0x22U, 0x33U };
static const uint8_t MAGIC_APPLY[3] = { 0x77U, 0x88U, 0x99U };
//******************************** Defines **********************************//

//******************************* Variables *********************************//
extern EventGroupHandle_t g_ota_evgrp;
extern SemaphoreHandle_t  g_listener_byte_sem;
extern uint8_t            g_listener_rx_byte;
//******************************* Variables *********************************//

//******************************* Functions *********************************//
/**
* @brief Slide a fresh byte into a 3-byte window and compare to a target.
*
* @param[in,out] window : 3-byte sliding window; LSB-most byte is the
*                         newest. Caller seeds it to zero before first call.
* @param[in]     byte   : freshly received byte.
* @param[in]     target : 3-byte target sequence.
*
* @return true if `window` now equals `target`, else false.
* */
static bool slide_and_match(uint8_t window[3], uint8_t byte,
                            const uint8_t target[3])
{
    window[0] = window[1];
    window[1] = window[2];
    window[2] = byte;
    return (window[0] == target[0]) &&
           (window[1] == target[1]) &&
           (window[2] == target[2]);
}

/**
* @brief Arm one byte of interrupt-driven RX. Quietly retries on BUSY —
*        that means HAL is still in the previous RX or Ymodem grabbed the
*        bus, both transient.
* */
static void arm_listener_rx(void)
{
    while (HAL_UART_Receive_IT(&huart1, &g_listener_rx_byte, 1U) != HAL_OK)
    {
        /**
        * HAL returns BUSY if a previous RX is still active or if the
        * peripheral hasn't drained from a Ymodem Abort. Yield briefly
        * and retry — never spin-poll. ~5 ms gives Ymodem time to release
        * the bus on the abort path.
        **/
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void ota_uart_listener_task(void *argument)
{
    (void)argument;

    enum { LISTEN_FOR_START, LISTEN_FOR_APPLY } state = LISTEN_FOR_START;
    uint8_t window[3] = { 0U, 0U, 0U };

    DEBUG_OUT(i, YMODEM_LOG_TAG,
              "ota_uart_listener_task entered (scanning for start magic)");

    arm_listener_rx();

    for (;;)
    {
        /**
        * Block on the binary semaphore the RxCpltCallback gives. CPU
        * yields cleanly here — listener no longer starves PRI_BACKGROUND
        * tasks like iwdg_feeder, which is what was tripping IWDG and
        * making the system look frozen.
        **/
        if (xSemaphoreTake(g_listener_byte_sem, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        const uint8_t rx_byte = g_listener_rx_byte;

        if (LISTEN_FOR_START == state)
        {
            if (slide_and_match(window, rx_byte, MAGIC_START))
            {
                DEBUG_OUT(i, YMODEM_LOG_TAG,
                          "OTA start magic seen, signalling ymodem_recv");

                memset(window, 0, sizeof(window));
                (void)firmware_upgrade_signal_start();

                /**
                * Block on STAGED — UART1 RX is implicitly released to
                * Ymodem here. We do NOT re-arm HAL_UART_Receive_IT until
                * Ymodem is done, otherwise IT and ReceiveToIdle_DMA fight
                * over the same peripheral. ymodem_recv_task calls
                * HAL_UARTEx_ReceiveToIdle_DMA from inside Ymodem_Receive;
                * once that returns and ota_flag is stamped, it sets the
                * STAGED bit and the bus is free again.
                **/
                (void)xEventGroupWaitBits(g_ota_evgrp,
                                          UPGRADE_EVENT_OTA_STAGED,
                                          pdTRUE,   /* clear on take */
                                          pdFALSE,  /* any bit       */
                                          portMAX_DELAY);

                state = LISTEN_FOR_APPLY;
                DEBUG_OUT(i, YMODEM_LOG_TAG,
                          "OTA staged, now scanning for apply magic");
            }
        }
        else /* LISTEN_FOR_APPLY */
        {
            if (slide_and_match(window, rx_byte, MAGIC_APPLY))
            {
                DEBUG_OUT(w, YMODEM_LOG_TAG,
                          "OTA apply magic seen, resetting");
                firmware_upgrade_signal_apply();  /* no return */
            }
        }

        /* Re-arm one-byte IT for the next character. */
        arm_listener_rx();
    }
}
//******************************* Functions *********************************//
