/******************************************************************************
 * @file ota_uart_listener.c
 *
 * @par dependencies
 * - ota_transport.h
 * - firmware_upgrade.h
 * - cfg_ota.h
 * - upgrade_service.h
 * - ymodem.h
 * - osal_wrapper_adapter.h / os_freertos.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief Single OTA orchestration task. Service-layer code — speaks only
 *        to ota_transport.h (abstract byte/frame I/O), never to HAL or a
 *        specific MCU.
 *
 *            ┌──────────────┐
 *            │ SCAN_START   │  ota_transport_listen_byte_*   slide-and-match
 *            │              │      0x11 22 33  ─►─┐
 *            └──────────────┘                     │
 *                                                 ▼
 *                                       ┌──────────────────┐
 *                                       │ YMODEM_ACTIVE    │  Ymodem_Receive
 *                                       │  + ota_flag write│  (frame mode)
 *                                       └────────┬─────────┘
 *                                                │ success
 *                                                ▼
 *            ┌──────────────┐
 *            │ SCAN_APPLY   │  ota_transport_listen_byte_*   slide-and-match
 *            │              │      0x77 88 99  → NVIC_SystemReset
 *            └──────────────┘
 *
 *        Producer / consumer split with firmware_upgrade_task is preserved:
 *        the Ymodem user-handler queues each FILE_DATA ctx pointer into
 *        g_otaDataQueue so W25Q64 writes happen in parallel with
 *        DMA reception.
 *
 *        The transport adapter (01_APP/Service_Adapters/uart1_ota_transport.c)
 *        guarantees that listen-byte (IT-mode single byte) and frame
 *        (DMA-idle frame) paths never race — switching between them is a
 *        synchronous handoff inside the adapter.
 *
 * @version V2.0 2026-05-14  merged listener + ymodem_recv → ota_service_task
 * @version V3.0 2026-05-18  decoupled from HAL UART — uses ota_transport
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include <string.h>
#include <stdint.h>

#include "osal_wrapper_adapter.h"
#include "osal_error.h"
#include "os_freertos.h"

#include "ota_transport.h"
#include "firmware_upgrade.h"
#include "upgrade_service.h"
#include "cfg_ota.h"
#include "ymodem.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* Magic-byte sequences shared with the PC-side OTA driver. */
static const uint8_t MAGIC_START[3] = { 0x11U, 0x22U, 0x33U };
static const uint8_t MAGIC_APPLY[3] = { 0x77U, 0x88U, 0x99U };
//******************************** Defines **********************************//

//******************************* Variables *********************************//
/**
 * @brief Linker-provided symbol: total bytes the current APP occupies in
 *        the internal-Flash slot. See STM32F411XX_FLASH.ld; the symbol's
 *        *address* IS the value. Stamped into ota_flag.current_app_size
 *        so the bootloader knows how many bytes to back up before flashing
 *        the new image.
 */
extern uint32_t                  __app_size__;

/**
 * @brief Ymodem double-buffer: two 1030-byte packet slots ping-pong'd by
 *        the producer (Ymodem_Receive) and consumer (firmware_upgrade_task)
 *        through Queue_AppDataBuffer + Semaphore_ExtFlashState handshake.
 *        BSS-allocated to avoid pushing 2 KB onto the task stack.
 */
