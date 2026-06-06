/******************************************************************************
 * @file mcu_uart_port.h
 *
 * @par dependencies
 * - platform_type.h
 *
 * @author Ethan-Hang
 *
 * @brief MCU-level UART port abstraction.
 *
 *        Sibling of MCU_Core_IIC / MCU_Core_SPI / MCU_Core_IFlash etc.
 *        Hides the STM32 HAL (and the HAL_UART_*Callback ISR machinery)
 *        from upper layers; service-layer code links against this header
 *        and never touches stm32f4xx_hal_uart.h.
 *
 *        Two RX modes are exposed because they map to two genuinely
 *        different hardware paths on STM32 USART:
 *
 *          - byte mode  : HAL_UART_Receive_IT, one byte at a time.
 *                         Used by listeners that scan for magic bytes
 *                         and need cheap single-byte arrivals with no
 *                         DMA buffer tied up.
 *
 *          - frame mode : HAL_UARTEx_ReceiveToIdle_DMA, whole-frame
 *                         capture up to maxlen or until the line goes
 *                         idle. Used by streaming protocols (Ymodem,
 *                         AT command, etc.) where the boundary is the
 *                         idle gap, not a single character.
 *
 *        ISR dispatch:
 *
 *          The HAL UART RX callbacks (HAL_UART_RxCpltCallback and
 *          HAL_UARTEx_RxEventCallback) are defined ONCE inside
 *          mcu_uart_port.c and route by huart->Instance to per-UART
 *          internal state (sema + queue). Upper layers never see ISR
 *          context — they always go through the *_wait blocking APIs.
 *
 * @version V1.0 2026-05-18
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __MCU_UART_PORT_H__
#define __MCU_UART_PORT_H__

//******************************** Includes *********************************//
#include "platform_type.h"
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
 * @brief Logical UART identifiers. Each enum is bound to a concrete
 *        STM32 USART instance inside mcu_uart_port.c. Only USARTs that
 *        are actually wired up in CubeMX and listed here are usable.
 *
 *        Currently only MCU_UART_1 is mapped (USART1); add more when a
 *        second UART is brought up in the .ioc.
 */
typedef enum
{
    MCU_UART_1 = 0,
    MCU_UART_COUNT
} mcu_uart_id_t;
//******************************** Defines **********************************//

//******************************* Functions *********************************//
/**
* @brief One-shot port init. Allocates the internal byte-sema + frame
*        length queue for @p id, snapshots the corresponding HAL handle
*        pointer, and clears state. Idempotent.
*
* @param[in]  id : logical UART id (see mcu_uart_id_t).
*
* @return PLATFORM_OK on success, PLATFORM_ERR_PARAM on bad id,
*         PLATFORM_ERR_GENERAL on OS resource creation failure.
* */
platform_err_t mcu_uart_port_init(mcu_uart_id_t id);

/* ── byte mode (HAL_UART_Receive_IT, 1 B at a time) ─────────────────── */

/**
* @brief Arm one byte of interrupt-driven RX. Caller blocks via
*        mcu_uart_recv_byte_wait() until that byte arrives. Yields
*        the CPU on transient BUSY rather than spin-polling.
* */
platform_err_t mcu_uart_recv_byte_arm(mcu_uart_id_t id);

/**
* @brief Block until the byte previously armed arrives.
*
* @param[in]  id          : UART id.
* @param[out] out         : received byte.
* @param[in]  timeout_ms  : pass OSAL_MAX_DELAY for indefinite wait.
*
* @return PLATFORM_OK on byte received, PLATFORM_ERR_TIMEOUT on expiry.
* */
platform_err_t mcu_uart_recv_byte_wait(mcu_uart_id_t id,
                                           uint8_t      *out,
                                           uint32_t      timeout_ms);

/* ── frame mode (HAL_UARTEx_ReceiveToIdle_DMA) ──────────────────────── */

/**
* @brief Arm a DMA-idle frame receive on @p buf. The peripheral fills
*        @p buf until either @p maxlen bytes have arrived or the line
*        goes idle, then publishes the byte count to mcu_uart_recv_
*        frame_wait().
* */
platform_err_t mcu_uart_recv_frame_arm(mcu_uart_id_t id,
                                           uint8_t      *buf,
                                           uint16_t      maxlen);

/**
* @brief Block until a frame's worth of bytes is delivered.
*
* @param[in]  id          : UART id.
* @param[out] out_len     : actual bytes received.
* @param[in]  timeout_ms  : pass OSAL_MAX_DELAY for indefinite wait.
*
* @return PLATFORM_OK on a frame, PLATFORM_ERR_TIMEOUT on expiry.
* */
platform_err_t mcu_uart_recv_frame_wait(mcu_uart_id_t id,
                                            uint16_t     *out_len,
                                            uint32_t      timeout_ms);

/**
* @brief Query whether a frame RX is currently armed. Used by Ymodem
*        to skip the re-arm path when a user-handler pre-armed the
*        next buffer already.
*
* @return 1 if armed, 0 if idle.
* */
int mcu_uart_recv_frame_is_armed(mcu_uart_id_t id);

/**
* @brief Force-stop a pending frame RX. Used in error recovery before
*        re-arming with a fresh buffer.
* */
platform_err_t mcu_uart_recv_frame_abort(mcu_uart_id_t id);

/* ── TX ─────────────────────────────────────────────────────────────── */

/**
* @brief Polled blocking byte send. Returns after the byte has been
*        clocked out of the peripheral.
* */
platform_err_t mcu_uart_send_byte(mcu_uart_id_t id, uint8_t b);

//******************************* Functions *********************************//

#endif /* __MCU_UART_PORT_H__ */
