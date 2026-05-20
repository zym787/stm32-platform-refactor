/******************************************************************************
 * @file firmware_upgrade_task.c
 *
 * @par dependencies
 * - firmware_upgrade.h
 * - ymodem.h
 * - ota_storage.h     (abstract OTA staging — adapter lives in 01_APP)
 * - ota_transport.h   (called from service_init for one-shot setup)
 * - upgrade_service.h (ota_flag persistence — already MCU-port-abstracted)
 * - osal_wrapper_adapter.h / os_freertos.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief Consumer half of the OTA pipeline. Pure service-layer code:
 *        speaks only to OSAL + the abstract OTA storage interface. No
 *        HAL, no MCU peripheral, no W25Q64 awareness.
 *
 *        Owns two OSAL objects that bridge to the Ymodem producer:
 *          g_otaDataQueue     — Ymodem_RxContext_t * per packet
 *          g_extFlashAckSem — back-pressure ack to Ymodem
 *
 *        The third OSAL object the previous version owned
 *        (Q_YmodemReclength) is now internal to the transport adapter,
 *        accessed through ota_transport_frame_wait().
 *
 * @version V1.0 2026-05-14
 * @version V3.0 2026-05-18  storage abstracted via ota_storage; HAL
 *                            callbacks moved to the transport adapter
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include <string.h>
#include <stdbool.h>

#include "osal_wrapper_adapter.h"
#include "osal_error.h"
#include "os_freertos.h"

#include "firmware_upgrade.h"
#include "ymodem.h"
#include "ota_storage.h"
#include "ota_transport.h"
#include "upgrade_service.h"
#include "cfg_ota.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define UPGRADE_QUEUE_DATA_DEPTH   (2U)   /* matches Ymodem double-buffer  */

/**
 * @brief Sector-buffer size. Must equal ota_storage_sector_size() at
 *        runtime — kept as a compile-time constant because s_sector_buf
 *        is a static BSS allocation. If a future storage adapter has a
 *        different sector size (e.g. 64 KB block-erase chip), bump this
 *        in lockstep.
 */
#define UPGRADE_SECTOR_BUF_SIZE    (4096U)
//******************************** Defines **********************************//

//******************************* Variables *********************************//
/**
 * @brief Producer-consumer between Ymodem and this task.
 *
 *        Definitions live here (not in Ymodem) so service_ota owns the
 *        lifetime — Ymodem just externs them. Adapter-level OSAL objects
 *        (byte sema, frame-length queue) live in the transport adapter
 *        in 01_APP/Service_Adapters/.
 */
osal_queue_handle_t g_otaDataQueue     = NULL;
osal_sema_handle_t  g_extFlashAckSem = NULL;

/* Sector-aligned write coalescer. Owned by firmware_upgrade_task; flushed
   by firmware_upgrade_flush_staged() at session end. BSS-allocated to keep
   the task stack small. Single-producer access (this task processes
   g_otaDataQueue serially; the flush call from ota_service_task only
   runs AFTER Ymodem_Receive returns, by which point this task has parked
   on the queue) so no mutex is needed. */
static uint8_t   s_sector_buf[UPGRADE_SECTOR_BUF_SIZE];
static uint32_t  s_sector_buf_used   = 0U;
static uint32_t  s_session_bytes_out = 0U;
/* OTA-partition write cursor; advances per flushed sector. File-scope so
   firmware_upgrade_flush_staged() (called from ota_service_task) can
   reach it after firmware_upgrade_task has parked on g_otaDataQueue. */
static uint32_t  s_ota_write_offset  = 0U;
/* Sticky "this session is poisoned" flag — flipped on the first
   ota_storage_write failure. Once set, firmware_upgrade_flush_staged()
   returns -1 immediately so ota_run_ymodem_session() bails out before
   stamping the ota_flag. Cleared on FILE_INFO at the start of the next
   session. Without this, a write failure mid-stream would leave a 4 KB
   gap in the staged image but the bytes-out counter would still match
   the declared image size — bootloader would decrypt a corrupted blob. */
