/******************************************************************************
 * @file mcu_uart_port.c
 *
 * @par dependencies
 * - osal_wrapper_adapter.h / os_freertos.h
 * - stm32f4xx_hal.h, usart.h
 * - mcu_uart_port.h
 *
 * @author Ethan-Hang
 *
 * @brief STM32 HAL implementation of mcu_uart_port.h. Hosts:
 *
 *          - per-UART state table (HAL handle ptr + byte sema +
 *            frame-length queue + 1 B IT-mode receive cell);
 *          - HAL_UART_RxCpltCallback / HAL_UARTEx_RxEventCallback
 *            dispatch-by-Instance (defined ONCE in this file because
 *            HAL routes callbacks by name, regardless of which UART
 *            triggered);
 *          - the USART1_IRQHandler vector handler (moved out of
 *            01_APP/User_Isr_handlers — vector handlers for managed
 *            peripherals live in the MCU port layer).
 *
 *        Upper layers (service / middleware) never see ISR context
 *        nor a HAL_UART_HandleTypeDef — they call the *_arm / *_wait
 *        blocking APIs declared in mcu_uart_port.h.
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

#include "mcu_uart_port.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define MCU_UART_FRAME_QUEUE_DEPTH   (4U)
//******************************** Defines **********************************//

//******************************* Variables *********************************//
typedef struct
{
    UART_HandleTypeDef *hal_handle;   /* &huart1 / ... (or NULL if not init) */
    osal_sema_handle_t  byte_sem;     /* given by HAL_UART_RxCpltCallback     */
    osal_queue_handle_t frame_queue;  /* fed by HAL_UARTEx_RxEventCallback    */
    volatile uint8_t    byte_buf;     /* 1 B IT-mode receive cell             */
} mcu_uart_state_t;

static mcu_uart_state_t s_state[MCU_UART_COUNT];
//******************************* Variables *********************************//

//******************************* Functions *********************************//
/**
* @brief Look up the per-UART state row that owns the given HAL handle.
*        Returns NULL if the handle has not been bound by
*        mcu_uart_port_init.
* */
static mcu_uart_state_t *state_for_handle(UART_HandleTypeDef *huart)
{
    for (int i = 0; i < (int)MCU_UART_COUNT; i++)
    {
        if (s_state[i].hal_handle == huart)
        {
            return &s_state[i];
        }
    }
    return NULL;
}

/* ── init ───────────────────────────────────────────────────────────── */

mcu_uart_status_t mcu_uart_port_init(mcu_uart_id_t id)
{
    if (id >= MCU_UART_COUNT)
    {
        return MCU_UART_INVALID;
    }

    /* Snapshot the HAL handle pointer for ISR-side dispatch. Bind here
       so we don't include usart.h in every header that needs the enum. */
    switch (id)
    {
    case MCU_UART_1:
        s_state[id].hal_handle = &huart1;
        break;
    default:
        return MCU_UART_INVALID;
    }

    if (NULL == s_state[id].byte_sem)
    {
        if (OSAL_SUCCESS != osal_sema_init(&s_state[id].byte_sem, 0))
        {
            return MCU_UART_ERR;
        }
    }
    if (NULL == s_state[id].frame_queue)
    {
        if (OSAL_SUCCESS != osal_queue_create(&s_state[id].frame_queue,
                                              MCU_UART_FRAME_QUEUE_DEPTH,
                                              sizeof(uint16_t)))
        {
            return MCU_UART_ERR;
        }
    }
    return MCU_UART_OK;
}

/* ── byte path ──────────────────────────────────────────────────────── */

mcu_uart_status_t mcu_uart_recv_byte_arm(mcu_uart_id_t id)
{
    if (id >= MCU_UART_COUNT || NULL == s_state[id].hal_handle)
    {
        return MCU_UART_INVALID;
    }
    while (HAL_UART_Receive_IT(s_state[id].hal_handle,
                                (uint8_t *)&s_state[id].byte_buf, 1U)
           != HAL_OK)
    {
        /**
        * HAL returns BUSY if a previous RX is still active or the
        * peripheral hasn't drained from a Ymodem abort. Yield briefly
        * and retry — never spin-poll. ~5 ms gives the previous owner
        * (frame-mode DMA) time to release the bus.
        **/
        osal_task_delay(OS_MS_TO_TICKS(5));
    }
    return MCU_UART_OK;
}

mcu_uart_status_t mcu_uart_recv_byte_wait(mcu_uart_id_t id,
                                           uint8_t      *out,
                                           uint32_t      timeout_ms)
{
    if (id >= MCU_UART_COUNT || NULL == out)
    {
        return MCU_UART_INVALID;
    }
    if (OSAL_SUCCESS != osal_sema_take(s_state[id].byte_sem,
                                        OS_MS_TO_TICKS(timeout_ms)))
    {
        return MCU_UART_TIMEOUT;
    }
    *out = s_state[id].byte_buf;
    return MCU_UART_OK;
}

/* ── frame path ─────────────────────────────────────────────────────── */

