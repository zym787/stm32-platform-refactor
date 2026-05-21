/******************************************************************************
 * @file task_higher_water_monitor.c
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Runtime stack high-water monitor task implementation.
 *
 * Processing flow:
 * Override weak task_higher_water_thread, periodically scan configured tasks,
 * then print high-water report to SEGGER RTT Terminal 1.
 * @version V1.0 2026-04-13
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "user_task_reso_config.h"

#include "FreeRTOS.h"
#include "task.h"
#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define STACK_MONITOR_PERIOD_TICKS  (5000U)

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
extern usertaskcfg_t g_user_task_cfg[USER_TASK_NUM];

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
static void stack_higher_water_report_once(void)
{
    DEBUG_OUT(i, STACK_MONITOR_LOG_TAG, "---------------------------------------------");

    for (uint32_t i = 0; i < (uint32_t)USER_TASK_NUM; i++)
    {
        const char        *task_name   = g_user_task_cfg[i].task_name;
        osal_task_handle_t task_handle = g_user_task_cfg[i].task_handle;

        if ((NULL == task_handle) || (NULL == task_name))
        {
            continue;
        }

        UBaseType_t min_free_words = uxTaskGetStackHighWaterMark((TaskHandle_t)task_handle);
        uint32_t    cfg_words      = (uint32_t)g_user_task_cfg[i].stack_depth;
        uint32_t    used_max_words = (cfg_words > (uint32_t)min_free_words) ?
                                     (cfg_words - (uint32_t)min_free_words) : 0U;

        DEBUG_OUT(i, STACK_MONITOR_LOG_TAG,
                  "[%-25s] cfg=%5luW  used_max=%5luW  min_free=%5luW",
                  task_name,
                  (unsigned long)cfg_words,
                  (unsigned long)used_max_words,
                  (unsigned long)min_free_words);
    }
 
    DEBUG_OUT(i, STACK_MONITOR_LOG_TAG, "---------------------------------------------");
}

void task_higher_water_thread(void *argument)
{
    (void)argument;

    DEBUG_OUT(i, STACK_MONITOR_LOG_TAG, "stack monitor started");

    for (;;)
    {
        stack_higher_water_report_once();
        osal_task_delay(STACK_MONITOR_PERIOD_TICKS);
    }
}

//******************************* Functions *********************************//