static bool      s_session_poisoned   = false;
//******************************* Variables *********************************//

//******************************* Functions *********************************//
/**
* @brief Create the service-owned OS resources (g_otaDataQueue +
*        g_extFlashAckSem). Adapter-owned resources (byte sema,
*        frame queue) are created by ota_transport_init separately.
*        Idempotent.
* */
static int8_t firmware_upgrade_resources_init(void)
{
    if (NULL == g_otaDataQueue)
    {
        if (OSAL_SUCCESS != osal_queue_create(&g_otaDataQueue,
                                              UPGRADE_QUEUE_DATA_DEPTH,
                                              sizeof(Ymodem_RxContext_t *)))
        {
            return -1;
        }
    }

    if (NULL == g_extFlashAckSem)
    {
        /**
        * Binary semaphore, starts EMPTY: Ymodem's first packet must wait
        * for consumer to ack before reusing the double-buffer.
        **/
        if (OSAL_SUCCESS != osal_sema_init(&g_extFlashAckSem, 0))
        {
            return -1;
        }
    }

    return 0;
}

int8_t firmware_upgrade_service_init(void)
{
    /**
    * Post-OTA verification: bootloader leaves state=CHECK_START before
    * jumping in here. Confirm we booted successfully by flipping the flag
    * back to NO_APP_UPDATE — must complete inside the IWDG window (~6 s)
    * or the watchdog will fire and bootloader rolls back from BLOCK_1.
    *
    * For now this is unconditional auto-confirm. Future work (battery /
    * sensor self-test) can gate the write on those checks; do NOT push
    * it later in the init path without confirming total elapsed time
    * stays well under 6 s.
    *
    * Best-effort step — a write failure is logged but does not fail the
    * service init (bootloader rollback will recover).
    **/
    ota_flag_t ota_f;
    if (ota_flag_read(&ota_f) == 0 &&
        (ota_f.state == CFG_OTA_APP_CHECK_START ||
         ota_f.state == CFG_OTA_APP_CHECKING))
    {
        DEBUG_OUT(w, USER_INIT_LOG_TAG,
                  "post-OTA first boot (state=0x%02X): auto-confirm",
                  (unsigned)ota_f.state);
        ota_f.state = CFG_OTA_NO_APP_UPDATE;
        if (ota_flag_write(&ota_f) != 0)
        {
            DEBUG_OUT(e, USER_INIT_ERR_LOG_TAG,
                      "post-OTA confirm write failed (IWDG will roll back)");
        }
    }

    /**
    * Bring up the abstract transport + storage adapters. Doing this here
    * (vs scattered calls in user_init) keeps the OTA service self-
    * contained: callers only need to invoke firmware_upgrade_service_init
    * to have the whole OTA path ready.
    **/
    if (OTA_TRANSPORT_OK != ota_transport_init())
    {
        DEBUG_OUT(e, USER_INIT_ERR_LOG_TAG, "ota_transport_init failed");
        return -1;
    }
    if (OTA_STORAGE_OK != ota_storage_init())
    {
        DEBUG_OUT(e, USER_INIT_ERR_LOG_TAG, "ota_storage_init failed");
        return -1;
    }
    return firmware_upgrade_resources_init();
}

int32_t firmware_upgrade_flush_staged(void)
{
    if (s_session_poisoned)
    {
        /* A prior ota_storage_write failed; refuse to seal the session so
           ota_run_ymodem_session sees the error and skips the ota_flag write. */
        return -1;
    }

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

    ota_storage_status_t st = ota_storage_write(s_ota_write_offset,
                                                 s_sector_buf,
                                                 UPGRADE_SECTOR_BUF_SIZE);
    if (OTA_STORAGE_OK != st)
    {
        DEBUG_OUT(e, YMODEM_DATA_LOG_TAG,
                  "OTA storage final sector flush failed at offset=%lu",
                  (unsigned long)s_ota_write_offset);
        s_sector_buf_used = 0U;
        return -1;
    }

    /**
    * Account only the actual session bytes (not the 0xFF pad). The
    * bootloader records image_size = bytes_received (the Ymodem-declared
    * count, equal to the .mxxx file length), and reads only that many
    * bytes from storage during decrypt.
    **/
    s_session_bytes_out += s_sector_buf_used;
    s_sector_buf_used    = 0U;
    s_ota_write_offset  += UPGRADE_SECTOR_BUF_SIZE;

    return (int32_t)s_session_bytes_out;
}