mcu_uart_status_t mcu_uart_recv_frame_arm(mcu_uart_id_t id,
                                           uint8_t      *buf,
                                           uint16_t      maxlen)
{
    if (id >= MCU_UART_COUNT || NULL == buf || 0U == maxlen)
    {
        return MCU_UART_INVALID;
    }
    UART_HandleTypeDef *h = s_state[id].hal_handle;
    HAL_StatusTypeDef st  = HAL_UARTEx_ReceiveToIdle_DMA(h, buf, maxlen);
    if (HAL_OK != st)
    {
        /* Most often happens because the previous RX hasn't released
           RxState. Hard-stop then retry once. */
        HAL_UART_DMAStop(h);
        st = HAL_UARTEx_ReceiveToIdle_DMA(h, buf, maxlen);
        if (HAL_OK != st)
        {
            return MCU_UART_BUSY;
        }
    }
    /**
    * F411 HAL fires both half-transfer and transfer-complete DMA IRQs;
    * we only care about IDLE + TC. Killing HT cuts ~50% of unnecessary
    * ISR entries during a 1 KB packet (saves ~200 us of CPU per frame
    * at 256 kbps).
    **/
    __HAL_DMA_DISABLE_IT(h->hdmarx, DMA_IT_HT);
    return MCU_UART_OK;
}

mcu_uart_status_t mcu_uart_recv_frame_wait(mcu_uart_id_t id,
                                            uint16_t     *out_len,
                                            uint32_t      timeout_ms)
{
    if (id >= MCU_UART_COUNT || NULL == out_len)
    {
        return MCU_UART_INVALID;
    }
    uint16_t len = 0U;
    if (OSAL_SUCCESS != osal_queue_receive(s_state[id].frame_queue, &len,
                                            OS_MS_TO_TICKS(timeout_ms)))
    {
        return MCU_UART_TIMEOUT;
    }
    *out_len = len;
    return MCU_UART_OK;
}

int mcu_uart_recv_frame_is_armed(mcu_uart_id_t id)
{
    if (id >= MCU_UART_COUNT || NULL == s_state[id].hal_handle)
    {
        return 0;
    }
    return (s_state[id].hal_handle->RxState == HAL_UART_STATE_BUSY_RX)
            ? 1 : 0;
}

mcu_uart_status_t mcu_uart_recv_frame_abort(mcu_uart_id_t id)
{
    if (id >= MCU_UART_COUNT || NULL == s_state[id].hal_handle)
    {
        return MCU_UART_INVALID;
    }
    (void)HAL_UART_DMAStop(s_state[id].hal_handle);
    return MCU_UART_OK;
}

/* ── TX ─────────────────────────────────────────────────────────────── */

mcu_uart_status_t mcu_uart_send_byte(mcu_uart_id_t id, uint8_t b)
{
    if (id >= MCU_UART_COUNT || NULL == s_state[id].hal_handle)
    {
        return MCU_UART_INVALID;
    }
    /**
    * Blocking polled send. UART1 TX in mode_TX_RX so RX DMA is not
    * affected by this. HAL_MAX_DELAY because at 256 kbps the byte
    * takes ~40 us — fast enough we don't need to yield to other tasks.
    **/
    (void)HAL_UART_Transmit(s_state[id].hal_handle, &b, 1U, HAL_MAX_DELAY);
    return MCU_UART_OK;
}

/* ── HAL ISR callbacks (defined ONCE for the whole project) ─────────── */

/**
* @brief HAL byte-mode RX complete. HAL routes callbacks by name, so
*        this single definition handles every UART; we dispatch by
*        huart->Instance to the matching mcu_uart_state_t row.
* */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    mcu_uart_state_t *st = state_for_handle(huart);
    if (NULL == st || NULL == st->byte_sem)
    {
        return;
    }
    osal_base_type_t higher_prio_woken = OSAL_FALSE;
    (void)osal_sema_give_from_isr(st->byte_sem, &higher_prio_woken);
    portYIELD_FROM_ISR(higher_prio_woken);
}

/**
* @brief HAL DMA-idle frame event. @p Size = bytes received before the
*        idle line was detected (or before the buffer filled). Same
*        dispatch model as RxCpltCallback above.
* */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    mcu_uart_state_t *st = state_for_handle(huart);
    if (NULL == st || NULL == st->frame_queue)
    {
        return;
    }
    osal_base_type_t higher_prio_woken = OSAL_FALSE;
    (void)osal_queue_send_from_isr(st->frame_queue, &Size,
                                    &higher_prio_woken);
    portYIELD_FROM_ISR(higher_prio_woken);
}

/* ── NVIC vector handlers ───────────────────────────────────────────── */

/**
* @brief USART1 global interrupt vector. Moved from 01_APP/
*        User_Isr_handlers/ — vector handlers for managed peripherals
*        live in their MCU port file. HAL dispatches RXNE / IDLE /
*        error inside HAL_UART_IRQHandler to the callbacks above.
*
*        DMA2_Stream5 (USART1 RX DMA) IRQ stays in
*        Core/Src/stm32f4xx_it.c — that handler is CubeMX-generated
*        from the .ioc when UART RX DMA is enabled.
* */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
//******************************* Functions *********************************//
