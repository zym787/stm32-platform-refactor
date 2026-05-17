/******************************************************************************
 * @file firmware_upgrade_task.c
 *
 * @par dependencies
 * - firmware_upgrade.h
 * - ymodem.h
 * - user_externflash_manage.h
 * - upgrade_service.h
 * - osal_wrapper_adapter.h / os_freertos.h
 * - usart.h, stm32f4xx_hal.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief Consumer half of the OTA pipeline + the three global OS objects
 *        Ymodem (02_Middleware_Platform/Ymodem/src/ymodem.c) externs:
 *          Q_YmodemReclength     — uint16_t per-frame RX length from DMA-idle
 *          Queue_AppDataBuffer   — Ymodem_RxContext_t * per Ymodem packet
 *          Semaphore_ExtFlashState — back-pressure ack to Ymodem producer
 *
 *        Also hosts HAL_UARTEx_RxEventCallback (the DMA-idle bottom half
 *        that feeds Q_YmodemReclength from ISR context).
 *
 *        Does NOT drive Ymodem_Receive itself — that lives in
 *        ymodem_recv_task.c so the consumer can stay a pure drain loop.
 *
 * @version V1.0 2026-05-14
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include <string.h>

#include "osal_wrapper_adapter.h"
#include "osal_error.h"
#include "os_freertos.h"

#include "stm32f4xx_hal.h"
#include "usart.h"

#include "firmware_upgrade.h"
#include "ymodem.h"
#include "user_externflash_manage.h"
#include "upgrade_service.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define UPGRADE_QUEUE_LEN_FRAMES   (4U)
#define UPGRADE_QUEUE_DATA_DEPTH   (2U)   /* matches Ymodem double-buffer  */

/**
 * @brief Sector-buffer size. W25Q64's bsp driver (w25q64_write_data_erase)
 *        erases the full 4 KB sector containing the target address on
 *        EVERY write call. Two consecutive 1024-byte Ymodem packets that
 *        land in the same sector would otherwise wipe each other. Buffer
 *        a full sector in RAM and issue one Write_OtaData per sector to
 *        keep writes non-overlapping with their own erases.
 */
#define UPGRADE_SECTOR_BUF_SIZE    (4096U)
//******************************** Defines **********************************//

//******************************* Variables *********************************//
/**
 * @brief Globals exported to Ymodem via extern. Definitions live here so
 *        the producer (Ymodem in 02_Middleware_Platform) does not also try
 *        to own the lifetime.
 */
osal_queue_handle_t     Q_YmodemReclength       = NULL;
osal_queue_handle_t     Queue_AppDataBuffer     = NULL;
osal_sema_handle_t      Semaphore_ExtFlashState = NULL;

/* OTA orchestration event group, owned by this module — distinct from the
   storage_manager's event group (different bit space, different consumer). */
osal_event_group_handle_t g_ota_evgrp           = NULL;

/* Listener-only 1-byte RX path. Magic-byte scan uses interrupt-driven UART
   so the task blocks on this sema instead of busy-polling — otherwise the
   listener starves PRI_BACKGROUND tasks like iwdg_feeder and IWDG fires. */
osal_sema_handle_t g_listener_byte_sem    = NULL;
uint8_t           g_listener_rx_byte     = 0U;

/* Sector-aligned write coalescer. Owned by firmware_upgrade_task; flushed
   by firmware_upgrade_flush_staged() at session end. BSS-allocated to keep
   the task stack small. Access is single-producer (firmware_upgrade_task
   processes Queue_AppDataBuffer serially; the flush call from
   ymodem_recv_task only runs AFTER Ymodem_Receive returns, by which point
   firmware_upgrade_task has finished the last ctx and is parked on the
   queue) so no mutex is needed. */
static uint8_t   s_sector_buf[UPGRADE_SECTOR_BUF_SIZE];
static uint32_t  s_sector_buf_used   = 0U;
static uint32_t  s_session_bytes_out = 0U;
/* OTA-partition write cursor; advances per flushed 4 KB sector. File-scope
   so firmware_upgrade_flush_staged() (called from ymodem_recv_task) can
   reach it after firmware_upgrade_task has parked on Queue_AppDataBuffer. */
