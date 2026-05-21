/******************************************************************************
 * @file temp_humi_concurrency_test.c
 *
 * @par dependencies
 * - bsp_wrapper_temp_humi.h
 * - osal_wrapper_adapter.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief Concurrent sync-read stress test for the AHT21 adapter layer.
 *
 * Two independent FreeRTOS tasks call blocking sync-read APIs simultaneously
 * at different intervals.  The adapter's mutex + request-ID mechanism must
 * ensure that:
 *   1. Only one read is in flight at any given time (serialisation).
 *   2. Results are delivered to the correct caller (no cross-contamination).
 *   3. A timed-out request does not corrupt the next one (stale-callback
 *      protection via request-ID discard).
 *
 * Expected log output (TEMP_HUMI_TEST tag, info level):
 *   [TaskA] temp=26.50 C
 *   [TaskB] temp=26.50 C  humi=48.20 %
 *   [TaskA] temp=26.51 C
 *   ...
 *
 * If the mutex or request-ID logic is broken the two tasks may deadlock, or
 * you will see [TaskA]/[TaskB] results with swapped or zeroed values.
 *
 * @version V1.0 2026-04-12
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_wrapper_temp_humi.h"
#include "osal_wrapper_adapter.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/** Poll interval for Task A (temperature-only read). */
#define TEST_TASK_A_PERIOD_MS  (600U)

/** Poll interval for Task B (temperature + humidity read). */
#define TEST_TASK_B_PERIOD_MS  (800U)
//******************************** Defines **********************************//

//******************************* Functions *********************************//

/**
 * @brief Concurrent test task A — reads temperature only every 600 ms.
 *
 * Deliberately interleaves with Task B to stress-test the adapter mutex and
 * request-ID binding.
 *
 * @param[in] argument: unused.
 */
void temp_humi_test_task_a(void *argument)
{
    osal_task_delay(1000);
    (void)argument;
    float              temp = 0.0f;
    wp_temp_humi_status_t ret;

    DEBUG_OUT(i, TEMP_HUMI_TEST_LOG_TAG, "[TaskA] started");

    for (;;)
    {
        ret = temp_humi_read_temp_sync(&temp, 500);

        if (WP_TEMP_HUMI_OK == ret)
        {
            DEBUG_OUT(i, TEMP_HUMI_TEST_LOG_TAG,
                      "[TaskA] temp = %.2f C",
                      (double)temp);
        }
        else
        {
            DEBUG_OUT(e, TEMP_HUMI_TEST_ERR_LOG_TAG,
                      "[TaskA] read_temp_sync failed ret=%d",
                      (int)ret);
        }

        osal_task_delay(TEST_TASK_A_PERIOD_MS);
    }
}

/**
 * @brief Concurrent test task B — reads temperature + humidity every 800 ms.
 *
 * Deliberately interleaves with Task A to stress-test the adapter mutex and
 * request-ID binding.
 *
 * @param[in] argument: unused.
 */
void temp_humi_test_task_b(void *argument)
{
    osal_task_delay(1000);
    (void)argument;
    float              temp = 0.0f;
    float              humi = 0.0f;
    wp_temp_humi_status_t ret;

    DEBUG_OUT(i, TEMP_HUMI_TEST_LOG_TAG, "[TaskB] started");

    for (;;)
    {
        ret = temp_humi_read_all_sync(&temp, &humi, 500);

        if (WP_TEMP_HUMI_OK == ret)
        {
            DEBUG_OUT(i, TEMP_HUMI_TEST_LOG_TAG,
                      "[TaskB] temp = %.2f C  humi = %.2f %%",
                      (double)temp,
                      (double)humi);
        }
        else
        {
            DEBUG_OUT(e, TEMP_HUMI_TEST_ERR_LOG_TAG,
                      "[TaskB] read_all_sync failed ret=%d",
                      (int)ret);
        }

        osal_task_delay(TEST_TASK_B_PERIOD_MS);
    }
}

//******************************* Functions *********************************//
