/******************************************************************************
 * @file wt588_test_task.c
 *
 * @par dependencies
 * - user_task_reso_config.h
 * - bsp_wt588_handler.h
 *
 * @author Ethan-Hang
 *
 * @brief WT588 voice playback test cases.
 *
 * Processing flow:
 * Wait for handler to initialise, then submit play requests for voice 0x00
 * and 0x01 with volume 0xE1.  Each request is followed by a delay so the
 * previous playback can finish before the next one starts.
 *
 * @version V1.0 2026-04-16
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "user_task_reso_config.h"
#include "bsp_wrapper_audio.h"
#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define WT588_TEST_VOLUME       (6U)
#define WT588_TEST_PRIORITY     (1U)
#define WT588_TEST_PLAY_GAP_MS  (800U)
#define WT588_TEST_INIT_WAIT_MS (3000U)
//******************************** Defines **********************************//

//******************************* Functions *********************************//
void wt588_test_task(void *argument)
{
    (void)argument;

    DEBUG_OUT(i, WT588_TEST_LOG_TAG, "wt588_test_task started");

    /* Wait for handler thread to finish initialisation */
    osal_task_delay(WT588_TEST_INIT_WAIT_MS);

    DEBUG_OUT(i, WT588_TEST_LOG_TAG,
              "[TEST0] play 0x00, volume level 6");
    audio_drv_play(WT588_TEST_PRIORITY + 2, WT588_TEST_VOLUME, 0x00U);

    osal_task_delay(WT588_TEST_PLAY_GAP_MS);

    /* ---- Test 1: play voice 0x01 ---- */
    DEBUG_OUT(i, WT588_TEST_LOG_TAG,
              "[TEST1] play 0x01, volume level 6");
    audio_drv_play(WT588_TEST_PRIORITY + 1, WT588_TEST_VOLUME, 0x01U);

    osal_task_delay(WT588_TEST_PLAY_GAP_MS);

    /* ---- Test 2: play voice 0x02 ---- */
    DEBUG_OUT(i, WT588_TEST_LOG_TAG,
              "[TEST2] play 0x02, volume level 6");
    audio_drv_play(WT588_TEST_PRIORITY + 2, WT588_TEST_VOLUME, 0x02U);

    osal_task_delay(4000);

    audio_drv_stop();

    DEBUG_OUT(i, WT588_TEST_LOG_TAG, "wt588_test_task all tests done");
    
    for (;;)
    {
        osal_task_delay(1000U);
    }
}
//******************************* Functions *********************************//