static uint32_t  s_ota_write_offset  = 0U;
//******************************* Variables *********************************//

//******************************* Functions *********************************//
int8_t firmware_upgrade_resources_init(void)
{
    if (NULL == Q_YmodemReclength)
    {
        if (OSAL_SUCCESS != osal_queue_create(&Q_YmodemReclength,
                                              UPGRADE_QUEUE_LEN_FRAMES,
                                              sizeof(uint16_t)))
        {
            return -1;
        }
    }

    if (NULL == Queue_AppDataBuffer)
    {
        if (OSAL_SUCCESS != osal_queue_create(&Queue_AppDataBuffer,
                                              UPGRADE_QUEUE_DATA_DEPTH,
                                              sizeof(Ymodem_RxContext_t *)))
        {
            return -1;
        }
    }

    if (NULL == Semaphore_ExtFlashState)
    {
        /**
        * Binary semaphore, starts EMPTY: Ymodem's first packet must wait
        * for consumer to ack before reusing the double-buffer.
        **/
        if (OSAL_SUCCESS != osal_sema_init(&Semaphore_ExtFlashState, 0))
        {
            return -1;
        }
    }

    if (NULL == g_ota_evgrp)
    {
        if (OSAL_SUCCESS != osal_event_group_create(&g_ota_evgrp))
        {
            return -1;
        }
    }

    if (NULL == g_listener_byte_sem)
    {
        if (OSAL_SUCCESS != osal_sema_init(&g_listener_byte_sem, 0))
        {
            return -1;
        }
    }

    return 0;
}

int8_t firmware_upgrade_signal_start(void)
{
    if (NULL == g_ota_evgrp)
    {
        return -1;
    }
    (void)osal_event_group_set_bits(g_ota_evgrp, UPGRADE_EVENT_OTA_START);
    return 0;
}

int32_t firmware_upgrade_flush_staged(void)
{
    if (0U == s_sector_buf_used)
    {
        /* Already aligned — nothing left to flush. */
        return (int32_t)s_session_bytes_out;
    }

    /**
    * Pad the remainder of the partial sector with 0xFF so the encrypted
    * tail of the .mxxx doesn't carry garbage past the declared image
    * size. Bootloader's exA_to_exB_AES only decrypts up to app_size
    * bytes anyway, so the padding is harmless either way.
    **/
    memset(&s_sector_buf[s_sector_buf_used], 0xFFU,
           UPGRADE_SECTOR_BUF_SIZE - s_sector_buf_used);

    ext_flash_status_t st = Write_OtaData(s_ota_write_offset,
                                          UPGRADE_SECTOR_BUF_SIZE,
                                          s_sector_buf);
    if (EXT_FLASH_OK != st)
    {
        DEBUG_OUT(e, YMODEM_DATA_LOG_TAG,
                  "OTA W25Q64 final sector flush failed at offset=%lu err=%d",
                  (unsigned long)s_ota_write_offset, (int)st);
        s_sector_buf_used = 0U;
        return -1;
    }

    /**
    * Account only the actual session bytes (not the 0xFF pad). The
    * bootloader records image_size = bytes_received (the Ymodem-declared
    * count, equal to the .mxxx file length), and reads only that many
    * bytes from W25Q64 during decrypt.
    **/
    s_session_bytes_out += s_sector_buf_used;
    s_sector_buf_used    = 0U;
    s_ota_write_offset  += UPGRADE_SECTOR_BUF_SIZE;

    return (int32_t)s_session_bytes_out;
}

void firmware_upgrade_signal_apply(void)
{
    /**
    * Tiny delay before reset to let any in-flight UART TX (log lines /
    * the listener echo) drain. EasyLogger writes are async into RTT, not
    * UART, so this is mostly defensive — 50 ms is well within IWDG window.
    **/
    HAL_Delay(50U);
    NVIC_SystemReset();
}

