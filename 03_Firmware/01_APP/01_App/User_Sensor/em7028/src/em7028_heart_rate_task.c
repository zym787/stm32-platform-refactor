/******************************************************************************
 * @file em7028_heart_rate_task.c
 *
 * @par dependencies
 * - user_task_reso_config.h
 * - bsp_wrapper_heart_rate.h
 * - hr_algo.h
 * - osal_wrapper_adapter.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief EM7028 PPG heart-rate calculation task.
 *
 *        Consumes PPG frames via the heart_rate wrapper API, feeds hrs1_sum
 *        into the hr_algo middleware module, and publishes results via both
 *        volatile globals (J-Scope / debugger observation) and an OSAL queue
 *        (downstream consumers such as LVGL).
 *
 * @note   This task and em7028_jscope_capture_task both consume the handler
 *         frame queue (single-consumer).  Enable at most one of them at a
 *         time via user_task_reso_config.h.
 *
 * @version V1.0 2026-05-10
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "user_task_reso_config.h"
#include "bsp_wrapper_heart_rate.h"
#include "osal_wrapper_adapter.h"
#include "hr_algo.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

/** Wait for em7028_handler_thread to mount the singleton. */
#define EM7028_HR_BOOT_WAIT_MS                  500U

/** Per-frame wait.  ~8 frame periods of headroom at 40 Hz. */
#define EM7028_HR_FRAME_TIMEOUT_MS              200U

/** RTT heartbeat cadence -- one log line per second at 40 Hz. */
#define EM7028_HR_LOG_EVERY_N_FRAMES            40U

/** Idle delay after fatal setup error. */
#define EM7028_HR_PARK_DELAY_MS                 5000U

/** Result queue depth. */
#define EM7028_HR_RESULT_QUEUE_DEPTH            4U

/**
 * Algorithm selector: 0 = SIMPLE, 1 = BIQUAD.
 * Change this macro to switch algorithm at compile time.
 */
#define EM7028_HR_ALGO_VARIANT                  1

//******************************** Defines **********************************//

//******************************* Declaring *********************************/

/**
 * @brief Queue item delivered to downstream consumers (LVGL, etc.).
 */
typedef struct
{
    uint16_t bpm;
    uint8_t  confidence;
    uint32_t timestamp_ms;
    bool     beat_detected;
} hr_queue_item_t;

/* ---- Volatile globals for J-Scope / debugger observation ---- */
volatile uint16_t g_hr_bpm;
volatile uint8_t  g_hr_confidence;
volatile uint32_t g_hr_frame_cnt;
volatile uint32_t g_hr_drop_cnt;
volatile uint32_t g_hr_last_beat_ts_ms;
volatile int32_t  g_hr_filtered_signal;

/* ---- OSAL result queue handle (extern for downstream consumers) ---- */
osal_queue_handle_t g_hr_result_queue = NULL;

/* ---- PPG sensor config (HRS1-only, 40 Hz pulse) ---- */
static wp_heart_rate_config_t s_hr_cfg =
{
    .enable_hrs1     = true,
    .enable_hrs2     = false,
    .hrs1_gain_high  = false,
    .hrs1_range_high = true,
    .hrs1_freq       = WP_HRS1_FREQ_163K_25MS,
    .hrs1_res        = WP_HRS_RES_14BIT,
    .hrs2_mode       = WP_HRS2_MODE_PULSE,
    .hrs2_wait       = WP_HRS2_WAIT_25MS,
    .hrs2_gain_high  = false,
    .hrs2_pos_mask   = 0x0FU,
    .led_current     = 0x80U,
};

//******************************* Declaring *********************************/

//******************************* Functions *********************************/

/**
 * @brief      Park the task forever after a fatal setup error.
 */
static void em7028_hr_park(void)
{
    for (;;)
    {
        osal_task_delay(EM7028_HR_PARK_DELAY_MS);
    }
}

/**
 * @brief      Apply config, request start.  Mirrors em7028_scope_arm().
 */
static wp_heart_rate_status_t em7028_hr_arm(void)
{
    heart_rate_drv_init();

    wp_heart_rate_status_t ret = heart_rate_drv_reconfigure(&s_hr_cfg);
    if (WP_HEART_RATE_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "hr_task reconfigure failed = %d", (int)ret);
        return ret;
    }

    ret = heart_rate_drv_start();
    if (WP_HEART_RATE_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "hr_task start failed = %d", (int)ret);
        return ret;
    }

    DEBUG_OUT(i, EM7028_LOG_TAG, "hr_task armed: HRS1 @ 40 Hz");
    return WP_HEART_RATE_OK;
}

