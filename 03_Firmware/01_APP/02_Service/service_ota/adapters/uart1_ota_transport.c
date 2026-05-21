/******************************************************************************
 * @file uart1_ota_transport.c
 *
 * @par dependencies
 * - ota_transport.h
 * - firmware_upgrade.h
 * - mcu_uart_port.h
 *
 * @author Ethan-Hang
 *
 * @brief Service-layer adapter: implements ota_transport.h by binding to
 *        a specific MCU UART (here UART1) via mcu_uart_port. Zero HAL
 *        knowledge — every UART operation routes through the MCU port,
 *        which owns the HAL handle, ISR callbacks and OS sync primitives.
 *
 *        Also implements firmware_upgrade_signal_apply (system reset on
 *        apply magic). NVIC_SystemReset is the only MCU touch in this
 *        file; future work pulls it out into MCU_Core_Reset port.
 *
 *        Swapping OTA to e.g. UART6 is a 1-line change (replace
 *        OTA_UART_ID below with MCU_UART_6). Swapping OTA to BLE is a
 *        new adapter file that implements ota_transport.h directly,
 *        no UART involvement.
 *
 * @version V1.0 2026-05-18  v2 — adapter delegates to MCU_Core_UART
 *                            instead of touching HAL directly
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>

#include "stm32f4xx_hal.h"           /* NVIC_SystemReset only —
                                        will move to MCU_Core_Reset later */

#include "osal_wrapper_adapter.h"
#include "os_freertos.h"

#include "ota_transport.h"
#include "firmware_upgrade.h"
#include "mcu_uart_port.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
 * @brief Which MCU UART this OTA adapter binds to. Change one line to
 *        re-bind OTA to a different UART (provided that UART is enabled
 *        in CubeMX and mapped inside mcu_uart_port.c).
 */
#define OTA_UART_ID   MCU_UART_1
//******************************** Defines **********************************//

//******************************* Functions *********************************//
/**
* @brief Translate mcu_uart_status_t → ota_transport_status_t. Small
*        wrapper so the rest of this file can stay 1-line shims.
* */
static ota_transport_status_t translate(mcu_uart_status_t st)
{
    switch (st)
    {
    case MCU_UART_OK:      return OTA_TRANSPORT_OK;
    case MCU_UART_TIMEOUT: return OTA_TRANSPORT_TIMEOUT;
    case MCU_UART_BUSY:    return OTA_TRANSPORT_BUSY;
    case MCU_UART_ERR:
    case MCU_UART_INVALID:
    default:               return OTA_TRANSPORT_ERR;
    }
}

ota_transport_status_t ota_transport_init(void)
{
    return translate(mcu_uart_port_init(OTA_UART_ID));
}

/* ── listen-byte path ───────────────────────────────────────────────── */

ota_transport_status_t ota_transport_listen_byte_arm(void)
{
    return translate(mcu_uart_recv_byte_arm(OTA_UART_ID));
}

ota_transport_status_t ota_transport_listen_byte_wait(uint8_t *out,
                                                      uint32_t timeout_ms)
{
    return translate(mcu_uart_recv_byte_wait(OTA_UART_ID, out, timeout_ms));
}

/* ── frame path ─────────────────────────────────────────────────────── */

ota_transport_status_t ota_transport_frame_arm(uint8_t *buf, uint16_t maxlen)
{
    return translate(mcu_uart_recv_frame_arm(OTA_UART_ID, buf, maxlen));
}

ota_transport_status_t ota_transport_frame_wait(uint16_t *out_len,
                                                uint32_t timeout_ms)
{
    return translate(mcu_uart_recv_frame_wait(OTA_UART_ID, out_len,
                                               timeout_ms));
}

int ota_transport_frame_is_armed(void)
{
    return mcu_uart_recv_frame_is_armed(OTA_UART_ID);
}

ota_transport_status_t ota_transport_frame_stop(void)
{
    return translate(mcu_uart_recv_frame_abort(OTA_UART_ID));
}

/* ── TX ─────────────────────────────────────────────────────────────── */

ota_transport_status_t ota_transport_tx_byte(uint8_t b)
{
    return translate(mcu_uart_send_byte(OTA_UART_ID, b));
}

/* ── Board lifecycle (apply-magic → reset) ──────────────────────────── */

/**
* @brief Implementation of firmware_upgrade.h's apply hook. NVIC_System-
*        Reset is STM32-specific and not really a "transport" concern;
*        kept here for now because the OTA service has no other MCU-port
*        for system reset. Future cleanup: add MCU_Core_Reset port and
*        delegate (one-line change in this file).
* */
void firmware_upgrade_signal_apply(void)
{
    /**
    * Yield (not busy-wait) for ~50 ms so any in-flight RTT logs and the
    * final ACK byte on UART1 can drain before the core resets. Lower-
    * priority background tasks may also run during this window — that's
    * fine, we're about to reset anyway.
    **/
    osal_task_delay(OS_MS_TO_TICKS(50));
    NVIC_SystemReset();
}
//******************************* Functions *********************************//
