/******************************************************************************
 * @file em7028_iic_hal_test_task.c
 *
 * @par dependencies
 * - user_task_reso_config.h
 * - bsp_em7028_handler.h
 * - bsp_em7028_driver.h
 * - osal_wrapper_adapter.h
 * - osal_error.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief EM7028 end-to-end I2C HAL smoke test.
 *
 * Processing flow:
 * The em7028_handler_thread (started separately by user_init from the same
 * em7028_input_arg bundle) takes ownership of the underlying driver and
 * runs bsp_em7028_handler_inst() during its boot. inst implicitly probes
 * the chip ID over the shared sensor I2C bus, so a successful boot proves
 * the integration -> handler -> driver -> hi2c3 path.
 *
 * After boot this task drives a fixed event sequence through the public
 * handler API (no wrapper / platform layer is built yet), pulling frames
 * out of the handler-owned frame queue and validating non-zero PPG data
 * to confirm real I/O is happening:
 *
 *     START -> drain N HRS1 frames -> STOP
 *           -> RECONFIGURE (HRS1+HRS2) -> START
 *           -> drain N dual-channel frames -> STOP
 *
 * Each step records a row in s_records; the final summary is dumped to
 * the EM7028 RTT terminal (DEBUG_RTT_CH_PPG).
 *
 * @version V1.0 2026-05-05
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
#include "bsp_em7028_handler.h"
#include "bsp_em7028_driver.h"
#include "osal_wrapper_adapter.h"
#include "osal_error.h"
#include "Debug.h"
//******************************** Includes *********************************//

/**
 * Whole-file gate. Default OFF — production builds run the production
 * em7028_heart_rate_task instead. Flip USER_TASK_EM7028_IIC_HAL_TEST to 1
 * in user_task_reso_config.h to compile this smoke-test path; note the
 * frame queue is single-consumer so only one of {IIC_HAL_TEST,
 * JSCOPE_CAPTURE, HEART_RATE} can be enabled at a time.
 */
#if USER_TASK_EM7028_IIC_HAL_TEST

//******************************** Defines **********************************//
/** Wait for em7028_handler_thread to finish bsp_em7028_handler_inst. The
 *  inst auto-runs the driver's pf_init which itself spends ~12 ms on boot
 *  delay + soft reset; 500 ms is comfortably above that.                  */
#define EM7028_HAL_BOOT_WAIT_MS              500U

/** Per-frame wait. Default cfg is 40 Hz pulse mode (~25 ms/frame); allow
 *  a few periods of slack so transient I2C jitter on the shared sensor
 *  bus does not flake the test.                                           */
#define EM7028_HAL_FRAME_TIMEOUT_MS          200U

/** Cmd queue submit deadline -- handler thread should always be ready to
 *  consume; a small ceiling catches the case where the dispatch loop is
 *  wedged.                                                                */
#define EM7028_HAL_CMD_TIMEOUT_MS            100U

/** Number of frames to drain per active phase.                            */
#define EM7028_HAL_FRAME_BURST_COUNT          20U

/** Inter-step gap so RTT log lines stay readable.                         */
#define EM7028_HAL_GAP_MS                     50U
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
typedef enum
{
    EM7028_HAL_TC_BOOT_PROBE = 0,        /* implicit chip-id probe via inst */
    EM7028_HAL_TC_START_HRS1,
    EM7028_HAL_TC_DRAIN_HRS1,
    EM7028_HAL_TC_STOP_HRS1,
    EM7028_HAL_TC_RECONFIGURE,
    EM7028_HAL_TC_START_BOTH,
    EM7028_HAL_TC_DRAIN_BOTH,
    EM7028_HAL_TC_STOP_BOTH,
    EM7028_HAL_TC_COUNT
} em7028_hal_tc_idx_t;

/**
 * @brief One row of the final test report.
 */
typedef struct
{
    char const *name;
    bool        ok;
    int32_t     status;     /* last em7028_handler_status_t observed         */
    uint32_t    detail;     /* frame count / step-specific counter           */
    uint32_t    elapsed_ms;
} em7028_hal_record_t;

static em7028_hal_record_t s_records[EM7028_HAL_TC_COUNT];

static uint32_t s_pass_count;
static uint32_t s_fail_count;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief      Record the outcome of one test case for the final report.
 *
 * @param[in]  idx        : Slot index in s_records.
 * @param[in]  name       : Human-readable case label.
 * @param[in]  ok         : true on pass.
 * @param[in]  status     : Last observed handler status code.
 * @param[in]  detail     : Step-specific counter (e.g. drained-frame count).
 * @param[in]  elapsed_ms : Wall-clock elapsed time for the case.
 *
 * @return     None.
 * */