/**
* @brief Consumer of Ymodem_RxContext_t pointers — writes each FILE_DATA
*        payload to the W25Q64 OTA partition and unblocks the Ymodem
*        producer via Semaphore_ExtFlashState. FILE_INFO ctx skips the
*        write but still acks so Ymodem can flip buffers.
*
* @param[in] argument : ignored.
* @param[out] : None.
* @return None.
* */
void firmware_upgrade_task(void *argument)
{
    (void)argument;

    DEBUG_OUT(i, YMODEM_LOG_TAG, "firmware_upgrade_task entered");

    for (;;)
    {
        Ymodem_RxContext_t *ctx = NULL;

        if (OSAL_SUCCESS != osal_queue_receive(Queue_AppDataBuffer, &ctx, OSAL_MAX_DELAY))
        {
            continue;
        }
        if (NULL == ctx)
        {
            continue;
        }

        if (YMODEM_RX_STATE_FILE_INFO == ctx->state)
        {
            /**
            * FILE_INFO block: header containing filename + ASCII size.
            * Reset the staging cursor + sector coalescer. The actual
            * declared size is in ctx->size (parsed by Ymodem's Str2Int)
            * — we don't need to act on it here.
            **/
            s_ota_write_offset  = 0U;
            s_sector_buf_used   = 0U;
            s_session_bytes_out = 0U;
            DEBUG_OUT(i, YMODEM_FILE_LOG_TAG,
                      "OTA session start, declared size=%d B", (int)ctx->size);
        }
        else if (YMODEM_RX_STATE_FILE_DATA == ctx->state)
        {
            /**
            * FILE_DATA block. Ymodem trimmed packet_length to actual
            * payload bytes already. Payload sits past PACKET_HEADER.
            *
            * Drip the bytes into s_sector_buf and only call Write_OtaData
            * when we have a full 4 KB sector — the underlying driver
            * erases the whole sector per write, so partial-sector writes
            * would wipe out preceding partial-sector writes in the same
            * sector. ymodem_recv_task calls firmware_upgrade_flush_staged
            * at session end to drain the final partial sector.
            **/
            uint32_t        bytes = (uint32_t)ctx->packet_length;
            const uint8_t  *src   = ctx->packet_data + PACKET_HEADER;

            while (bytes > 0U)
            {
                const uint32_t cap = UPGRADE_SECTOR_BUF_SIZE - s_sector_buf_used;
                const uint32_t take = (bytes < cap) ? bytes : cap;

                memcpy(&s_sector_buf[s_sector_buf_used], src, take);
                s_sector_buf_used += take;
                src               += take;
                bytes             -= take;

                if (s_sector_buf_used == UPGRADE_SECTOR_BUF_SIZE)
                {
                    /**
                    * Diagnostic: dump the first 16 B of the very first
                    * sector before write + after read-back. Lets us tell
                    * a W25Q64 write/read corruption from an upstream-
                    * Ymodem corruption when the bootloader rejects the
                    * staged image. Only logs on the first sector
                    * (offset == 0) — kept terse so it's safe to leave on.
                    **/
                    if (0U == s_ota_write_offset)
                    {
                        DEBUG_OUT(i, YMODEM_DATA_LOG_TAG,
                                  "first 16 B to write: "
                                  "%02X %02X %02X %02X %02X %02X %02X %02X "
                                  "%02X %02X %02X %02X %02X %02X %02X %02X",
                                  s_sector_buf[0],  s_sector_buf[1],
                                  s_sector_buf[2],  s_sector_buf[3],
                                  s_sector_buf[4],  s_sector_buf[5],
                                  s_sector_buf[6],  s_sector_buf[7],
                                  s_sector_buf[8],  s_sector_buf[9],
                                  s_sector_buf[10], s_sector_buf[11],
                                  s_sector_buf[12], s_sector_buf[13],
                                  s_sector_buf[14], s_sector_buf[15]);
                    }

                    ext_flash_status_t st = Write_OtaData(s_ota_write_offset,
                                                          UPGRADE_SECTOR_BUF_SIZE,
                                                          s_sector_buf);
                    if (EXT_FLASH_OK != st)
                    {
                        DEBUG_OUT(e, YMODEM_DATA_LOG_TAG,
                                  "OTA W25Q64 sector write failed at "
                                  "offset=%lu err=%d",
                                  (unsigned long)s_ota_write_offset, (int)st);
                    }
                    else
                    {
                        s_session_bytes_out += UPGRADE_SECTOR_BUF_SIZE;
                    }

                    /* Read-back verify the first sector. */
                    if (0U == s_ota_write_offset && EXT_FLASH_OK == st)
                    {
                        uint8_t verify[16] = {0};
                        if (EXT_FLASH_OK == Read_OtaData(0U, 16U, verify))
                        {
                            DEBUG_OUT(i, YMODEM_DATA_LOG_TAG,
                                      "first 16 B read-back: "
                                      "%02X %02X %02X %02X %02X %02X %02X %02X "
                                      "%02X %02X %02X %02X %02X %02X %02X %02X",
                                      verify[0],  verify[1],
                                      verify[2],  verify[3],
                                      verify[4],  verify[5],
                                      verify[6],  verify[7],
                                      verify[8],  verify[9],
                                      verify[10], verify[11],
                                      verify[12], verify[13],
                                      verify[14], verify[15]);
                        }
                    }

                    s_ota_write_offset += UPGRADE_SECTOR_BUF_SIZE;
                    s_sector_buf_used   = 0U;
                }
            }
        }

        /**
        * Always ack the producer — even on write failure the Ymodem state
        * machine needs the buffer back. The error is logged above, and
        * the bootloader will reject the image on the next boot if the
        * size or CRC doesn't match what was written.
        **/
        (void)osal_sema_give(Semaphore_ExtFlashState);
    }
}