static uint8_t                   s_ymodem_buf[2][1030];
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
* @brief Run one Ymodem download cycle and stamp the OTA flag on success.
*
* Called from the YMODEM_ACTIVE state. Returns 0 on success (image staged
* + ota_flag written), -1 on any failure inside the receive / flush / flag
* write path. Caller decides whether to re-scan or apply based on result.
* */
static int8_t ota_run_ymodem_session(void)
{
    DEBUG_OUT(i, YMODEM_LOG_TAG, "OTA download starting, awaiting Ymodem");

    const int32_t rx_result = Ymodem_Receive(s_ymodem_buf);

    if (rx_result <= 0)
    {
        DEBUG_OUT(e, YMODEM_LOG_TAG,
                  "Ymodem_Receive returned %d, OTA staging aborted",
                  (int)rx_result);
        return -1;
    }

    /**
    * Drain firmware_upgrade_task's 4 KB sector-aligned coalescing buffer.
    * Without this the final sub-4KB chunk never reaches storage.
    **/
    if (firmware_upgrade_flush_staged() < 0)
    {
        DEBUG_OUT(e, YMODEM_LOG_TAG,
                  "Final-sector flush failed, aborting stage");
        return -1;
    }

    /**
    * Stamp the OTA flag struct so the next boot the bootloader sees
    * state=DOWNLOAD_FINISHED, reads image_size, and applies.
    **/
    ota_flag_t f;
    if (ota_flag_read(&f) != 0)
    {
        f.magic = CFG_OTA_FLAG_MAGIC;
    }
    f.current_app_size = (uint32_t)&__app_size__;
    f.state            = CFG_OTA_DOWNLOAD_FINISHED;
    f.image_size       = (uint32_t)rx_result;

    if (ota_flag_write(&f) != 0)
    {
        DEBUG_OUT(e, YMODEM_LOG_TAG,
                  "ota_flag_write failed, image will NOT apply on boot");
        return -1;
    }

    DEBUG_OUT(i, YMODEM_LOG_TAG,
              "OTA staged: %ld bytes; awaiting apply trigger",
              (long)rx_result);
    return 0;
}

/**
* @brief Unified OTA orchestration task — scans the transport for start
*        magic, drives one Ymodem session end-to-end, then scans for apply
*        magic and resets. Doesn't know what underlying transport it's
*        running on — that's the adapter's job.
* */
void ota_service_task(void *argument)
{
    (void)argument;

    enum { SCAN_START, SCAN_APPLY } scan_state = SCAN_START;
    uint8_t window[3] = { 0U, 0U, 0U };

    DEBUG_OUT(i, YMODEM_LOG_TAG,
              "ota_service_task entered (scanning for start magic)");

    (void)ota_transport_listen_byte_arm();

    for (;;)
    {
        uint8_t rx_byte = 0U;
        if (OTA_TRANSPORT_OK !=
            ota_transport_listen_byte_wait(&rx_byte, OSAL_MAX_DELAY))
        {
            /* Timeout would only fire if the adapter reports it — we use
               OS_MS_MAX_DELAY so this branch is effectively unreachable
               on the production adapter, but defensive coding keeps the
               state machine robust if a future BLE transport surfaces
               idle timeouts here. */
            continue;
        }

        if (SCAN_START == scan_state)
        {
            if (slide_and_match(window, rx_byte, MAGIC_START))
            {
                DEBUG_OUT(i, YMODEM_LOG_TAG,
                          "OTA start magic seen, switching to Ymodem");
                memset(window, 0, sizeof(window));

                /**
                * UART RX is implicitly released to Ymodem here. The
                * adapter guarantees listen_byte_arm's IT slot was
                * consumed by the third magic byte's RxCpltCallback, so
                * Ymodem_Receive's first frame_arm sees the transport
                * idle and arms cleanly.
                **/
                if (0 == ota_run_ymodem_session())
                {
                    scan_state = SCAN_APPLY;
                    DEBUG_OUT(i, YMODEM_LOG_TAG,
                              "OTA staged, now scanning for apply magic");
                }
                else
                {
                    /* Stay in SCAN_START — let the user retry the upload. */
                    DEBUG_OUT(w, YMODEM_LOG_TAG,
                              "OTA session failed, rescanning for start magic");
                }
            }
        }
        else /* SCAN_APPLY */
        {
            if (slide_and_match(window, rx_byte, MAGIC_APPLY))
            {
                DEBUG_OUT(w, YMODEM_LOG_TAG,
                          "OTA apply magic seen, resetting");
                firmware_upgrade_signal_apply();  /* no return */
            }
        }

        /* Re-arm the next byte slot. */
        (void)ota_transport_listen_byte_arm();
    }
}
//******************************* Functions *********************************//