static void em7028_hal_record(em7028_hal_tc_idx_t idx,
                              char const         *name,
                              bool                ok,
                              int32_t             status,
                              uint32_t            detail,
                              uint32_t            elapsed_ms)
{
    s_records[idx].name       = name;
    s_records[idx].ok         = ok;
    s_records[idx].status     = status;
    s_records[idx].detail     = detail;
    s_records[idx].elapsed_ms = elapsed_ms;

    if (ok)
    {
        s_pass_count++;
    }
    else
    {
        s_fail_count++;
    }

    DEBUG_OUT(i, EM7028_LOG_TAG,
              "[%s] %s status=%d detail=%u t=%u ms",
              ok ? "PASS" : "FAIL",
              name, (int)status, (unsigned)detail, (unsigned)elapsed_ms);
}

/**
 * @brief      Drain up to `target` frames from the handler frame queue,
 *             scoring per-channel liveness separately.
 *
 * @param[in]  expect_hrs1   : true if HRS1 is expected to be enabled.
 * @param[in]  expect_hrs2   : true if HRS2 is expected to be enabled.
 * @param[in]  target        : Number of frames to consume.
 * @param[out] p_alive_hrs1  : Number of frames where HRS1 sum was non-zero.
 * @param[out] p_alive_hrs2  : Number of frames where HRS2 sum was non-zero.
 *
 * @return     EM7028_HANDLER_OK once `target` frames are drained, or the
 *             handler error code returned by bsp_em7028_handler_get_frame
 *             if a frame fails to arrive within EM7028_HAL_FRAME_TIMEOUT_MS.
 * */
static em7028_handler_status_t em7028_hal_drain_frames(bool      expect_hrs1,
                                                       bool      expect_hrs2,
                                                       uint32_t  target,
                                                       uint32_t *p_alive_hrs1,
                                                       uint32_t *p_alive_hrs2)
{
    uint32_t alive_hrs1 = 0U;
    uint32_t alive_hrs2 = 0U;

    for (uint32_t i = 0U; i < target; i++)
    {
        em7028_ppg_frame_t frame;
        memset(&frame, 0, sizeof(frame));

        em7028_handler_status_t ret =
            bsp_em7028_handler_get_frame(&frame, EM7028_HAL_FRAME_TIMEOUT_MS);
        if (EM7028_HANDLER_OK != ret)
        {
            DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                      "get_frame[%u/%u] failed = %d",
                      (unsigned)i, (unsigned)target, (int)ret);
            if (NULL != p_alive_hrs1)
            {
                *p_alive_hrs1 = alive_hrs1;
            }
            if (NULL != p_alive_hrs2)
            {
                *p_alive_hrs2 = alive_hrs2;
            }
            return ret;
        }

        /**
         * Liveness check per channel. The first 1-2 frames after START
         * may be all-zero while the ADC settles; this is expected, so
         * the pass threshold below tolerates a few zero frames.
         **/
        if ((!expect_hrs1) || (frame.hrs1_sum != 0U))
        {
            alive_hrs1++;
        }
        if ((!expect_hrs2) || (frame.hrs2_sum != 0U))
        {
            alive_hrs2++;
        }

        /* Log every 5th frame so RTT bandwidth stays sane during the burst. */
        if (0U == (i % 5U))
        {
            DEBUG_OUT(i, EM7028_LOG_TAG,
                      "frame[%u] ts=%u hrs1=%u hrs2=%u",
                      (unsigned)i,
                      (unsigned)frame.timestamp_ms,
                      (unsigned)frame.hrs1_sum,
                      (unsigned)frame.hrs2_sum);
        }
    }

    if (NULL != p_alive_hrs1)
    {
        *p_alive_hrs1 = alive_hrs1;
    }
    if (NULL != p_alive_hrs2)
    {
        *p_alive_hrs2 = alive_hrs2;
    }
    return EM7028_HANDLER_OK;
}

/**
 * @brief      Print the per-case summary and aggregate counters.
 *
 * @return     None.
 * */