/**
 * @brief      EM7028 heart-rate calculation task entry.
 *
 * @param[in]  argument : Unused.
 */
void em7028_heart_rate_task(void *argument)
{
    (void)argument;

    DEBUG_OUT(i, EM7028_LOG_TAG, "em7028_heart_rate_task start");

    /* Wait for handler thread to mount singleton. */
    osal_task_delay(EM7028_HR_BOOT_WAIT_MS);

    /* Initialize heart-rate algorithm. */
    hr_algo_config_t algo_cfg;
    hr_algo_state_t  algo_state;

#if (EM7028_HR_ALGO_VARIANT == 0)
    int32_t algo_ret = hr_algo_get_default_config(HR_ALGO_SIMPLE, &algo_cfg);
#else
    int32_t algo_ret = hr_algo_get_default_config(HR_ALGO_BIQUAD, &algo_cfg);
#endif

    if ((0 != algo_ret) || (0 != hr_algo_init(&algo_state, &algo_cfg)))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG, "hr_algo_init failed");
        em7028_hr_park();
    }

    /* Create result queue for downstream consumers. */
    int32_t qret = osal_queue_create(&g_hr_result_queue,
                                     EM7028_HR_RESULT_QUEUE_DEPTH,
                                     sizeof(hr_queue_item_t));
    if (0 != qret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "hr_task create result queue failed: %d", (int)qret);
        em7028_hr_park();
    }

    /* Arm the PPG stream. */
    if (WP_HEART_RATE_OK != em7028_hr_arm())
    {
        em7028_hr_park();
    }

    /* ---- Streaming loop ---- */
    hr_algo_result_t result;
    hr_queue_item_t  queue_item;

    for (;;)
    {
        wp_heart_rate_status_t ret =
            heart_rate_drv_get_req(EM7028_HR_FRAME_TIMEOUT_MS);
        if (WP_HEART_RATE_OK != ret)
        {
            g_hr_drop_cnt++;
            continue;
        }

        wp_ppg_frame_t *p_frame = heart_rate_get_frame_addr();
        if (NULL == p_frame)
        {
            g_hr_drop_cnt++;
            continue;
        }

        /* Feed sample into the algorithm. */
        if (0 != hr_algo_process(&algo_state,
                                 p_frame->hrs1_sum,
                                 p_frame->timestamp_ms,
                                 &result))
        {
            g_hr_drop_cnt++;
            heart_rate_read_data_done();
            continue;
        }

        /* Publish to volatile globals. */
        g_hr_bpm          = result.bpm;
        g_hr_confidence   = result.confidence;
        g_hr_frame_cnt++;
        g_hr_filtered_signal = (int32_t)p_frame->hrs1_sum;

        if (result.beat_detected)
        {
            g_hr_last_beat_ts_ms = result.timestamp_ms;
        }

        /* Publish to result queue; drop the oldest item if the consumer lags. */
        queue_item.bpm           = result.bpm;
        queue_item.confidence    = result.confidence;
        queue_item.timestamp_ms  = result.timestamp_ms;
        queue_item.beat_detected = result.beat_detected;

        if (0 != osal_queue_send(g_hr_result_queue, &queue_item, 0))
        {
            hr_queue_item_t old_item;
            (void)osal_queue_receive(g_hr_result_queue, &old_item, 0);
            if (0 != osal_queue_send(g_hr_result_queue, &queue_item, 0))
            {
                g_hr_drop_cnt++;
            }
        }

        heart_rate_read_data_done();

        /* RTT heartbeat -- one line per second at 40 Hz. */
        if (0U == (g_hr_frame_cnt % EM7028_HR_LOG_EVERY_N_FRAMES))
        {
            DEBUG_OUT(i, EM7028_LOG_TAG,
                      "hr #%u bpm=%u conf=%u drops=%u",
                      (unsigned)g_hr_frame_cnt,
                      (unsigned)g_hr_bpm,
                      (unsigned)g_hr_confidence,
                      (unsigned)g_hr_drop_cnt);
        }
    }
}

//******************************* Functions *********************************/
