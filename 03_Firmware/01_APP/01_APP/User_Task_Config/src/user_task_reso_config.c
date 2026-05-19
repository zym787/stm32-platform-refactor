/******************************************************************************
 * @file user_task_reso_config.c
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Configure user task resource table and weak fallback task entries.
 *
 * Processing flow:
 * Define g_user_task_cfg and provide weak default task functions.
 * @version V1.0 2026-04-10
 * @version V2.0 2026-04-12
 * @upgrade 2.0: Added temp_humi_test_task_a / _b for concurrent sync-read
 *               concurrency validation.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "user_task_reso_config.h"

#include "aht21_integration.h"
#include "mpu6050_integration.h"
#include "wt588_integration.h"
#include "w25q64_integration.h"
#include "em7028_integration.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define __WEAK__ __attribute__((weak))

//******************************** Defines **********************************//

//******************************* Functions *********************************//
void mpuxxxx_handler_thread(void *argument);
void unpack_task_thread(void *argument);
void temp_humi_handler_thread(void *argument);
void wt588_handler_thread(void *argument);

void temp_humi_test_task_a(void *argument);
void temp_humi_test_task_b(void *argument);
void task_higher_water_thread(void *argument);
void wt588_test_task(void *argument);
void st7789_hal_test_task(void *argument);
void lvgl_display_task(void *argument);
void cst816t_mock_test_task(void *argument);
void cst816t_hal_test_task(void *argument);
void w25q64_mock_test_task(void *argument);
void w25q64_handler_mock_test_task(void *argument);
void w25q64_hal_test_task(void *argument);
void em7028_handler_thread(void *argument);
void em7028_iic_hal_test_task(void *argument);
void em7028_mock_test_task(void *argument);
void em7028_handler_mock_test_task(void *argument);
void em7028_jscope_capture_task(void *argument);
void em7028_heart_rate_task(void *argument);
void flash_handler_thread(void *argument);
void storage_manager_task(void *argument);
void iwdg_feeder_task(void *argument);
void firmware_upgrade_task(void *argument);
void ota_service_task(void *argument);