static void em7028_hal_print_report(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "============== EM7028 IIC HAL REPORT ==============");

    for (uint32_t i = 0U; i < (uint32_t)EM7028_HAL_TC_COUNT; i++)
    {
        if (NULL == s_records[i].name)
        {
            continue;
        }
        DEBUG_OUT(i, EM7028_LOG_TAG,
                  "  %02u | %-18s | %-4s | status=%d | detail=%u | %u ms",
                  (unsigned)i,
                  s_records[i].name,
                  s_records[i].ok ? "PASS" : "FAIL",
                  (int)s_records[i].status,
                  (unsigned)s_records[i].detail,
                  (unsigned)s_records[i].elapsed_ms);
    }

    DEBUG_OUT(i, EM7028_LOG_TAG,
              "  TOTAL : %u passed, %u failed (of %u cases)",
              (unsigned)s_pass_count, (unsigned)s_fail_count,
              (unsigned)EM7028_HAL_TC_COUNT);
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===================================================");
}

/**
 * @brief      EM7028 IIC HAL test task entry.
 *
 *             Drives a fixed event sequence against the real chip through
 *             the public handler API and publishes a one-shot pass/fail
 *             report. Idles after the report so the task stack stays
 *             available for postmortem inspection.
 *
 * @param[in]  argument : Unused (handler thread owns the input args).
 *
 * @return     None (task entry).
 * */
