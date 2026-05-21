/******************************************************************************
 * @file em7028_jscope_capture_task.c
 *
 * @par dependencies
 * - user_task_reso_config.h
 * - bsp_wrapper_heart_rate.h
 * - bsp_em7028_driver.h    (only for ID_REG / register defs in raw dump)
 * - em7028_integration.h   (only for raw I2C interface in raw dump)
 * - osal_wrapper_adapter.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief EM7028 raw PPG capture task for host-side curve plotting.
 *
 * Processing flow:
 * 1. Wait for em7028_handler_thread to finish bsp_em7028_handler_inst()
 *    so the wrapper-vtable forwards land on a fully mounted handler.
 * 2. Push a single-channel (HRS1) 40 Hz pulse-mode config through the
 *    heart_rate wrapper and call heart_rate_drv_start() to begin
 *    streaming frames.
 * 3. Forever pull frames via heart_rate_drv_get_req() +
 *    heart_rate_get_frame_addr() and republish them to a fixed set of
 *    external-linkage globals:
 *      - g_em7028_scope_ring[256]   : ring of full wp_ppg_frame_t,
 *                                     for Ozone "Memory" / "Watched
 *                                     Memory" history dumps.
 *      - g_em7028_scope_w_idx       : ring write index, advanced AFTER
 *                                     the slot is fully written so any
 *                                     halt-time inspection sees a
 *                                     coherent snapshot.
 *      - g_em7028_scope_hrs1_p[4],
 *        g_em7028_scope_hrs2_p[4],
 *        g_em7028_scope_hrs1_sum,
 *        g_em7028_scope_hrs2_sum,
 *        g_em7028_scope_ts_ms       : "latest sample" scalar mirrors,
 *                                     designed for J-Scope subscription.
 *      - g_em7028_scope_frame_cnt   : sticky frame counter (J-Scope x).
 *      - g_em7028_scope_drop_cnt    : non-OK get_req counter.
 *
 * Note: only ONE consumer may pull from the underlying handler frame
 * queue at a time. Keep USER_TASK_EM7028_IIC_HAL_TEST disabled while
 * this task is enabled.
 *
 * @version V1.0 2026-05-07
 * @version V2.0 2026-05-07
 * @upgrade 2.0: Migrated streaming + lifecycle to bsp_wrapper_heart_rate;
 *               raw-register dump still bypasses the abstraction (the
 *               wrapper does not expose register access) and uses the
 *               EM7028 integration's I2C interface directly.
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
#include "bsp_em7028_driver.h"
#include "bsp_em7028_reg.h"
#include "em7028_integration.h"
#include "osal_wrapper_adapter.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/** Wait for em7028_handler_thread's bsp_em7028_handler_inst() to mount
 *  the singleton and auto-init the chip; until then every wrapper call
 *  returns ERRORNOTINIT. inst spends ~12 ms on boot delay + soft reset,
 *  500 ms gives comfortable slack.                                       */
#define EM7028_SCOPE_BOOT_WAIT_MS                500U

/** Per-frame wait. Default cfg is 40 Hz (~25 ms/frame); 200 ms is ~8
 *  frame periods, enough to ride out shared-bus I2C jitter.             */
#define EM7028_SCOPE_FRAME_TIMEOUT_MS            200U

/** Ring depth -- ~6.4 s history at 40 Hz. Power-of-two so the modulo
 *  reduces to a mask on toolchains that bother to optimise it.          */
#define EM7028_SCOPE_RING_DEPTH                  256U

/** RTT heartbeat cadence -- one log line per second at 40 Hz.           */
#define EM7028_SCOPE_LOG_EVERY_N_FRAMES           40U

/** Idle delay if reconfigure / start fails -- task parks itself and
 *  keeps its stack alive for postmortem inspection.                     */
#define EM7028_SCOPE_PARK_DELAY_MS              5000U

/** I2C transfer constants for the post-start register dump. */
#define EM7028_SCOPE_I2C_TIMEOUT_MS              100U
#define EM7028_SCOPE_I2C_MEM_SIZE_8BIT             1U
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/**
 * Ring of full PPG frames. External linkage so Ozone's Symbol Browser
 * lists it without needing a TU-specific scope; volatile fields below
 * the ring keep the compiler from coalescing stores across the publish
 * path.
 */