usertaskcfg_t g_user_task_cfg[USER_TASK_NUM] =
{
    /* ========== Production tasks ========== */

    /* Motion (MPU6050) */
#if USER_TASK_MPU6050_HANDLER
    [USER_TASK_MPU6050_HANDLER_IDX] = {
        .task_name    = "mpu6050_handler_thread",
        .func_pointer = mpuxxxx_handler_thread,
        .stack_depth  = 384,
        .priority     = PRI_NORMAL + 1,
        .task_handle  = NULL,
        .argument     = &mpu6050_input_args
    },
#endif

#if USER_TASK_UNPACK_TASK
    [USER_TASK_UNPACK_TASK_IDX] = {
        .task_name    = "unpack_task_thread",
        .func_pointer = unpack_task_thread,
        .stack_depth  = 512,
        .priority     = PRI_NORMAL,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

    /* Temp / Humidity (AHT21) */
#if USER_TASK_TEMP_HUMI_HANDLER
    [USER_TASK_TEMP_HUMI_HANDLER_IDX] = {
        .task_name    = "temp_humi_handler_thread",
        .func_pointer = temp_humi_handler_thread,
        .stack_depth  = 256,
        .priority     = PRI_NORMAL,
        .task_handle  = NULL,
        .argument     = &aht21_input_arg
    },
#endif

#if USER_TASK_TEMP_HUMI_TEST_A
    [USER_TASK_TEMP_HUMI_TEST_A_IDX] = {
        .task_name    = "temp_humi_test_task_a",
        .func_pointer = temp_humi_test_task_a,
        .stack_depth  = 384,
        .priority     = PRI_NORMAL,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

#if USER_TASK_TEMP_HUMI_TEST_B
    [USER_TASK_TEMP_HUMI_TEST_B_IDX] = {
        .task_name    = "temp_humi_test_task_b",
        .func_pointer = temp_humi_test_task_b,
        .stack_depth  = 384,
        .priority     = PRI_NORMAL,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

    /* Audio (WT588) */
#if USER_TASK_WT588_HANDLER
    [USER_TASK_WT588_HANDLER_IDX] = {
        .task_name    = "wt588_handler_thread",
        .func_pointer = wt588_handler_thread,
        .stack_depth  = 256,
        .priority     = PRI_NORMAL - 2,
        .task_handle  = NULL,
        .argument     = &wt588_handler_input_args
    },
#endif

    /* Display (LVGL / ST7789) */
#if USER_LVGL_TEST_TASK
    [USER_LVGL_TEST_TASK_IDX] = {
        .task_name    = "lvgl_display_task",
        .func_pointer = lvgl_display_task,
        .stack_depth  = 4096,
        .priority     = PRI_SOFT_REALTIME,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

    /* Storage (W25Q64) */
#if USER_TASK_W25Q64_HANDLER
    [USER_TASK_W25Q64_HANDLER_IDX] = {
        .task_name    = "flash_handler_thread",
        .func_pointer = flash_handler_thread,
        .stack_depth  = 384,
        .priority     = PRI_NORMAL,
        .task_handle  = NULL,
        .argument     = &w25q64_input_arg
    },
#endif

#if USER_TASK_STORAGE_MANAGER
    [USER_TASK_STORAGE_MANAGER_IDX] = {
        .task_name    = "storage_manager_task",
        .func_pointer = storage_manager_task,
        .stack_depth  = 256,
        .priority     = PRI_NORMAL,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

    /* Heart Rate (EM7028) */
#if USER_TASK_EM7028_HANDLER
    [USER_TASK_EM7028_HANDLER_IDX] = {
        .task_name    = "em7028_handler_thread",
        .func_pointer = em7028_handler_thread,
        .stack_depth  = 384,
        .priority     = PRI_NORMAL,
        .task_handle  = NULL,
        .argument     = &em7028_input_arg
    },
#endif

#if USER_TASK_EM7028_HEART_RATE
    [USER_TASK_EM7028_HEART_RATE_IDX] = {
        .task_name    = "em7028_heart_rate",
        .func_pointer = em7028_heart_rate_task,
        .stack_depth  = 384,
        .priority     = PRI_NORMAL,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

    /* System */
#if USER_TASK_TASK_HIGHER_WATER
    [USER_TASK_TASK_HIGHER_WATER_IDX] = {
        .task_name    = "task_higher_water_thread",
        .func_pointer = task_higher_water_thread,
        .stack_depth  = 256,
        .priority     = PRI_NORMAL + 2,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

#if USER_TASK_IWDG_FEEDER
    [USER_TASK_IWDG_FEEDER_IDX] = {
        .task_name    = "iwdg_feeder",
        .func_pointer = iwdg_feeder_task,
        .stack_depth  = 128,
        .priority     = PRI_BACKGROUND,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

    /* OTA */
#if USER_TASK_FIRMWARE_UPGRADE
    [USER_TASK_FIRMWARE_UPGRADE_IDX] = {
        .task_name    = "firmware_upgrade",
        .func_pointer = firmware_upgrade_task,
        .stack_depth  = 256,
        .priority     = PRI_SOFT_REALTIME,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

#if USER_TASK_OTA_SERVICE
    [USER_TASK_OTA_SERVICE_IDX] = {
        .task_name    = "ota_service",
        .func_pointer = ota_service_task,
        .stack_depth  = 384,
        .priority     = PRI_SOFT_REALTIME,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

    /* ========== Test / debug tasks ========== */

#if USER_TASK_WT588_TEST
    [USER_TASK_WT588_TEST_IDX] = {
        .task_name = "wt588_test_task",
        .func_pointer = wt588_test_task,
        .stack_depth = 256,
        .priority = PRI_HARD_REALTIME,
        .task_handle = NULL,
        .argument = NULL
    },
#endif

#if USER_TASK_W25Q64_MOCK
    [USER_TASK_W25Q64_MOCK_IDX] = {
        .task_name    = "w25q64_mock_test_task",
        .func_pointer = w25q64_handler_mock_test_task,
        .stack_depth  = 1024,
        .priority     = PRI_NORMAL,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

#if USER_TASK_W25Q64_HAL_TEST
    [USER_TASK_W25Q64_HAL_TEST_IDX] = {
        .task_name    = "w25q64_hal_test_task",
        .func_pointer = w25q64_hal_test_task,
        .stack_depth  = 1024,
        .priority     = PRI_NORMAL - 1,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

#if USER_TASK_EM7028_HAL_TEST
    [USER_TASK_EM7028_HAL_TEST_IDX] = {
        .task_name      = "em7028_mock",
        .stack_depth    = 1024U,
        .priority       = PRI_HARD_REALTIME,
        .func_pointer   = em7028_mock_test_task,
        .argument       = NULL,
    },
#endif

#if USER_TASK_EM7028_IIC_HAL_TEST
    [USER_TASK_EM7028_IIC_HAL_TEST_IDX] = {
        .task_name    = "em7028_iic_hal_test",
        .func_pointer = em7028_iic_hal_test_task,
        .stack_depth  = 1024,
        .priority     = PRI_NORMAL - 1,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

#if USER_TASK_EM7028_HANDLER_MOCK
    [USER_TASK_EM7028_HANDLER_MOCK_IDX] = {
        .task_name    = "em7028_handler_mock",
        .func_pointer = em7028_handler_mock_test_task,
        .stack_depth  = 1024,
        .priority     = PRI_NORMAL,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

#if USER_TASK_EM7028_JSCOPE_CAPTURE
    [USER_TASK_EM7028_JSCOPE_CAPTURE_IDX] = {
        .task_name    = "em7028_jscope_capture",
        .func_pointer = em7028_jscope_capture_task,
        .stack_depth  = 1024,
        .priority     = PRI_NORMAL - 1,
        .task_handle  = NULL,
        .argument     = NULL
    },
#endif

};

__WEAK__ void mpuxxxx_handler_thread(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void unpack_task_thread(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void temp_humi_handler_thread(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void wt588_handler_thread(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void task_higher_water_thread(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void temp_humi_test_task_a(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void temp_humi_test_task_b(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void wt588_test_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void st7789_hal_test_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void lvgl_display_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void cst816t_mock_test_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void cst816t_hal_test_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void w25q64_mock_test_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void w25q64_hal_test_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void em7028_handler_thread(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void em7028_iic_hal_test_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void em7028_mock_test_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void em7028_handler_mock_test_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void em7028_jscope_capture_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void em7028_heart_rate_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void flash_handler_thread(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}

__WEAK__ void storage_manager_task(void *argument)
{
    for (;;)
    {
        osal_task_delay(1000);
    }
}
//******************************* Functions *********************************//
