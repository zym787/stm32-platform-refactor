/******************************************************************************
 * @file w25q64_hal_test_task.c
 *
 * @par dependencies
 * - user_task_reso_config.h
 * - bsp_wrapper_externflash.h
 * - osal_wrapper_adapter.h
 * - osal_error.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief External-flash end-to-end smoke test through the externflash
 *        wrapper API.  No layer below the wrapper is referenced --
 *        a successful run proves that wrapper -> adapter -> handler ->
 *        driver path is wired correctly against real SPI1 hardware.
 *
 * Processing flow:
 * The flash_handler_thread (started separately by user_init from the same
 * w25q64_input_arg bundle) owns the driver instance and the event queue.
 * This task pushes a sequence of requests through externflash_drv_*() and
 * waits on a binary semaphore that the shared wrapper-level callback gives
 * back from the handler thread, so each step runs fully serialised:
 *     WAKEUP -> ERASE_SECTOR -> WRITE -> READ + verify
 *            -> ERASE_CHIP   -> READ + verify (all 0xFF)
 *            -> SLEEP        -> WAKEUP
 * After the sequence finishes a PASS/FAIL summary is dumped to the W25Q64
 * RTT terminal.  CHIP_ERASE on a W25Q64 takes up to ~30 s so the wait is
 * sized accordingly.
 *
 * @version V1.0 2026-04-27
 * @version V2.0 2026-05-03
 * @upgrade 2.0: Migrated from direct handler_flash_event_send() to the
 *               externflash wrapper API.  The READ_ID (JEDEC) case is
 *               removed -- it is a device-specific operation not exposed
 *               by the portable wrapper.
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
#include "bsp_wrapper_externflash.h"
#include "osal_wrapper_adapter.h"
#include "osal_error.h"
#include "Debug.h"
//******************************** Includes *********************************//

/**
 * Whole-file gate. Production builds default to OFF — the task is never
 * created (g_user_task_cfg[] entry is also gated by the same macro) and
 * the __WEAK__ stub in user_task_reso_config.c picks up the symbol.
 * Flip USER_TASK_W25Q64_HAL_TEST to 1 in user_task_reso_config.h to
 * bring up real hardware.
 */
#if USER_TASK_W25Q64_HAL_TEST

//******************************** Defines **********************************//
/** Wait for flash_handler_thread to finish bsp_w25q64_handler_inst(). */
#define W25Q64_HAL_BOOT_WAIT_MS         500U

/** Test region: well inside the 8 MB part, sector-aligned. */
#define W25Q64_HAL_TEST_ADDR            (0x000010U)
#define W25Q64_HAL_TEST_PATTERN_LEN     128U

/** Per-event timeouts, in OSAL ticks (== ms with 1 kHz tick). */
#define W25Q64_HAL_TIMEOUT_FAST_MS      500U   /* sleep / wakeup            */
#define W25Q64_HAL_TIMEOUT_SECTOR_MS    1000U  /* sector erase ~50 ms typ.  */
#define W25Q64_HAL_TIMEOUT_RW_MS        2000U  /* page write + sector erase */
#define W25Q64_HAL_TIMEOUT_CHIP_MS      40000U /* chip erase up to ~30 s    */

#define W25Q64_HAL_GAP_MS               50U
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/**
 * @brief One row of the final test report.
 */
typedef struct
{
    const char              *  name;
    bool                         ok;
    platform_err_t  status;
    uint32_t             elapsed_ms;
} hal_test_record_t;

typedef enum
{
    HAL_TC_WAKEUP_BOOT = 0,
    HAL_TC_ERASE_SECTOR,
    HAL_TC_WRITE,
    HAL_TC_READ_VERIFY,
    HAL_TC_ERASE_CHIP,
    HAL_TC_READ_BLANK,
    HAL_TC_SLEEP,
    HAL_TC_WAKEUP_END,
    HAL_TC_COUNT
} hal_tc_idx_t;

