/******************************************************************************
 * @file uart1_ota_transport.c
 *
 * @par dependencies
 * - osal_wrapper_adapter.h / os_freertos.h
 * - stm32f4xx_hal.h, usart.h
 * - ota_transport.h
 *
 * @author Ethan-Hang
 *
 * @brief Board-specific implementation of ota_transport.h on top of USART1
 *        (HAL UART driver, NVIC IRQ via 01_APP/User_Isr_handlers/
 *        user_isr_handlers.c → HAL_UART_IRQHandler).
 *
 *        Owns the OS objects through which the HAL callbacks deliver RX
 *        events into the service / middleware:
 *
 *          - s_byte_sem     : binary sema, given by RxCpltCallback when the
 *                             1-byte IT-mode RX completes. Consumed by the
 *                             magic-byte scanner via ota_transport_listen_
 *                             byte_wait().
 *          - s_frame_queue  : length queue (uint16_t × 4), given by
 *                             RxEventCallback when the DMA-idle event
 *                             fires. Consumed by Ymodem via
 *                             ota_transport_frame_wait().
 *          - s_byte_buf     : single-byte staging cell for IT-mode RX.
 *
 *        Replacing this file with bleN_ota_transport.c (or similar) on a
 *        different MCU swaps the entire OTA transport with zero changes
 *        to service_ota / Ymodem source.
 *
 * @version V1.0 2026-05-18
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>

#include "osal_wrapper_adapter.h"
#include "osal_error.h"
#include "os_freertos.h"

#include "stm32f4xx_hal.h"
#include "usart.h"

#include "ota_transport.h"
#include "firmware_upgrade.h"   /* firmware_upgrade_signal_apply lives here
                                   too — see board-lifecycle section below */
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define OTA_TRANSPORT_FRAME_QUEUE_DEPTH   (4U)
//******************************** Defines **********************************//

//******************************* Variables *********************************//
/* RX path 1: single-byte IT-mode receive. RxCpltCallback fills s_byte_buf
   and gives s_byte_sem; the scanner task consumes both. */
static osal_sema_handle_t  s_byte_sem  = NULL;
static volatile uint8_t    s_byte_buf  = 0U;

/* RX path 2: DMA-idle frame receive. RxEventCallback pushes the byte count
   into s_frame_queue; Ymodem pops it as the per-packet length oracle. */
static osal_queue_handle_t s_frame_queue = NULL;
//******************************* Variables *********************************//

//******************************* Functions *********************************//
ota_transport_status_t ota_transport_init(void)
{
    if (NULL == s_byte_sem)
    {
        if (OSAL_SUCCESS != osal_sema_init(&s_byte_sem, 0))
        {
            return OTA_TRANSPORT_ERR;
        }
    }
    if (NULL == s_frame_queue)
    {
        if (OSAL_SUCCESS != osal_queue_create(&s_frame_queue,
                                              OTA_TRANSPORT_FRAME_QUEUE_DEPTH,
                                              sizeof(uint16_t)))
        {
            return OTA_TRANSPORT_ERR;
        }
    }
    return OTA_TRANSPORT_OK;
}

/* ── listen_byte path ───────────────────────────────────────────────── */

ota_transport_status_t ota_transport_listen_byte_arm(void)
{
    while (HAL_UART_Receive_IT(&huart1, (uint8_t *)&s_byte_buf, 1U) != HAL_OK)
    {
        /**
        * HAL returns BUSY if a previous RX is still active or the
        * peripheral hasn't drained from a Ymodem abort. Yield briefly
        * and retry — never spin-poll. ~5 ms gives the previous owner
        * (Ymodem DMA) time to release the bus.
        **/
        osal_task_delay(OS_MS_TO_TICKS(5));
    }
    return OTA_TRANSPORT_OK;
}

ota_transport_status_t ota_transport_listen_byte_wait(uint8_t *out,
                                                      uint32_t timeout_ms)
{
    if (NULL == out)
    {
        return OTA_TRANSPORT_ERR;
    }
    int32_t r = osal_sema_take(s_byte_sem, OS_MS_TO_TICKS(timeout_ms));
    if (OSAL_SUCCESS != r)
    {
        return OTA_TRANSPORT_TIMEOUT;
    }
    *out = s_byte_buf;
    return OTA_TRANSPORT_OK;
}

/* ── frame path ─────────────────────────────────────────────────────── */

