/******************************************************************************
 * @file firmware_upgrade.h
 *
 * @par dependencies
 * - stdint.h
 *
 * @author Ethan-Hang
 *
 * @brief Public interface of the OTA firmware-upgrade service.
 *
 *        Exposes:
 *          - One-shot resource init called from user_init before tasks spawn
 *          - Apply-trigger entry the orchestrator calls on the apply magic
 *          - End-of-session sector-buffer flush helper
 *          - The two task entries that get registered in g_user_task_cfg[]
 *
 *        Task topology (v2 — merged listener + ymodem_recv):
 *
 *          ┌─────────────────────────────────────────────────────────────┐
 *          │ ota_service_task                                            │
 *          │   scans UART1 for 0x11 22 33 / 0x77 88 99 magic bytes,      │
 *          │   drives Ymodem_Receive (double-buffer 2×1030 B), writes    │
 *          │   ota_flag {state=DOWNLOAD_FINISHED, image_size,            │
 *          │   current_app_size} on success, then NVIC_SystemReset on    │
 *          │   apply magic.                                              │
 *          └──┬──────────────────────────────────────────────────────────┘
 *             │ g_otaDataQueue (Ymodem_RxContext_t *)
 *             ▼
 *          ┌──┴──────────────────────────────────────────────────────────┐
 *          │ firmware_upgrade_task                                       │
 *          │   consumes ctx, writes payload to W25Q64 OTA partition      │
 *          │   gives g_extFlashAckSem so Ymodem can swap buffers         │
 *          └─────────────────────────────────────────────────────────────┘
 *
 *        UART1 RX is owned by ota_service_task — see
 *        HAL_UARTEx_RxEventCallback in firmware_upgrade_task.c.
 *
 * @version V2.0 2026-05-18  merged listener + ymodem_recv → ota_service_task
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __FIRMWARE_UPGRADE_H__
#define __FIRMWARE_UPGRADE_H__

//******************************** Includes *********************************//
#include <stdint.h>

#include "osal_wrapper_adapter.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
//******************************** Defines **********************************//

//******************************* Variables *********************************//
/**
 * @brief OSAL handles shared between the Ymodem producer and
 *        firmware_upgrade_task (consumer). Defined in firmware_upgrade_task.c,
 *        created by firmware_upgrade_service_init.
 *
 *        g_otaDataQueue   : Ymodem_RxContext_t * per FILE_INFO / FILE_DATA
 *                           packet — Ymodem hands ownership of the current
 *                           ping-pong buffer to the consumer.
 *        g_extFlashAckSem : binary back-pressure ack — consumer signals
 *                           "buffer free for reuse" so Ymodem can swap.
 *
 *        Declared here (not at file scope in ymodem.c) so the linkage is
 *        discoverable by grep and the symbols cannot drift apart.
 */
extern osal_queue_handle_t g_otaDataQueue;
extern osal_sema_handle_t  g_extFlashAckSem;
//******************************* Variables *********************************//

//******************************* Functions *********************************//
/**
* @brief One-shot service-level init for the OTA firmware-upgrade path.
*
*        Two responsibilities in one call so user_init doesn't have to
*        know the internal ordering:
*
*          1. Post-OTA verification: if the bootloader left ota_flag.state
*             at CHECK_START / CHECKING, auto-confirm by flipping it back
*             to NO_APP_UPDATE. Must complete inside the IWDG window
*             (~6 s) — bootloader rolls back from W25Q64 BLOCK_1 otherwise.
*          2. Create OS resources (queues + binary semaphores) consumed by
*             ota_service_task (orchestrator) and firmware_upgrade_task
*             (W25Q64 write consumer).
*
*        Must be called from user_init_task_function BEFORE the two upgrade
*        tasks are spawned. Step 2 is idempotent (no-op on second call).
*
* @return  0 on success, -1 on resource creation failure.
* */
int8_t firmware_upgrade_service_init(void);

/**
* @brief Signal "apply the staged image now" — triggers NVIC_SystemReset()
*        after a short delay so any in-flight UART TX can drain.
*        ota_service_task calls this on the 0x77 88 99 magic.
*
* @return  Does not return; the system resets.
* */
void firmware_upgrade_signal_apply(void);

/**
* @brief Flush the partial-sector staging buffer at the end of a Ymodem
*        session. Must be called by ota_service_task right after
*        Ymodem_Receive returns successfully, BEFORE stamping the OTA
*        flag — otherwise the last sub-4KB chunk never reaches W25Q64.
*
* @param[in]  : None.
* @param[out] : None.
* @return  Total bytes successfully written across the whole session
*          (matches Ymodem_Receive's return on success), or -1 on flush
*          error.
*
* @note See firmware_upgrade_task.c — the W25Q64 driver's write path
*       erases the full 4 KB sector on every write call. To avoid
*       wiping previous chunks in the same sector, packets are
*       buffered in RAM and flushed only when a full 4 KB lines up.
* */
int32_t firmware_upgrade_flush_staged(void);

/* Task entries, registered in g_user_task_cfg[]. */
void firmware_upgrade_task(void *argument);
void ota_service_task     (void *argument);
//******************************* Functions *********************************//

#endif /* __FIRMWARE_UPGRADE_H__ */
