/******************************************************************************
 * @file user_task_reso_config.h
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Define user task configuration types, priorities and task table size.
 *
 * Processing flow:
 * Expose usertaskcfg_t and task priority/resource macros for app init.
 * @version V1.0 2026-04-10
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __USER_TASK_RESO_CONFIG_H__
#define __USER_TASK_RESO_CONFIG_H__

//******************************** Includes *********************************//
#include "osal_wrapper_adapter.h"
#include "FreeRTOSConfig.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define PRI_EMERGENCY                   (configMAX_PRIORITIES - 1)
#define PRI_HARD_REALTIME               (PRI_EMERGENCY - 4)
#define PRI_SOFT_REALTIME               (PRI_HARD_REALTIME - 5)
#define PRI_NORMAL                      (PRI_SOFT_REALTIME - 7)
#define PRI_BACKGROUND                  (1)
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/* ========== Production tasks ========== */

/* --- Motion (MPU6050) --- */
#define USER_TASK_MPU6050_HANDLER       1
#define USER_TASK_UNPACK_TASK           1

/* --- Temp / Humidity (AHT21) --- */
#define USER_TASK_TEMP_HUMI_HANDLER     1
#define USER_TASK_TEMP_HUMI_TEST_A      1
#define USER_TASK_TEMP_HUMI_TEST_B      0

/* --- Audio (WT588) --- */
#define USER_TASK_WT588_HANDLER         0

/* --- Display (LVGL / ST7789) --- */
#define USER_LVGL_TEST_TASK             1

/* --- Storage (W25Q64) --- */
/* W25Q64_HANDLER + STORAGE_MANAGER are mandatory for the OTA path:
   firmware_upgrade_task → Write_OtaData → storage_manager_task posts
   EVENT_OTA → externflash_drv_write → flash_handler_thread does the
   actual SPI page-program. With either disabled, Write_OtaData blocks
   forever on s_done_sem and Ymodem stalls at the first data packet. */
#define USER_TASK_W25Q64_HANDLER        1
#define USER_TASK_STORAGE_MANAGER       1

/* --- Heart Rate (EM7028) --- */
#define USER_TASK_EM7028_HANDLER        1
#define USER_TASK_EM7028_HEART_RATE     1

/* --- System --- */
#define USER_TASK_TASK_HIGHER_WATER     0

/* --- OTA (01_SERVICE/upgrade) --- */
/* IWDG feeder runs on EVERY boot because F411 IWDG can't be disabled once
   the bootloader enables it; gating it on USER_TASK_IWDG_FEEDER is just for
   bring-up / unit testing on hardware where the bootloader hasn't enabled
   the watchdog. Keep at 1 in production. */
#define USER_TASK_IWDG_FEEDER           1
#define USER_TASK_FIRMWARE_UPGRADE      1
/* ota_service_task is the merge of the old listener + ymodem_recv tasks
   (v2 2026-05-18); USER_TASK_YMODEM_RECV / USER_TASK_OTA_UART_LISTENER
   are removed. */
#define USER_TASK_OTA_SERVICE           1

/* ========== Test / debug tasks (disabled by default) ========== */

#define USER_TASK_WT588_TEST            0
#define USER_TASK_W25Q64_MOCK           0
#define USER_TASK_W25Q64_HAL_TEST       0
#define USER_TASK_ST7789_HAL_TEST       0
#define USER_TASK_CST816T_HAL_TEST      0
#define USER_TASK_EM7028_HAL_TEST       0
/* IIC_HAL_TEST, JSCOPE_CAPTURE, and HEART_RATE all consume the handler
 * frame queue (single-consumer queue) -- enable at most one at a time. */
#define USER_TASK_EM7028_IIC_HAL_TEST   0
#define USER_TASK_EM7028_HANDLER_MOCK   0
#define USER_TASK_EM7028_JSCOPE_CAPTURE 0

typedef enum
{
/* --- Production --- */
#if USER_TASK_MPU6050_HANDLER
    USER_TASK_MPU6050_HANDLER_IDX = 0,
#endif
#if USER_TASK_UNPACK_TASK
    USER_TASK_UNPACK_TASK_IDX,
#endif
#if USER_TASK_TEMP_HUMI_HANDLER
    USER_TASK_TEMP_HUMI_HANDLER_IDX,
#endif
#if USER_TASK_TEMP_HUMI_TEST_A
    USER_TASK_TEMP_HUMI_TEST_A_IDX,
#endif
#if USER_TASK_TEMP_HUMI_TEST_B
    USER_TASK_TEMP_HUMI_TEST_B_IDX,
#endif
#if USER_TASK_WT588_HANDLER
    USER_TASK_WT588_HANDLER_IDX,
#endif
#if USER_LVGL_TEST_TASK
    USER_LVGL_TEST_TASK_IDX,
#endif
#if USER_TASK_W25Q64_HANDLER
    USER_TASK_W25Q64_HANDLER_IDX,
#endif
#if USER_TASK_STORAGE_MANAGER
    USER_TASK_STORAGE_MANAGER_IDX,
#endif
#if USER_TASK_EM7028_HANDLER
    USER_TASK_EM7028_HANDLER_IDX,
#endif
#if USER_TASK_EM7028_HEART_RATE
    USER_TASK_EM7028_HEART_RATE_IDX,
#endif
#if USER_TASK_TASK_HIGHER_WATER
    USER_TASK_TASK_HIGHER_WATER_IDX,
#endif
#if USER_TASK_IWDG_FEEDER
    USER_TASK_IWDG_FEEDER_IDX,
#endif
#if USER_TASK_FIRMWARE_UPGRADE
    USER_TASK_FIRMWARE_UPGRADE_IDX,
#endif
#if USER_TASK_OTA_SERVICE
    USER_TASK_OTA_SERVICE_IDX,
#endif
/* --- Test / debug --- */
#if USER_TASK_WT588_TEST
    USER_TASK_WT588_TEST_IDX,
#endif
#if USER_TASK_W25Q64_MOCK
    USER_TASK_W25Q64_MOCK_IDX,
#endif
#if USER_TASK_W25Q64_HAL_TEST
    USER_TASK_W25Q64_HAL_TEST_IDX,
#endif
#if USER_TASK_EM7028_HAL_TEST
    USER_TASK_EM7028_HAL_TEST_IDX,
#endif
#if USER_TASK_EM7028_IIC_HAL_TEST
    USER_TASK_EM7028_IIC_HAL_TEST_IDX,
#endif
#if USER_TASK_EM7028_HANDLER_MOCK
    USER_TASK_EM7028_HANDLER_MOCK_IDX,
#endif
#if USER_TASK_EM7028_JSCOPE_CAPTURE
    USER_TASK_EM7028_JSCOPE_CAPTURE_IDX,
#endif
    USER_TASK_NUM
} usertaskid_t;

typedef struct
{
    const char        *task_name;
    osal_task_entry_t  func_pointer;
    size_t             stack_depth;
    uint32_t           priority;
    osal_task_handle_t task_handle;
    void              *argument;
} usertaskcfg_t;

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

//******************************* Functions *********************************//

#endif /* __USER_TASK_RESO_CONFIG_H__ */
