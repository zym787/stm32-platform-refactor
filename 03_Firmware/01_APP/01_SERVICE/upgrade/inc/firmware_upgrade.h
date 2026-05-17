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
 *          - Event bits that the magic-byte listener (01_SERVICE/upgrade/
 *            ota_uart_listener.c, added in a later commit) uses to drive the
 *            upgrade state machine
 *          - The two task entries that get registered in g_user_task_cfg[]
 *
 *        Task topology:
 *
 *          ┌─────────────────────────────────────────────────────────────┐
 *          │ ota_uart_listener (later commit)                            │
 *          │   scans UART1 RX for 0x11 22 33 / 0x77 88 99 magic bytes    │
 *          │   sets EVENT_OTA_START / EVENT_OTA_APPLY                    │
 *          └────────────────────────┬────────────────────────────────────┘
 *                                   │ event group
 *                                   ▼
 *          ┌────────────────────────┴────────────────────────────────────┐
 *          │ ymodem_recv_task                                            │
 *          │   waits EVENT_OTA_START                                     │
 *          │   runs Ymodem_Receive(double-buffer 2×1030 B)               │
 *          │   on completion: writes ota_flag {state=DOWNLOAD_FINISHED,  │
 *          │   image_size, current_app_size} and logs "OTA staged"       │
 *          └──┬──────────────────────────────────────────────────────────┘
 *             │ Queue_AppDataBuffer (Ymodem_RxContext_t *)
 *             ▼
 *          ┌──┴──────────────────────────────────────────────────────────┐
 *          │ firmware_upgrade_task                                       │
 *          │   consumes ctx, writes payload to W25Q64 OTA partition      │
 *          │   gives Semaphore_ExtFlashState so Ymodem can swap buffers  │
 *          └─────────────────────────────────────────────────────────────┘
 *
 *        UART1 RX is owned by the listener / Ymodem_Receive — see
 *        HAL_UARTEx_RxEventCallback in firmware_upgrade_task.c.
 *
 * @version V1.0 2026-05-14
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __FIRMWARE_UPGRADE_H__
#define __FIRMWARE_UPGRADE_H__

//******************************** Includes *********************************//
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* Event-group bits driving the OTA orchestration state machine. */
#define UPGRADE_EVENT_OTA_START   (1UL << 0)  /* listener → ymodem_recv  */
#define UPGRADE_EVENT_OTA_APPLY   (1UL << 1)  /* (reserved)              */
#define UPGRADE_EVENT_OTA_STAGED  (1UL << 2)  /* ymodem_recv → listener  */
//******************************** Defines **********************************//

//******************************* Functions *********************************//
/**
* @brief Create OS resources owned by the firmware-upgrade service.
*
*        Must be called from user_init_task_function BEFORE the two upgrade
*        tasks (firmware_upgrade_task, ymodem_recv_task) and the magic-byte
*        listener are spawned. Idempotent (no-op on second call).
*
* @param[in]  : None.
* @param[out] : None.
*
* @return  0 on success, -1 on resource creation failure.
* */
int8_t firmware_upgrade_resources_init(void);

/**
* @brief Signal "start a Ymodem download cycle now". Edge-triggered; the
*        listener calls this on the first 0x11 22 33 magic, ymodem_recv_task
*        consumes the event.
*
* @return  0 on success, -1 if resources not initialised.
* */
int8_t firmware_upgrade_signal_start(void);

/**
* @brief Signal "apply the staged image now" — triggers NVIC_SystemReset()
*        after a short delay so the listener TX buffer can flush. Listener
*        calls this on the 0x77 88 99 magic.
*
* @return  Does not return; the system resets.
* */
void firmware_upgrade_signal_apply(void);

/**
* @brief Flush the partial-sector staging buffer at the end of a Ymodem
*        session. Must be called by ymodem_recv_task right after
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
void ymodem_recv_task     (void *argument);
void ota_uart_listener_task(void *argument);
//******************************* Functions *********************************//

#endif /* __FIRMWARE_UPGRADE_H__ */
