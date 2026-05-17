/******************************************************************************
 * @file user_init.c
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Create application tasks from the user task configuration table.
 *
 * Processing flow:
 * Start init task, create configured tasks, rollback on failure, then exit.
 * @version V1.0 2026--
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_wrapper_adapter.h"

#include "i2c_port.h"
#include "spi_port.h"

#include "user_task_reso_config.h"
#include "user_init.h"
#include "user_externflash_manage.h"
#include "upgrade_service.h"
#include "firmware_upgrade.h"
#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
osal_task_handle_t user_init_task_handle;

extern usertaskcfg_t g_user_task_cfg[USER_TASK_NUM];
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
static void user_init_task_function(void *argument)
{
    int32_t ret = 0;

    /**
    * Post-OTA verification: bootloader leaves state=CHECK_START before
    * jumping in here. Confirm we booted successfully by flipping the flag
    * back to NO_APP_UPDATE — must complete inside the IWDG window (~6 s)
    * or the watchdog will fire and bootloader rolls back from BLOCK_1.
    *
    * For now this is unconditional auto-confirm right at the top of init.
    * Future work (battery / sensor self-test) can gate the write on those
    * checks; do NOT push it further down the init path without confirming
    * total elapsed time stays well under 6 s.
    **/
    ota_flag_t ota_f;
    if (ota_flag_read(&ota_f) == 0 &&
        (ota_f.state == CFG_OTA_APP_CHECK_START ||
         ota_f.state == CFG_OTA_APP_CHECKING))
    {
        /**
        * Both CHECK_START (0x33, ota_apply_update direct jump) and
        * CHECKING (0x44, bootloader OTA_StateManager re-entry after an
        * intermediate reset) need an auto-confirm here — otherwise IWDG
        * fires and bootloader bounces us forever.
        **/
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

    /* App-layer OS resources owned by the storage manager.
     * Created here -- before any application task is spawned -- so that
     * storage_manager_task and the first Read_/Write_LvglData() caller
     * find the event group / sema / mutex already in place. */
    if (EXT_FLASH_OK != storage_manager_resources_init())
    {
        DEBUG_OUT(e, USER_INIT_ERR_LOG_TAG,
                  "storage_manager_resources_init failed");
    }

    /* OTA firmware-upgrade OS resources: queues + binary semaphore + event
     * group consumed by ymodem_recv_task / firmware_upgrade_task /
     * ota_uart_listener (the listener is added in a later commit). */
    if (0 != firmware_upgrade_resources_init())
    {
        DEBUG_OUT(e, USER_INIT_ERR_LOG_TAG,
                  "firmware_upgrade_resources_init failed");
    }

    /* MCU port buses (mutex creation; HAL handles already initialised). */
    (void)core_i2c_port_init(CORE_I2C_BUS_1);   /* MPU6050 / I2C3       */
    (void)core_i2c_port_init(CORE_I2C_BUS_2);   /* CST816T / I2C1       */
    (void)core_spi_port_init(CORE_SPI_BUS_1);   /* ST7789  / SPI1       */
    (void)core_spi_port_init(CORE_SPI_BUS_2);   /* W25Q64  / SPI2       */

    for (int8_t i = 0; i < USER_TASK_NUM; i++)
    {
        ret = osal_task_create(
                    &g_user_task_cfg[i].task_handle,
                     g_user_task_cfg[i].task_name,
                     g_user_task_cfg[i].argument,
                     g_user_task_cfg[i].func_pointer,
                     g_user_task_cfg[i].stack_depth,
                     g_user_task_cfg[i].priority);
        if (ret == 0)
        {
            DEBUG_OUT(i, USER_INIT_LOG_TAG,
                      "task [%s] created ok", g_user_task_cfg[i].task_name);
        }
        else
        {
            DEBUG_OUT(e, USER_INIT_ERR_LOG_TAG,
                      "task [%s] create failed, ret=%d",
                      g_user_task_cfg[i].task_name, (int)ret);
            while (i >= 0)
            {
                osal_task_delete(g_user_task_cfg[i--].task_handle);
            }
        }
    }

    osal_task_delete(user_init_task_handle);
}

void user_apptask_init(void)
{
    osal_task_create(&user_init_task_handle, "user_Init_Task", NULL,
                   user_init_task_function, 1024, PRI_HARD_REALTIME);
}

//******************************* Functions *********************************//
