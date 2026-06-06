/******************************************************************************
 * @file ota_transport.h
 *
 * @par dependencies
 * - stdint.h
 *
 * @author Ethan-Hang
 *
 * @brief Platform-agnostic byte-stream transport for the OTA service.
 *
 *        Abstracts away the concrete UART / BLE / CAN / USB peripheral the
 *        OTA flow rides on. service_ota and the Ymodem middleware include
 *        ONLY this header — they never pull in stm32f4xx_hal.h or usart.h.
 *
 *        Binding is compile-time: exactly one .c file in 01_APP/
 *        Service_Adapters/ implements every function declared here.
 *        Swapping to a different transport ⇔ swapping which adapter .c
 *        is in the Makefile.
 *
 * ── Two reception modes (mutually exclusive at any instant) ─────────────
 *
 *   listen_byte_*  : 1-byte interrupt-driven receive, used for the magic-
 *                    byte slide-and-match before / after a Ymodem session.
 *                    Cheap, no DMA, doesn't tie up a buffer.
 *
 *   frame_*        : variable-length DMA-to-idle receive, used by Ymodem
 *                    for whole packets (1030 B = STX+SEQ+~SEQ+1024+CRC16).
 *                    Delivers the actual received byte count via *out_len
 *                    when the line goes idle or the buffer fills.
 *
 *        The transport guarantees that listen_byte and frame paths never
 *        race: the adapter switches the underlying peripheral between
 *        modes synchronously when the caller transitions.
 *
 * @version V1.0 2026-05-18
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __OTA_TRANSPORT_H__
#define __OTA_TRANSPORT_H__

//******************************** Includes *********************************//
#include "platform_type.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    OTA_TRANSPORT_OK      =  0,
    OTA_TRANSPORT_ERR     = -1,
    OTA_TRANSPORT_TIMEOUT = -2,
    OTA_TRANSPORT_BUSY    = -3,
} ota_transport_status_t;
//******************************** Defines **********************************//

//******************************* Functions *********************************//
/**
* @brief One-shot transport init. Called from user_init before any OTA
*        task is spawned. Creates the OS objects the adapter needs
*        (e.g. byte-arrival semaphore + frame-length queue) and configures
*        the peripheral. Idempotent.
*
* @return OTA_TRANSPORT_OK on success, OTA_TRANSPORT_ERR on resource
*         creation failure.
* */
ota_transport_status_t ota_transport_init(void);

/* ── Single-byte listen path (magic-scan) ───────────────────────────── */

/**
* @brief Arm one byte of interrupt-driven RX. After this returns, exactly
*        one byte's worth of capacity is enabled; the caller blocks via
*        ota_transport_listen_byte_wait() until that byte arrives.
*
*        Retries internally on transient BUSY (peripheral mid-state) —
*        yields the CPU between attempts, never spins.
*
* @return OTA_TRANSPORT_OK on successful arm.
* */
ota_transport_status_t ota_transport_listen_byte_arm(void);

/**
* @brief Block until the byte armed by ota_transport_listen_byte_arm()
*        arrives. After return the IT slot is consumed — caller must
*        re-arm before the next call.
*
* @param[out]  out         : received byte.
* @param[in]   timeout_ms  : max blocking time; pass OS_MS_MAX-equivalent
*                            (UINT32_MAX) for indefinite.
*
* @return OTA_TRANSPORT_OK on byte received, OTA_TRANSPORT_TIMEOUT on
*         expiry without a byte.
* */
ota_transport_status_t ota_transport_listen_byte_wait(UINT8_T *out,
                                                      UINT32_T timeout_ms);

/* ── Frame receive path (Ymodem DMA-idle) ───────────────────────────── */

/**
* @brief Arm a DMA-idle frame receive on the caller-supplied buffer. The
*        transport fills the buffer until either @p maxlen bytes are in
*        OR the line goes idle, then delivers the byte count via
*        ota_transport_frame_wait().
*
* @param[in]  buf    : caller-owned RX buffer. Must remain valid until
*                      the corresponding ota_transport_frame_wait()
*                      returns or ota_transport_frame_stop() is called.
* @param[in]  maxlen : buffer capacity. Anything beyond this is dropped.
*
* @return OTA_TRANSPORT_OK on successful arm.
* */
ota_transport_status_t ota_transport_frame_arm(UINT8_T *buf, UINT16_T maxlen);

/**
* @brief Block until a frame's worth of bytes arrives. Returns the
*        actual count via @p *out_len.
*
* @param[out] out_len    : actual bytes received before idle / fill.
* @param[in]  timeout_ms : max blocking time.
*
* @return OTA_TRANSPORT_OK on a frame, OTA_TRANSPORT_TIMEOUT on expiry.
* */
ota_transport_status_t ota_transport_frame_wait(UINT16_T *out_len,
                                                UINT32_T timeout_ms);

/**
* @brief Query whether the frame RX is currently armed. Used by Ymodem
*        to short-circuit the arm path when the user-handler pre-armed
*        the next buffer already (closes the ~2 ms re-arm gap that
*        causes F411 USART RDR overruns — see project memory
*        [[f411-usart-rdr-overrun]]).
*
* @return 1 if armed, 0 if idle.
* */
int ota_transport_frame_is_armed(void);

/**
* @brief Force-stop any pending frame RX. Used in error recovery before
*        re-arming with a fresh buffer (e.g. after a Ymodem packet that
*        failed length / SEQ / CRC validation).
*
* @return OTA_TRANSPORT_OK.
* */
ota_transport_status_t ota_transport_frame_stop(void);

/* ── TX (shared by both paths) ──────────────────────────────────────── */

/**
* @brief Polled blocking byte send. Returns after the byte has been
*        clocked out (peripheral TC flag set / equivalent).
*
* @param[in]  b : byte to transmit.
*
* @return OTA_TRANSPORT_OK on success.
* */
ota_transport_status_t ota_transport_tx_byte(UINT8_T b);

//******************************* Functions *********************************//

#endif /* __OTA_TRANSPORT_H__ */