wp_ppg_frame_t    g_em7028_scope_ring[EM7028_SCOPE_RING_DEPTH];

/**
 * Ring write index. Always < EM7028_SCOPE_RING_DEPTH after publish.
 * Bumped AFTER the slot is fully populated -- an external observer
 * (debugger / DMA snoop) reading idx then ring[idx-1] always sees a
 * complete frame.
 */
volatile uint32_t g_em7028_scope_w_idx;

/** Sticky frame counter -- never wraps within a session, suitable as
 *  the x-axis when plotting from J-Scope.                               */
volatile uint32_t g_em7028_scope_frame_cnt;

/** Non-OK heart_rate_drv_get_req counter -- non-zero growth signals
 *  handler-side starvation or I2C errors.                               */
volatile uint32_t g_em7028_scope_drop_cnt;

/* ---- Latest-sample scalar mirrors (subscribe these in J-Scope) ----- */
volatile uint32_t g_em7028_scope_ts_ms;
volatile uint16_t g_em7028_scope_hrs1_p[WP_HEART_RATE_PIXEL_NUM];
volatile uint16_t g_em7028_scope_hrs2_p[WP_HEART_RATE_PIXEL_NUM];
volatile uint32_t g_em7028_scope_hrs1_sum;
volatile uint32_t g_em7028_scope_hrs2_sum;

/**
 * One-shot register readback captured AFTER start. Useful when one
 * channel reads zero -- compares the chip-side state against the
 * driver's cached_cfg to localise whether the write made it through.
 */
volatile uint8_t  g_em7028_scope_reg_id;
volatile uint8_t  g_em7028_scope_reg_hrs_cfg;
volatile uint8_t  g_em7028_scope_reg_hrs1_ctrl;
volatile uint8_t  g_em7028_scope_reg_hrs2_ctrl;
volatile uint8_t  g_em7028_scope_reg_hrs2_gain;
volatile uint8_t  g_em7028_scope_reg_led_crt;
volatile uint8_t  g_em7028_scope_reg_dump_ok;

/**
 * Capture cfg -- 40 Hz, 14-bit ADC, HRS1 only.
 *
 * HRS1 gain x1 + range x8 at LED current 0x80 lands the 14-bit ADC
 * mid-scale (~6000-12000 in vivo), leaving room for the PPG AC swing.
 * Bump gain one step at a time if amplitude is too small AND the sum
 * stays well below ~14000.
 */
wp_heart_rate_config_t g_em7028_scope_cfg =
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
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief      Republish one frame to all J-Scope-visible globals.
 *             Single-producer (this task) / multi-observer (debugger);
 *             no lock is required as long as the ordering between the
 *             ring slot store and the write-index bump is preserved.
 *
 * @param[in]  p_frame : Frame to publish (non-NULL, fully populated).
 *
 * @return     None.
 * */
static void em7028_scope_publish(wp_ppg_frame_t const *const p_frame)
{
    /**
     * Copy-by-value into the ring slot first, THEN bump the write
     * index -- this gives an inspector that reads w_idx then the
     * preceding slot a coherent snapshot regardless of when the
     * halt happens.
     **/
    uint32_t idx = g_em7028_scope_w_idx;
    if (idx >= EM7028_SCOPE_RING_DEPTH)
    {
        idx = 0U;
    }
    g_em7028_scope_ring[idx] = *p_frame;

    /**
     * Mirror the latest sample into scalar globals. J-Scope plots
     * scalars natively, so subscribing g_em7028_scope_hrs1_sum (and
     * peers) yields a live waveform without any host-side scripting.
     **/
    g_em7028_scope_ts_ms = p_frame->timestamp_ms;
    for (uint32_t i = 0U; i < (uint32_t)WP_HEART_RATE_PIXEL_NUM; i++)
    {
        g_em7028_scope_hrs1_p[i] = p_frame->hrs1_pixel[i];
        g_em7028_scope_hrs2_p[i] = p_frame->hrs2_pixel[i];
    }
    g_em7028_scope_hrs1_sum = p_frame->hrs1_sum;
    g_em7028_scope_hrs2_sum = p_frame->hrs2_sum;

    /* Advance index AFTER content is in place (publish ordering). */
    g_em7028_scope_w_idx     = (idx + 1U) % EM7028_SCOPE_RING_DEPTH;
    g_em7028_scope_frame_cnt++;
}