static osal_sema_handle_t s_done_sema = NULL;

/** Shared latch filled by the wrapper callback before s_done_sema is given. */
static volatile platform_err_t s_last_status =
                                            PLATFORM_ERR_RESERVED;

/** Buffers used across the test sequence. */
static uint8_t  s_write_buf[W25Q64_HAL_TEST_PATTERN_LEN];
static uint8_t  s_read_buf [W25Q64_HAL_TEST_PATTERN_LEN];

static hal_test_record_t s_records[HAL_TC_COUNT];
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief  Shared wrapper-level completion callback fired from
 *         flash_handler_thread via the adapter trampoline.
 *
 * @param[in] result : Pointer to the wrapper-level completion record.
 *                     The handler / adapter has filled `status` with the
 *                     operation result before invoking us.
 *
 * @return None.
 * */
static void w25q64_hal_event_done_cb(wp_externflash_result_t *result)
{
    /**
     * Latch the status into a file-scope variable, then unblock the test
     * task.  The callback runs in flash_handler_thread context, so giving
     * the semaphore is the cheapest way to hand control back to us.
     */
    if (NULL != result)
    {
        s_last_status = result->status;
    }
    (void)osal_sema_give(s_done_sema);
}

/**
 * @brief  Block on the completion semaphore with the supplied timeout.
 *
 *         Reset the latch before each call so we never observe a stale
 *         status from the previous step.  OSAL_MAX_DELAY would block
 *         forever if the handler thread crashed, so use an explicit
 *         per-step ceiling instead.
 *
 * @param[in] timeout_ms : Max wait for the callback in milliseconds.
 *
 * @return PLATFORM_OK on success, error code otherwise.
 * */
static platform_err_t w25q64_hal_wait_done(uint32_t timeout_ms)
{
    int32_t take_ret = osal_sema_take(s_done_sema,
                                      (osal_tick_type_t)timeout_ms);
    if (OSAL_SUCCESS != take_ret)
    {
        DEBUG_OUT(e, W25Q64_HAL_TEST_ERR_LOG_TAG,
                  "wrapper event timeout after %u ms",
                  (unsigned)timeout_ms);
        return PLATFORM_ERR_TIMEOUT;
    }
    return s_last_status;
}

/**
 * @brief  Reset the completion latch before each new submit.
 * */
static void w25q64_hal_reset_latch(void)
{
    s_last_status = PLATFORM_ERR_RESERVED;
}

/**
 * @brief  Fill the write buffer with a deterministic pattern that catches
 *         both bit-flips and byte-mis-ordering errors.
 * */
static void w25q64_hal_fill_pattern(uint8_t *p_buf, uint32_t len)
{
    /* Pattern: i ^ 0xA5 -- distinct per byte, exercises every bit. */
    for (uint32_t i = 0U; i < len; i++)
    {
        p_buf[i] = (uint8_t)((i & 0xFFU) ^ 0xA5U);
    }
}

/**
 * @brief  Compare two byte buffers for equality.
 *
 * @return true on full match, false otherwise.
 * */
static bool w25q64_hal_buf_equal(uint8_t const *a, uint8_t const *b,
                                 uint32_t       len)
{
    /* memcmp would do, but inline-loop logs the first mismatch index. */
    for (uint32_t i = 0U; i < len; i++)
    {
        if (a[i] != b[i])
        {
            DEBUG_OUT(e, W25Q64_HAL_TEST_ERR_LOG_TAG,
                      "mismatch @%u: got 0x%02X exp 0x%02X",
                      (unsigned)i, (unsigned)a[i], (unsigned)b[i]);
            return false;
        }
    }
    return true;
}

/**
 * @brief  Check that every byte in `buf` equals `expected`.
 *
 * @return true on full match, false otherwise.
 * */