/**
* @brief HAL DMA-idle callback. Fires from USART1 / DMA2_Stream5 IRQ
*        context with @p Size = bytes received before the idle line was
*        detected. Forwards the length to Receive_Byte() in ymodem.c via
*        Q_YmodemReclength.
*
* @param[in]  huart : UART handle (only huart1 is wired).
* @param[in]  Size  : bytes received in this DMA segment.
* @param[out] : None.
* @return None.
*
* @note Implemented here (vs. inside ymodem.c) because the queue handle is
*       owned by this module's resource init — keeping the callback near
*       the owner avoids a header-only dependency on FreeRTOS internals
*       from inside the upstream Ymodem source.
* */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART1)
    {
        return;
    }
    if (NULL == Q_YmodemReclength)
    {
        return;
    }

    osal_base_type_t higher_prio_woken = OSAL_FALSE;
    (void)osal_queue_send_from_isr(Q_YmodemReclength, &Size, &higher_prio_woken);
    portYIELD_FROM_ISR(higher_prio_woken);
}

/**
* @brief HAL completion callback for one-byte interrupt-driven RX. Fires
*        when HAL_UART_Receive_IT() armed by the listener finishes. Wakes
*        the listener task via a binary semaphore so it can process the
*        byte and re-arm.
*
* @param[in]  huart : UART handle (only huart1 is wired).
* @param[out] : None.
* @return None.
*
* @note Co-existence with HAL_UARTEx_RxEventCallback above: at any given
*       moment only one of {RxCplt, RxEvent} is armed on huart1.
*         - LISTENER state IDLE/AWAIT_APPLY → listener has armed
*           HAL_UART_Receive_IT, expects RxCpltCallback
*         - listener is blocked on UPGRADE_EVENT_OTA_STAGED → Ymodem owns
*           UART via HAL_UARTEx_ReceiveToIdle_DMA, expects RxEventCallback
* */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }
    if (NULL == g_listener_byte_sem)
    {
        return;
    }

    osal_base_type_t higher_prio_woken = OSAL_FALSE;
    (void)osal_sema_give_from_isr(g_listener_byte_sem, &higher_prio_woken);
    portYIELD_FROM_ISR(higher_prio_woken);
}
//******************************* Functions *********************************//