/**
 * @brief      Read one EM7028 register over the shared I2C bus through
 *             the integration-layer trampoline. Bypasses the wrapper
 *             vtable on purpose -- the wrapper does not expose raw
 *             register access. The bus mutex inside the I2C port layer
 *             serialises against the handler thread's frame reads.
 *
 * @param[in]  reg   : Target register address.
 * @param[out] p_val : Output byte (untouched on error).
 *
 * @return     EM7028_OK on success, otherwise the I2C error code.
 * */
static em7028_status_t em7028_scope_read_reg(uint8_t reg, uint8_t *p_val)
{
    em7028_iic_driver_interface_t *p_iic = em7028_input_arg.p_iic_interface;

    return p_iic->pf_iic_mem_read(p_iic->hi2c,
                                  (uint16_t)((EM7028_ADDR << 1) | 1U),
                                  (uint16_t)reg,
                                  EM7028_SCOPE_I2C_MEM_SIZE_8BIT,
                                  p_val,
                                  1U,
                                  EM7028_SCOPE_I2C_TIMEOUT_MS);
}

/**
 * @brief      One-shot register dump captured AFTER start. Publishes
 *             the readback to the g_em7028_scope_reg_* globals and
 *             prints a summary line to RTT.
 *
 *             The dump deliberately races the handler -- the I2C bus
 *             mutex serialises every transaction, and the handler
 *             only touches the chip during the 25 ms sample tick, so
 *             a sub-1 ms dump completes well within one slot.
 *
 * @return     None.
 * */
static void em7028_scope_dump_regs(void)
{
    uint8_t v = 0U;
    bool    ok = true;

    if (EM7028_OK != em7028_scope_read_reg(ID_REG, &v))         { ok = false; }
    g_em7028_scope_reg_id        = v;
    if (EM7028_OK != em7028_scope_read_reg(HRS_CFG, &v))        { ok = false; }
    g_em7028_scope_reg_hrs_cfg   = v;
    if (EM7028_OK != em7028_scope_read_reg(HRS1_CTRL, &v))      { ok = false; }
    g_em7028_scope_reg_hrs1_ctrl = v;
    if (EM7028_OK != em7028_scope_read_reg(HRS2_CTRL, &v))      { ok = false; }
    g_em7028_scope_reg_hrs2_ctrl = v;
    if (EM7028_OK != em7028_scope_read_reg(HRS2_GAIN_CTRL, &v)) { ok = false; }
    g_em7028_scope_reg_hrs2_gain = v;
    if (EM7028_OK != em7028_scope_read_reg(LED_CRT, &v))        { ok = false; }
    g_em7028_scope_reg_led_crt   = v;

    g_em7028_scope_reg_dump_ok = ok ? 1U : 0U;

    /**
     * One log line carries everything a debugger needs to decide if a
     * silent channel is a write failure or a chip-side issue.
     **/
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "scope regs ok=%u id=0x%02X cfg=0x%02X "
              "h1ctrl=0x%02X h2ctrl=0x%02X h2gain=0x%02X led=0x%02X",
              (unsigned)g_em7028_scope_reg_dump_ok,
              (unsigned)g_em7028_scope_reg_id,
              (unsigned)g_em7028_scope_reg_hrs_cfg,
              (unsigned)g_em7028_scope_reg_hrs1_ctrl,
              (unsigned)g_em7028_scope_reg_hrs2_ctrl,
              (unsigned)g_em7028_scope_reg_hrs2_gain,
              (unsigned)g_em7028_scope_reg_led_crt);
}

/**
 * @brief      Park the task forever after a fatal setup error.
 *             Stack is kept allocated for postmortem inspection.
 *
 * @return     Does not return.
 * */
static void em7028_scope_park(void)
{
    /**
     * One-shot setup failed -- there is no sensible retry path here:
     * the handler singleton is owned by another thread and will keep
     * rejecting our wrapper calls. Idle indefinitely.
     **/
    for (;;)
    {
        osal_task_delay(EM7028_SCOPE_PARK_DELAY_MS);
    }
}

/**
 * @brief      Apply the capture cfg through the heart_rate wrapper and
 *             request the underlying driver to begin sampling.
 *
 * @return     WP_HEART_RATE_OK on success, otherwise the failing status.
 * */