void em7028_iic_hal_test_task(void *argument)
{
    (void)argument;

    DEBUG_OUT(i, EM7028_LOG_TAG, "em7028_iic_hal_test_task start");

    /**
     * Give em7028_handler_thread time to run bsp_em7028_handler_inst()
     * (mounts the singleton + auto-inits the chip via the I2C bus). Until
     * that mount completes, every public API returns ERRORNOTINIT.
     **/
    osal_task_delay(EM7028_HAL_BOOT_WAIT_MS);

    em7028_handler_status_t ret = EM7028_HANDLER_OK;
    uint32_t                t_start = 0U;
    uint32_t                t_end   = 0U;
    uint32_t                alive_hrs1 = 0U;
    uint32_t                alive_hrs2 = 0U;
    bool                    ok      = false;

    /* ===== CASE 0: BOOT PROBE ============================================
     *
     * The chip-id probe ran inside handler_inst long before this task woke
     * up; the only handle we have to that result is the public API: if
     * any subsequent call returns OK then the inst path completed. Use
     * STOP as a side-effect-free probe -- it is idempotent against an
     * already-stopped handler and exercises the cmd queue end-to-end.
     */
    t_start = osal_task_get_tick_count();
    ret     = bsp_em7028_handler_stop();
    t_end   = osal_task_get_tick_count();
    ok      = (EM7028_HANDLER_OK == ret);
    em7028_hal_record(EM7028_HAL_TC_BOOT_PROBE, "BOOT_PROBE",
                      ok, (int32_t)ret, 0U, t_end - t_start);
    if (!ok)
    {
        /**
         * Handler never came up -- there is no I2C path to test. Skip the
         * remaining cases, dump the report, idle.
         **/
        em7028_hal_print_report();
        for (;;) { osal_task_delay(5000U); }
    }
    osal_task_delay(EM7028_HAL_GAP_MS);

    /* ===== CASE 1: START (HRS1 only -- driver default cfg) =============== */
    t_start = osal_task_get_tick_count();
    ret     = bsp_em7028_handler_start();
    t_end   = osal_task_get_tick_count();
    ok      = (EM7028_HANDLER_OK == ret);
    em7028_hal_record(EM7028_HAL_TC_START_HRS1, "START_HRS1",
                      ok, (int32_t)ret, 0U, t_end - t_start);
    osal_task_delay(EM7028_HAL_GAP_MS);

    /* ===== CASE 2: drain HRS1 frames (real I2C bursts) =================== *
     * The first 1-3 frames after START may be zero while the PPG LED and
     * ADC settle; accept >= 70 % as a pass.                               */
    t_start = osal_task_get_tick_count();
    alive_hrs1 = 0U;
    ret     = em7028_hal_drain_frames(true, false,
                                      EM7028_HAL_FRAME_BURST_COUNT,
                                      &alive_hrs1, NULL);
    t_end   = osal_task_get_tick_count();
    ok = (EM7028_HANDLER_OK == ret) &&
         (alive_hrs1 >= (EM7028_HAL_FRAME_BURST_COUNT * 70U / 100U));
    em7028_hal_record(EM7028_HAL_TC_DRAIN_HRS1, "DRAIN_HRS1",
                      ok, (int32_t)ret, alive_hrs1, t_end - t_start);
    osal_task_delay(EM7028_HAL_GAP_MS);

    /* ===== CASE 3: STOP =================================================== */
    t_start = osal_task_get_tick_count();
    ret     = bsp_em7028_handler_stop();
    t_end   = osal_task_get_tick_count();
    ok      = (EM7028_HANDLER_OK == ret);
    em7028_hal_record(EM7028_HAL_TC_STOP_HRS1, "STOP_HRS1",
                      ok, (int32_t)ret, 0U, t_end - t_start);
    osal_task_delay(EM7028_HAL_GAP_MS);

    /* ===== CASE 4: RECONFIGURE (HRS1 + HRS2) ============================== *
     * Same 40 Hz pulse mode, 14-bit ADC; flip enable_hrs2 and tighten
     * led_current so the dual-channel path is exercised.                    */
    em7028_config_t cfg = {
        .enable_hrs1     = true,
        .enable_hrs2     = true,
        .hrs1_gain_high  = true,
        .hrs1_range_high = true,
        .hrs1_freq       = EM7028_HRS1_FREQ_163K_25MS,
        .hrs1_res        = EM7028_HRS_RES_14BIT,
        .hrs2_mode       = EM7028_HRS2_MODE_PULSE,
        .hrs2_wait       = EM7028_HRS2_WAIT_25MS,
        .hrs2_gain_high  = false,
        .hrs2_pos_mask   = 0x0FU,
        .led_current     = 0x80U,
    };
    t_start = osal_task_get_tick_count();
    ret     = bsp_em7028_handler_reconfigure(&cfg);
    t_end   = osal_task_get_tick_count();
    ok      = (EM7028_HANDLER_OK == ret);
    em7028_hal_record(EM7028_HAL_TC_RECONFIGURE, "RECONFIGURE",
                      ok, (int32_t)ret, 0U, t_end - t_start);
    osal_task_delay(EM7028_HAL_GAP_MS);

    /* ===== CASE 5: START (after reconfigure) ============================== */
    t_start = osal_task_get_tick_count();
    ret     = bsp_em7028_handler_start();
    t_end   = osal_task_get_tick_count();
    ok      = (EM7028_HANDLER_OK == ret);
    em7028_hal_record(EM7028_HAL_TC_START_BOTH, "START_BOTH",
                      ok, (int32_t)ret, 0U, t_end - t_start);
    osal_task_delay(EM7028_HAL_GAP_MS);

    /* ===== CASE 6: drain HRS1+HRS2 frames ================================ *
     * HRS1 is the primary channel (always strong); HRS2 may be very weak
     * (sum ~0-4 counts) depending on LED position and skin perfusion, so
     * we only require HRS2 to show *some* non-zero activity (>= 1) and
     * HRS1 to meet the 70 % threshold.                                    */
    t_start = osal_task_get_tick_count();
    alive_hrs1 = 0U;
    alive_hrs2 = 0U;
    ret     = em7028_hal_drain_frames(true, true,
                                      EM7028_HAL_FRAME_BURST_COUNT,
                                      &alive_hrs1, &alive_hrs2);
    t_end   = osal_task_get_tick_count();
    ok = (EM7028_HANDLER_OK == ret) &&
         (alive_hrs1 >= (EM7028_HAL_FRAME_BURST_COUNT * 70U / 100U)) &&
         (alive_hrs2 >= 1U);
    em7028_hal_record(EM7028_HAL_TC_DRAIN_BOTH, "DRAIN_BOTH",
                      ok, (int32_t)ret, alive_hrs1, t_end - t_start);
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "  HRS1=%u/%u HRS2=%u/%u",
              (unsigned)alive_hrs1, (unsigned)EM7028_HAL_FRAME_BURST_COUNT,
              (unsigned)alive_hrs2, (unsigned)EM7028_HAL_FRAME_BURST_COUNT);
    osal_task_delay(EM7028_HAL_GAP_MS);

    /* ===== CASE 7: STOP =================================================== */
    t_start = osal_task_get_tick_count();
    ret     = bsp_em7028_handler_stop();
    t_end   = osal_task_get_tick_count();
    ok      = (EM7028_HANDLER_OK == ret);
    em7028_hal_record(EM7028_HAL_TC_STOP_BOTH, "STOP_BOTH",
                      ok, (int32_t)ret, 0U, t_end - t_start);

    /* ===== Final report ================================================== */
    em7028_hal_print_report();

    /**
     * Test sequence is one-shot. Idle forever so the task's stack stays
     * available for postmortem inspection (high-water monitor still runs
     * even after the test finishes).
     **/
    for (;;)
    {
        osal_task_delay(5000U);
    }
}

#endif /* USER_TASK_EM7028_IIC_HAL_TEST */
//******************************* Functions *********************************//