static bool w25q64_hal_buf_all_eq(uint8_t const *buf, uint32_t len,
                                  uint8_t        expected)
{
    for (uint32_t i = 0U; i < len; i++)
    {
        if (buf[i] != expected)
        {
            DEBUG_OUT(e, W25Q64_HAL_TEST_ERR_LOG_TAG,
                      "blank check fail @%u: got 0x%02X exp 0x%02X",
                      (unsigned)i, (unsigned)buf[i], (unsigned)expected);
            return false;
        }
    }
    return true;
}

/**
 * @brief  Record the outcome of one test case for the final report.
 * */
static void w25q64_hal_record(hal_tc_idx_t            idx,
                              const char            *name,
                              bool                     ok,
                              platform_err_t status,
                              uint32_t        elapsed_ms)
{
    s_records[idx].name       = name;
    s_records[idx].ok         = ok;
    s_records[idx].status     = status;
    s_records[idx].elapsed_ms = elapsed_ms;

    DEBUG_OUT(i, W25Q64_HAL_TEST_LOG_TAG,
              "[%s] %s status=%d t=%u ms",
              ok ? "PASS" : "FAIL",
              name, (int)status, (unsigned)elapsed_ms);
}

/**
 * @brief  Print the per-case summary and aggregate counters.
 * */
static void w25q64_hal_print_report(void)
{
    /**
     * Report layout mirrors a CI test runner: each row carries case name,
     * status code and wall-clock time, followed by the totals line.  All
     * rows go to the W25Q64 RTT terminal so they are easy to extract.
     */
    uint32_t pass = 0U;
    uint32_t fail = 0U;

    DEBUG_OUT(i, W25Q64_HAL_TEST_LOG_TAG,
              "============== W25Q64 WRAPPER HAL REPORT ==============");

    for (uint32_t i = 0U; i < (uint32_t)HAL_TC_COUNT; i++)
    {
        if (NULL == s_records[i].name)
        {
            continue;
        }
        if (s_records[i].ok)
        {
            pass++;
        }
        else
        {
            fail++;
        }
        DEBUG_OUT(i, W25Q64_HAL_TEST_LOG_TAG,
                  "  %02u | %-20s | %-4s | status=%d | %u ms",
                  (unsigned)i,
                  s_records[i].name,
                  s_records[i].ok ? "PASS" : "FAIL",
                  (int)s_records[i].status,
                  (unsigned)s_records[i].elapsed_ms);
    }

    DEBUG_OUT(i, W25Q64_HAL_TEST_LOG_TAG,
              "  TOTAL : %u passed, %u failed (of %u cases)",
              (unsigned)pass, (unsigned)fail,
              (unsigned)HAL_TC_COUNT);
    DEBUG_OUT(i, W25Q64_HAL_TEST_LOG_TAG,
              "=======================================================");
}

/**
 * @brief  W25Q64 HAL test task entry.
 *
 *         Drives a fixed event sequence against the real flash through the
 *         externflash wrapper and publishes a one-shot pass/fail report.
 *         After the report the task idles forever so it does not steal
 *         CPU from the rest of the system.
 *
 * @param[in] argument : Unused.  The handler thread holds the input args.
 *
 * @return None (task entry).
 * */