/**
* @brief Consumer of Ymodem_RxContext_t pointers — writes each FILE_DATA
*        payload to the abstract OTA storage and unblocks the Ymodem
*        producer via g_extFlashAckSem. FILE_INFO ctx skips the
*        write but still acks so Ymodem can flip buffers.
* */
void firmware_upgrade_task(void *argument)
{
    (void)argument;

    DEBUG_OUT(i, YMODEM_LOG_TAG, "firmware_upgrade_task entered");

    for (;;)
    {
        Ymodem_RxContext_t *ctx = NULL;

        if (OSAL_SUCCESS != osal_queue_receive(g_otaDataQueue, &ctx,
                                                OSAL_MAX_DELAY))
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
            s_session_poisoned  = false;
            DEBUG_OUT(i, YMODEM_FILE_LOG_TAG,
                      "OTA session start, declared size=%d B",
                      (int)ctx->size);
        }
        else if (YMODEM_RX_STATE_FILE_DATA == ctx->state)
        {
            /**
            * FILE_DATA block. Ymodem trimmed packet_length to actual
            * payload bytes already. Payload sits past PACKET_HEADER.
            *
            * Drip the bytes into s_sector_buf and only call
            * ota_storage_write when we have a full sector — the
            * underlying medium typically erases a whole sector per
            * write, so partial-sector writes would wipe out preceding
            * partial-sector writes in the same sector. ota_service_task
            * calls firmware_upgrade_flush_staged at session end to
            * drain the final partial sector.
            **/
            uint32_t        bytes = (uint32_t)ctx->packet_length;
            const uint8_t  *src   = ctx->packet_data + PACKET_HEADER;

            while (bytes > 0U)
            {
                const uint32_t cap  = UPGRADE_SECTOR_BUF_SIZE -
                                       s_sector_buf_used;
                const uint32_t take = (bytes < cap) ? bytes : cap;

                memcpy(&s_sector_buf[s_sector_buf_used], src, take);
                s_sector_buf_used += take;
                src               += take;
                bytes             -= take;

                if (s_sector_buf_used == UPGRADE_SECTOR_BUF_SIZE)
                {
                    ota_storage_status_t st = ota_storage_write(
                        s_ota_write_offset,
                        s_sector_buf,
                        UPGRADE_SECTOR_BUF_SIZE);
                    if (OTA_STORAGE_OK != st)
                    {
                        /**
                        * Poison the session. We keep consuming and acking
                        * incoming packets (so Ymodem can drain cleanly and
                        * the link doesn't time out), but bytes_out stays
                        * frozen and flush_staged will refuse to seal at
                        * the end. ota_run_ymodem_session sees the failure
                        * and does not write the ota_flag.
                        **/
                        DEBUG_OUT(e, YMODEM_DATA_LOG_TAG,
                                  "OTA storage write failed at offset=%lu — session poisoned",
                                  (unsigned long)s_ota_write_offset);
                        s_session_poisoned = true;
                        s_sector_buf_used  = 0U;
                        break;
                    }

                    s_session_bytes_out += UPGRADE_SECTOR_BUF_SIZE;
                    s_ota_write_offset  += UPGRADE_SECTOR_BUF_SIZE;
                    s_sector_buf_used    = 0U;
                }
            }
        }

        /**
        * Always ack the producer — even on write failure the Ymodem state
        * machine needs the buffer back. The error is logged above, and
        * the bootloader will reject the image on the next boot if the
        * size or CRC doesn't match what was written.
        **/
        (void)osal_sema_give(g_extFlashAckSem);
    }
}
//******************************* Functions *********************************//