static wp_heart_rate_status_t em7028_scope_arm(void)
{
    /* heart_rate_drv_init is idempotent and only zeroes the projection
     * buffer in the EM7028 adapter; safe to call here even if the
     * pre-kernel platform_io_register hook also touched it.            */
    heart_rate_drv_init();

    /**
     * RECONFIGURE applies the cfg even when the handler is idle (it
     * just caches and returns), so START still has to be issued
     * explicitly after.
     **/
    wp_heart_rate_status_t ret = heart_rate_drv_reconfigure(&g_em7028_scope_cfg);
    if (WP_HEART_RATE_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "scope reconfigure failed = %d", (int)ret);
        return ret;
    }

    ret = heart_rate_drv_start();
    if (WP_HEART_RATE_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "scope start failed = %d", (int)ret);
        return ret;
    }

    DEBUG_OUT(d, EM7028_LOG_TAG,
              "scope armed: HRS1 @ 40 Hz pulse, ring=%u",
              (unsigned)EM7028_SCOPE_RING_DEPTH);
    return WP_HEART_RATE_OK;
}

/**
 * @brief      EM7028 J-Scope capture task entry.
 *
 *             Boots the handler-side stream through the heart_rate
 *             wrapper, then forever republishes every frame to the
 *             J-Scope-visible globals so a host debugger (Ozone /
 *             J-Scope) can plot raw PPG waveforms without any extra
 *             wire protocol.
 *
 * @param[in]  argument : Unused (handler thread owns the input args).
 *
 * @return     None (task entry).
 * */
void em7028_jscope_capture_task(void *argument)
{
    (void)argument;

    DEBUG_OUT(i, EM7028_LOG_TAG, "em7028_jscope_capture_task start");

    /* Wait for the handler thread to mount the singleton. */
    osal_task_delay(EM7028_SCOPE_BOOT_WAIT_MS);

    if (WP_HEART_RATE_OK != em7028_scope_arm())
    {
        em7028_scope_park();
    }

    /**
     * Snapshot the chip-side register state right after start so the
     * user can confirm cached_cfg made it into hardware. The I2C bus
     * mutex serialises this against the handler thread's frame reads.
     **/
    em7028_scope_dump_regs();

    /**
     * Streaming loop. The handler-side frame queue is depth=10 with
     * drop-on-full, so this consumer is the rate-limiting side -- it
     * must stay drained or the handler will start dropping frames at
     * the producer end. With a 25 ms cadence and 200 ms timeout the
     * loop has ~8 frame periods of headroom before timing out and
     * incrementing the drop counter.
     **/
    for (;;)
    {
        wp_heart_rate_status_t ret =
            heart_rate_drv_get_req(EM7028_SCOPE_FRAME_TIMEOUT_MS);
        if (WP_HEART_RATE_OK != ret)
        {
            /**
             * Frame queue starvation -- count and continue. Do NOT
             * spin without yielding: get_req already blocks for
             * EM7028_SCOPE_FRAME_TIMEOUT_MS, so the OS scheduler has
             * already had its turn.
             **/
            g_em7028_scope_drop_cnt++;
            continue;
        }

        /* Frame is now in the adapter's projection buffer; pointer is
         * valid until the next get_req call mutates it.               */
        wp_ppg_frame_t *p_frame = heart_rate_get_frame_addr();
        if (NULL == p_frame)
        {
            g_em7028_scope_drop_cnt++;
            continue;
        }

        em7028_scope_publish(p_frame);

        /* Symmetric release -- no-op for this adapter, but keeps the
         * call site honest for future ring-buffer backends.           */
        heart_rate_read_data_done();

        /* RTT heartbeat -- one line per second at 40 Hz. */
        if (0U == (g_em7028_scope_frame_cnt %
                   (uint32_t)EM7028_SCOPE_LOG_EVERY_N_FRAMES))
        {
            DEBUG_OUT(i, EM7028_LOG_TAG,
                      "scope #%u ts=%u hrs1=%u hrs2=%u drops=%u",
                      (unsigned)g_em7028_scope_frame_cnt,
                      (unsigned)p_frame->timestamp_ms,
                      (unsigned)p_frame->hrs1_sum,
                      (unsigned)p_frame->hrs2_sum,
                      (unsigned)g_em7028_scope_drop_cnt);
        }
    }
}
//******************************* Functions *********************************//
