/******************************************************************************
 * @file ymodem_recv_task.c
 *
 * @par dependencies
 * - firmware_upgrade.h
 * - upgrade_service.h
 * - cfg_ota.h
 * - ymodem.h
 * - osal_wrapper_adapter.h / os_freertos.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief Producer half of the OTA pipeline. Waits for UPGRADE_EVENT_OTA_START
 *        (set by the magic-byte listener), drives one Ymodem_Receive session
 *        end-to-end, and on success stamps the OTA flag struct so the next
 *        boot triggers the bootloader's apply path.
 *
 * @version V1.0 2026-05-14
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>

#include "osal_wrapper_adapter.h"
#include "osal_error.h"
#include "os_freertos.h"

#include "firmware_upgrade.h"
#include "upgrade_service.h"
#include "cfg_ota.h"
#include "ymodem.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
 * @brief Linker-provided symbol: total bytes the current APP occupies in
 *        the internal-Flash slot. Defined in 01_APP/STM32F411XX_FLASH.ld as
 *        LOADADDR(.data) + SIZEOF(.data) - ORIGIN(FLASH). Read as
 *        (uint32_t)&__app_size__ — the symbol's *address* IS the value.
 *
 *        Stamped into ota_flag.current_app_size so the bootloader knows
 *        exactly how many bytes to back up into W25Q64 BLOCK_1 before
 *        flashing the new image.
 */
extern uint32_t __app_size__;
//******************************** Defines **********************************//

//******************************* Variables *********************************//
/**
 * @brief Ymodem double-buffer: two 1030-byte packet slots ping-pong'd by
 *        the producer (Ymodem_Receive) and consumer (firmware_upgrade_task)
 *        through Queue_AppDataBuffer + Semaphore_ExtFlashState handshake.
 *        BSS-allocated to avoid pushing 2 KB onto the task stack.
 */
static uint8_t s_ymodem_buf[2][1030];
//******************************* Variables *********************************//

//******************************* Functions *********************************//
extern osal_event_group_handle_t g_ota_evgrp;

void ymodem_recv_task(void *argument)
{
    (void)argument;

    DEBUG_OUT(i, YMODEM_LOG_TAG, "ymodem_recv_task entered");

    for (;;)
    {
        /**
        * Block forever waiting for the listener to set OTA_START. The
        * bit is cleared on take so we always run a fresh session.
        **/
        uint32_t bits = 0;
        (void)osal_event_group_wait_bits(g_ota_evgrp,
                                          UPGRADE_EVENT_OTA_START,
                                          true,   /* clear */
                                          false,  /* any   */
                                          OSAL_MAX_DELAY,
                                          &bits);
        if (0U == (bits & UPGRADE_EVENT_OTA_START))
        {
            continue;
        }

        DEBUG_OUT(i, YMODEM_LOG_TAG, "OTA download requested, awaiting Ymodem");

        /**
        * Ymodem_Receive runs to completion (or error) and returns the
        * total payload byte count on success. It is the one that arms
        * HAL_UARTEx_ReceiveToIdle_DMA on huart1, so by the time we get
        * here UART RX DMA is fully owned by Ymodem until it returns.
        **/
        const int32_t rx_result = Ymodem_Receive(s_ymodem_buf);

        if (rx_result <= 0)
        {
            DEBUG_OUT(e, YMODEM_LOG_TAG,
                      "Ymodem_Receive returned %d, OTA staging aborted",
                      (int)rx_result);
            (void)osal_event_group_set_bits(g_ota_evgrp, UPGRADE_EVENT_OTA_START);
            continue;
        }

        /**
        * Drain firmware_upgrade_task's 4 KB sector-aligned coalescing
        * buffer. Without this, the final sub-4KB chunk never reaches
        * W25Q64 and the bootloader either sees a truncated image or
        * reads 0xFF for the trailing bytes. See firmware_upgrade_task.c
        * for why the buffer exists.
        **/
        if (firmware_upgrade_flush_staged() < 0)
        {
            DEBUG_OUT(e, YMODEM_LOG_TAG,
                      "Final-sector flush failed, aborting stage");
            continue;
        }

        /**
        * Stamp the OTA flag struct so the next boot the bootloader sees
        * state=DOWNLOAD_FINISHED, reads image_size, and applies. The
        * apply event (UPGRADE_EVENT_OTA_APPLY → soft reset) is driven by
        * the listener separately so the user can verify the staged image
        * before committing to the reset.
        **/
        ota_flag_t f;
        if (ota_flag_read(&f) != 0)
        {
            f.magic = CFG_OTA_FLAG_MAGIC;
        }
        /**
        * Always restamp current_app_size from the linker symbol — the
        * running image's size IS what the bootloader needs for the
        * rollback backup, regardless of what value sat in the flag
        * sector from the previous OTA cycle.
        **/
        f.current_app_size = (uint32_t)&__app_size__;
        f.state            = CFG_OTA_DOWNLOAD_FINISHED;
        f.image_size       = (uint32_t)rx_result;

        if (ota_flag_write(&f) != 0)
        {
            DEBUG_OUT(e, YMODEM_LOG_TAG,
                      "ota_flag_write failed, image will NOT apply on boot");
            continue;
        }

        DEBUG_OUT(i, YMODEM_LOG_TAG,
                  "OTA staged: %ld bytes; awaiting apply trigger",
                  (long)rx_result);

        /**
        * Notify the listener: it was blocked on this bit since calling
        * firmware_upgrade_signal_start. Now it can resume polling UART1
        * for the apply magic (0x77 88 99).
        **/
        (void)osal_event_group_set_bits(g_ota_evgrp, UPGRADE_EVENT_OTA_STAGED);
    }
}
//******************************* Functions *********************************//