ota_transport_status_t ota_transport_frame_arm(uint8_t *buf, uint16_t maxlen)
{
    if (NULL == buf || 0U == maxlen)
    {
        return OTA_TRANSPORT_ERR;
    }
    HAL_StatusTypeDef st = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, buf, maxlen);
    if (HAL_OK != st)
    {
        /* Most often happens because the previous RX hasn't released
           RxState. Fall back: hard-stop then re-arm. */
        HAL_UART_DMAStop(&huart1);
        st = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, buf, maxlen);
        if (HAL_OK != st)
        {
            return OTA_TRANSPORT_BUSY;
        }
    }
    /**
    * F411 HAL fires both HT and TC interrupts on DMA; we only care about
    * IDLE + TC. Killing HT cuts ~50% of unnecessary ISR entries during a
    * 1 KB packet (saves ~200 us of CPU per frame at 256 kbps).
    **/
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    return OTA_TRANSPORT_OK;
}

ota_transport_status_t ota_transport_frame_wait(uint16_t *out_len,
                                                uint32_t timeout_ms)
{
    if (NULL == out_len)
    {
        return OTA_TRANSPORT_ERR;
    }
    uint16_t len = 0U;
    int32_t  r   = osal_queue_receive(s_frame_queue, &len,
                                       OS_MS_TO_TICKS(timeout_ms));
    if (OSAL_SUCCESS != r)
    {
        return OTA_TRANSPORT_TIMEOUT;
    }
    *out_len = len;
    return OTA_TRANSPORT_OK;
}

int ota_transport_frame_is_armed(void)
{
    return (huart1.RxState == HAL_UART_STATE_BUSY_RX) ? 1 : 0;
}

ota_transport_status_t ota_transport_frame_stop(void)
{
    (void)HAL_UART_DMAStop(&huart1);
    return OTA_TRANSPORT_OK;
}

/* ── TX (shared) ────────────────────────────────────────────────────── */

ota_transport_status_t ota_transport_tx_byte(uint8_t b)
{
    /**
    * Blocking polled send. UART1 TX is in mode_TX_RX so RX DMA is not
    * affected by this. HAL_MAX_DELAY because at 256 kbps the byte takes
    * ~40 us — fast enough we don't need to yield to other tasks.
    **/
    (void)HAL_UART_Transmit(&huart1, &b, 1U, HAL_MAX_DELAY);
    return OTA_TRANSPORT_OK;
}

/* ── HAL callbacks (board glue) ─────────────────────────────────────── */

/**
* @brief USART1 1-byte interrupt-driven RX complete. Wakes the scanner
*        task that is parked in ota_transport_listen_byte_wait().
*
*        Only handles huart1; other UARTs (if added later) get their own
*        adapter file with their own callback override — HAL dispatches by
*        Instance.
* */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }
    if (NULL == s_byte_sem)
    {
        return;
    }
    osal_base_type_t higher_prio_woken = OSAL_FALSE;
    (void)osal_sema_give_from_isr(s_byte_sem, &higher_prio_woken);
    portYIELD_FROM_ISR(higher_prio_woken);
}

/**
* @brief USART1 DMA-idle event. Fires from DMA2_Stream5 / USART1 IRQ
*        context with @p Size = bytes received before the idle line.
*        Pushes the byte count to s_frame_queue which Ymodem pops via
*        ota_transport_frame_wait().
* */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART1)
    {
        return;
    }
    if (NULL == s_frame_queue)
    {
        return;
    }
    osal_base_type_t higher_prio_woken = OSAL_FALSE;
    (void)osal_queue_send_from_isr(s_frame_queue, &Size, &higher_prio_woken);
    portYIELD_FROM_ISR(higher_prio_woken);
}

/* ── Board lifecycle (apply-magic → reset) ──────────────────────────── */

/**
* @brief Implementation of the firmware_upgrade.h apply hook on this
*        board. NVIC_SystemReset is STM32-specific; lives in the board
*        adapter so service_ota stays MCU-agnostic.
*
*        Brief delay before reset lets any in-flight UART TX (log lines /
*        listener echo) drain. EasyLogger writes are async into RTT
*        rather than UART, so this is mostly defensive — 50 ms is well
*        within the 6 s IWDG window.
* */
void firmware_upgrade_signal_apply(void)
{
    HAL_Delay(50U);
    NVIC_SystemReset();
}
//******************************* Functions *********************************//