void w25q64_hal_test_task(void *argument)
{
    (void)argument;

    DEBUG_OUT(i, W25Q64_HAL_TEST_LOG_TAG, "w25q64_hal_test_task start");

    /**
     * Counting semaphore with max=1, init=0 -- behaves as a binary
     * completion flag for the request/response pattern.
     */
    int32_t sema_ret = osal_sema_init(&s_done_sema, 0U);
    if (OSAL_SUCCESS != sema_ret)
    {
        DEBUG_OUT(e, W25Q64_HAL_TEST_ERR_LOG_TAG,
                  "sema_init failed = %d", (int)sema_ret);
        for (;;) { osal_task_delay(1000U); }
    }

    /**
     * Give flash_handler_thread time to run bsp_w25q64_handler_inst()
     * and call __mount_handler() before we send the first event.  Without
     * this the very first wrapper call returns ERRORRESOURCE because the
     * underlying gp_flash_instance is still NULL.
     */
    osal_task_delay(W25Q64_HAL_BOOT_WAIT_MS);

    /* No-op vtable init: hardware bring-up happens inside the handler
     * thread.  Calling this here documents the lifecycle expected by the
     * wrapper API and is harmless. */
    externflash_drv_init();

    platform_err_t ret      = PLATFORM_OK;
    uint32_t                t_start  = 0U;
    uint32_t                t_end    = 0U;
    bool                    ok       = false;

    /* ===== CASE 0: WAKEUP (boot probe) ================================== */
    /**
     * If the part was left in deep power-down across MCU reset, every
     * subsequent command would silently fail.  Issue an explicit wake-up
     * first so later cases run from a known device state.
     */
    w25q64_hal_reset_latch();
    t_start = osal_task_get_tick_count();
    ret     = externflash_drv_wakeup(w25q64_hal_event_done_cb, NULL);
    if (PLATFORM_OK == ret)
    {
        ret = w25q64_hal_wait_done(W25Q64_HAL_TIMEOUT_FAST_MS);
    }
    t_end   = osal_task_get_tick_count();
    ok = (PLATFORM_OK == ret);
    w25q64_hal_record(HAL_TC_WAKEUP_BOOT, "WAKEUP_BOOT",
                      ok, ret, t_end - t_start);
    osal_task_delay(W25Q64_HAL_GAP_MS);

    /* ===== CASE 1: ERASE_SECTOR ========================================= */
    w25q64_hal_reset_latch();
    t_start = osal_task_get_tick_count();
    ret     = externflash_drv_erase_sector(W25Q64_HAL_TEST_ADDR,
                                           w25q64_hal_event_done_cb, NULL);
    if (PLATFORM_OK == ret)
    {
        ret = w25q64_hal_wait_done(W25Q64_HAL_TIMEOUT_SECTOR_MS);
    }
    t_end   = osal_task_get_tick_count();
    ok = (PLATFORM_OK == ret);
    w25q64_hal_record(HAL_TC_ERASE_SECTOR, "ERASE_SECTOR",
                      ok, ret, t_end - t_start);
    osal_task_delay(W25Q64_HAL_GAP_MS);

    /* ===== CASE 2: WRITE (page-aligned, with internal erase) ============ */
    w25q64_hal_fill_pattern(s_write_buf, sizeof(s_write_buf));
    w25q64_hal_reset_latch();
    t_start = osal_task_get_tick_count();
    ret     = externflash_drv_write(W25Q64_HAL_TEST_ADDR,
                                    s_write_buf,
                                    (uint32_t)sizeof(s_write_buf),
                                    w25q64_hal_event_done_cb, NULL);
    if (PLATFORM_OK == ret)
    {
        ret = w25q64_hal_wait_done(W25Q64_HAL_TIMEOUT_RW_MS);
    }
    t_end   = osal_task_get_tick_count();
    ok = (PLATFORM_OK == ret);
    w25q64_hal_record(HAL_TC_WRITE, "WRITE", ok, ret, t_end - t_start);
    osal_task_delay(W25Q64_HAL_GAP_MS);

    /* ===== CASE 3: READ + verify against pattern ======================== */
    memset(s_read_buf, 0, sizeof(s_read_buf));
    w25q64_hal_reset_latch();
    t_start = osal_task_get_tick_count();
    ret     = externflash_drv_read(W25Q64_HAL_TEST_ADDR,
                                   s_read_buf,
                                   (uint32_t)sizeof(s_read_buf),
                                   w25q64_hal_event_done_cb, NULL);
    if (PLATFORM_OK == ret)
    {
        ret = w25q64_hal_wait_done(W25Q64_HAL_TIMEOUT_RW_MS);
    }
    t_end   = osal_task_get_tick_count();
    ok = (PLATFORM_OK == ret) &&
         w25q64_hal_buf_equal(s_read_buf, s_write_buf,
                              (uint32_t)sizeof(s_read_buf));
    w25q64_hal_record(HAL_TC_READ_VERIFY, "READ_VERIFY",
                      ok, ret, t_end - t_start);
    osal_task_delay(W25Q64_HAL_GAP_MS);

    /* ===== CASE 4: ERASE_CHIP (slow -- up to ~30 s) ===================== */
    w25q64_hal_reset_latch();
    t_start = osal_task_get_tick_count();
    ret     = externflash_drv_erase_chip(w25q64_hal_event_done_cb, NULL);
    if (PLATFORM_OK == ret)
    {
        ret = w25q64_hal_wait_done(W25Q64_HAL_TIMEOUT_CHIP_MS);
    }
    t_end   = osal_task_get_tick_count();
    ok = (PLATFORM_OK == ret);
    w25q64_hal_record(HAL_TC_ERASE_CHIP, "ERASE_CHIP",
                      ok, ret, t_end - t_start);
    osal_task_delay(W25Q64_HAL_GAP_MS);

    /* ===== CASE 5: READ -> all 0xFF (post chip-erase blank check) ======= */
    memset(s_read_buf, 0, sizeof(s_read_buf));
    w25q64_hal_reset_latch();
    t_start = osal_task_get_tick_count();
    ret     = externflash_drv_read(W25Q64_HAL_TEST_ADDR,
                                   s_read_buf,
                                   (uint32_t)sizeof(s_read_buf),
                                   w25q64_hal_event_done_cb, NULL);
    if (PLATFORM_OK == ret)
    {
        ret = w25q64_hal_wait_done(W25Q64_HAL_TIMEOUT_RW_MS);
    }
    t_end   = osal_task_get_tick_count();
    ok = (PLATFORM_OK == ret) &&
         w25q64_hal_buf_all_eq(s_read_buf,
                               (uint32_t)sizeof(s_read_buf), 0xFFU);
    w25q64_hal_record(HAL_TC_READ_BLANK, "READ_BLANK",
                      ok, ret, t_end - t_start);
    osal_task_delay(W25Q64_HAL_GAP_MS);

    /* ===== CASE 6: SLEEP =============================================== */
    w25q64_hal_reset_latch();
    t_start = osal_task_get_tick_count();
    ret     = externflash_drv_sleep(w25q64_hal_event_done_cb, NULL);
    if (PLATFORM_OK == ret)
    {
        ret = w25q64_hal_wait_done(W25Q64_HAL_TIMEOUT_FAST_MS);
    }
    t_end   = osal_task_get_tick_count();
    ok = (PLATFORM_OK == ret);
    w25q64_hal_record(HAL_TC_SLEEP, "SLEEP", ok, ret, t_end - t_start);
    osal_task_delay(W25Q64_HAL_GAP_MS);

    /* ===== CASE 7: WAKEUP ============================================== */
    w25q64_hal_reset_latch();
    t_start = osal_task_get_tick_count();
    ret     = externflash_drv_wakeup(w25q64_hal_event_done_cb, NULL);
    if (PLATFORM_OK == ret)
    {
        ret = w25q64_hal_wait_done(W25Q64_HAL_TIMEOUT_FAST_MS);
    }
    t_end   = osal_task_get_tick_count();
    ok = (PLATFORM_OK == ret);
    w25q64_hal_record(HAL_TC_WAKEUP_END, "WAKEUP_END", ok, ret, t_end - t_start);
    osal_task_delay(W25Q64_HAL_GAP_MS);

    /* ===== Final report =================================================*/
    w25q64_hal_print_report();

    /**
     * Test sequence is one-shot.  Idle forever so the task's stack stays
     * available for postmortem inspection (high-water monitor still runs
     * even after the test finishes).
     */
    for (;;)
    {
        osal_task_delay(5000U);
    }
}

#endif /* USER_TASK_W25Q64_HAL_TEST */
//******************************* Functions *********************************//
