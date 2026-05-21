/******************************************************************************
 * @file bsp_adapter_port_heart_rate.c
 *
 * @par dependencies
 * - bsp_adapter_port_heart_rate.h
 * - bsp_wrapper_heart_rate.h
 * - bsp_em7028_handler.h
 * - bsp_em7028_driver.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief Adapter for routing bsp_wrapper_heart_rate calls into the EM7028
 *        handler API.
 *
 * == Lifecycle ==
 *   start / stop / reconfigure forward 1:1 to bsp_em7028_handler_*; the
 *   handler thread owns the chip-side state machine (cfg cache + 40 Hz
 *   pacing loop), the adapter is purely a translation point.
 *
 * == Streaming ==
 *   pf_heart_rate_drv_get_req calls bsp_em7028_handler_get_frame() into a
 *   private em7028_ppg_frame_t scratch, then projects the chip frame
 *   onto a wrapper-shaped wp_ppg_frame_t held in s_latest_frame.
 *   pf_heart_rate_get_frame_addr returns &s_latest_frame so the consumer
 *   can read fields without copying.
 *   pf_heart_rate_read_data_done is a no-op -- the underlying queue is
 *   already drained by the get_frame call.
 *
 * == Concurrency ==
 *   The em7028 handler frame queue is single-consumer by contract.
 *   s_latest_frame therefore needs no lock as long as only ONE consumer
 *   task drives the wrapper streaming API. Multiple consumers would
 *   require a wrapper-level mutex; out of scope for v1.
 *
 * == Configuration translation ==
 *   wp_heart_rate_config_t and em7028_config_t share enum ordinals by
 *   construction (see bsp_wrapper_heart_rate.h commentary), so the
 *   reconfigure path is a field-by-field cast/assignment.
 *
 * @version V1.0 2026-05-07
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <string.h>

#include "bsp_adapter_port_heart_rate.h"
#include "bsp_wrapper_heart_rate.h"
#include "bsp_em7028_handler.h"
#include "bsp_em7028_driver.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static void em7028_drv_init  (heart_rate_drv_t *const dev);
static void em7028_drv_deinit(heart_rate_drv_t *const dev);

static wp_heart_rate_status_t em7028_drv_start(heart_rate_drv_t *const dev);
static wp_heart_rate_status_t em7028_drv_stop (heart_rate_drv_t *const dev);
static wp_heart_rate_status_t em7028_drv_reconfigure(
                                heart_rate_drv_t *const                dev,
                                wp_heart_rate_config_t const *const  p_cfg);

static wp_heart_rate_status_t em7028_drv_get_req(
                                heart_rate_drv_t *const dev,
                                uint32_t         timeout_ms);
static wp_ppg_frame_t        *em7028_get_frame_addr(
                                heart_rate_drv_t *const dev);
static void                   em7028_read_data_done(
                                heart_rate_drv_t *const dev);

static wp_heart_rate_status_t em7028_status_to_wp(
                                em7028_handler_status_t status);
static void                   em7028_cfg_translate(
                                wp_heart_rate_config_t const *const p_src,
                                em7028_config_t              *const p_dst);
//******************************* Declaring *********************************//

//******************************** Variables ********************************//
/* Wrapper-shape projection of the most recent chip frame. Single-writer
 * (the consumer task that calls heart_rate_drv_get_req) / multi-reader
 * (the same task plus optionally a debugger), so no lock is required.   */
static wp_ppg_frame_t s_latest_frame;
//******************************** Variables ********************************//

//******************************* Functions *********************************//

/**
 * @brief    Map an em7028_handler_status_t into the wrapper-level
 *           wp_heart_rate_status_t. Both enums share the same first
 *           eight ordinals by construction, so the translation is a
 *           plain cast in the common case; the explicit switch keeps a
 *           sane default in case a chip-specific code is added later.
 *
 * @param[in] status : Status returned by the EM7028 handler API.
 *
 * @return   The matching wp_heart_rate_status_t.
 * */
static wp_heart_rate_status_t em7028_status_to_wp(em7028_handler_status_t status)
{
    switch (status)
    {
    case EM7028_HANDLER_OK:
        return WP_HEART_RATE_OK;
    case EM7028_HANDLER_ERROR:
        return WP_HEART_RATE_ERROR;
    case EM7028_HANDLER_ERRORTIMEOUT:
        return WP_HEART_RATE_ERRORTIMEOUT;
    case EM7028_HANDLER_ERRORRESOURCE:
        return WP_HEART_RATE_ERRORRESOURCE;
    case EM7028_HANDLER_ERRORPARAMETER:
        return WP_HEART_RATE_ERRORPARAMETER;
    case EM7028_HANDLER_ERRORNOMEMORY:
        return WP_HEART_RATE_ERRORNOMEMORY;
    case EM7028_HANDLER_ERRORUNSUPPORTED:
        return WP_HEART_RATE_ERRORUNSUPPORTED;
    case EM7028_HANDLER_ERRORISR:
        return WP_HEART_RATE_ERRORISR;
    case EM7028_HANDLER_ERRORNOTINIT:
        return WP_HEART_RATE_ERRORNOTINIT;
    default:
        return WP_HEART_RATE_ERROR;
    }
}

/**
 * @brief    Translate a wrapper-level config into the chip-level config.
 *           Wrapper enums match em7028 ordinals one-for-one, so the
 *           translation is a direct field assignment with explicit casts
 *           that document the type-bridge.
 *
 * @param[in]  p_src : Wrapper-level config supplied by the caller.
 * @param[out] p_dst : Chip-level config consumed by the EM7028 handler.
 *
 * @return    None.
 * */
static void em7028_cfg_translate(wp_heart_rate_config_t const *const p_src,
                                 em7028_config_t              *const p_dst)
{
    p_dst->enable_hrs1     = p_src->enable_hrs1;
    p_dst->enable_hrs2     = p_src->enable_hrs2;
    p_dst->hrs1_gain_high  = p_src->hrs1_gain_high;
    p_dst->hrs1_range_high = p_src->hrs1_range_high;
    p_dst->hrs1_freq       = (em7028_hrs1_freq_t)p_src->hrs1_freq;
    p_dst->hrs1_res        = (em7028_hrs_res_t  )p_src->hrs1_res;
    p_dst->hrs2_mode       = (em7028_hrs2_mode_t)p_src->hrs2_mode;
    p_dst->hrs2_wait       = (em7028_hrs2_wait_t)p_src->hrs2_wait;
    p_dst->hrs2_gain_high  = p_src->hrs2_gain_high;
    p_dst->hrs2_pos_mask   = p_src->hrs2_pos_mask;
    p_dst->led_current     = p_src->led_current;
}

/**
 * @brief    Driver init slot. The EM7028 handler thread owns its own
 *           init flow (auto-init from bsp_em7028_handler_inst), so this
 *           hook is intentionally empty -- it exists for vtable symmetry
 *           and to reserve space for any future adapter-local resources.
 *
 * @param[in] dev : Vtable slot (unused).
 *
 * @return   None.
 * */
static void em7028_drv_init(heart_rate_drv_t *const dev)
{
    (void)dev;
    /* Clear the projection buffer so a stale-read before the first
     * frame arrives returns zero rather than garbage from .bss aliasing
     * (the static qualifier already zeroes it; this is belt and braces). */
    memset(&s_latest_frame, 0, sizeof(s_latest_frame));
}

/**
 * @brief    Driver deinit slot. No adapter-local resources to release.
 *
 * @param[in] dev : Vtable slot (unused).
 *
 * @return   None.
 * */
static void em7028_drv_deinit(heart_rate_drv_t *const dev)
{
    (void)dev;
}

/**
 * @brief    Forward "begin sampling" to the EM7028 handler.
 *
 * @param[in] dev : Vtable slot (unused).
 *
 * @return   wp_heart_rate_status_t mirroring the handler outcome.
 * */
static wp_heart_rate_status_t em7028_drv_start(heart_rate_drv_t *const dev)
{
    (void)dev;
    em7028_handler_status_t ret = bsp_em7028_handler_start();
    if (EM7028_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "heart_rate adapter: start failed (%d)", (int)ret);
    }
    return em7028_status_to_wp(ret);
}

/**
 * @brief    Forward "halt sampling" to the EM7028 handler.
 *
 * @param[in] dev : Vtable slot (unused).
 *
 * @return   wp_heart_rate_status_t mirroring the handler outcome.
 * */
static wp_heart_rate_status_t em7028_drv_stop(heart_rate_drv_t *const dev)
{
    (void)dev;
    em7028_handler_status_t ret = bsp_em7028_handler_stop();
    if (EM7028_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "heart_rate adapter: stop failed (%d)", (int)ret);
    }
    return em7028_status_to_wp(ret);
}

/**
 * @brief    Translate the wrapper config and submit it to the EM7028
 *           handler. The handler deep-copies the cfg into the queued
 *           event, so the local em7028_cfg may live on this stack.
 *
 * @param[in] dev   : Vtable slot (unused).
 * @param[in] p_cfg : Wrapper-level cfg (already null-checked by wrapper).
 *
 * @return   wp_heart_rate_status_t mirroring the handler outcome.
 * */
static wp_heart_rate_status_t em7028_drv_reconfigure(
                                heart_rate_drv_t *const                dev,
                                wp_heart_rate_config_t const *const  p_cfg)
{
    (void)dev;

    /* Stack-local translation buffer. Cheap (~12 bytes) and avoids any
     * adapter-wide singleton config that would need locking under
     * concurrent reconfigure callers. */
    em7028_config_t em7028_cfg;
    em7028_cfg_translate(p_cfg, &em7028_cfg);

    em7028_handler_status_t ret = bsp_em7028_handler_reconfigure(&em7028_cfg);
    if (EM7028_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "heart_rate adapter: reconfigure failed (%d)", (int)ret);
    }
    return em7028_status_to_wp(ret);
}

/**
 * @brief    Block until the next chip frame is delivered by the handler,
 *           then project it onto the wrapper-shape s_latest_frame slot.
 *
 *           A successful return guarantees s_latest_frame holds the new
 *           data; on timeout / error the slot is left untouched so a
 *           late observer keeps seeing the previous good frame.
 *
 * @param[in] dev        : Vtable slot (unused).
 * @param[in] timeout_ms : Maximum wait in milliseconds.
 *
 * @return   wp_heart_rate_status_t mirroring the handler outcome.
 * */
static wp_heart_rate_status_t em7028_drv_get_req(
                                  heart_rate_drv_t *const dev,
                                  uint32_t         timeout_ms)
{
    (void)dev;

    em7028_ppg_frame_t chip_frame;
    /* Zero before the call so that even if the handler returns OK with
     * partially populated fields, no stack garbage leaks into the
     * projection. */
    memset(&chip_frame, 0, sizeof(chip_frame));

    em7028_handler_status_t ret =
        bsp_em7028_handler_get_frame(&chip_frame, timeout_ms);
    if (EM7028_HANDLER_OK != ret)
    {
        return em7028_status_to_wp(ret);
    }

    /* Project chip frame -> wrapper frame. Pixel arrays share width
     * (EM7028_HRS_PIXEL_NUM == WP_HEART_RATE_PIXEL_NUM == 4); a static
     * assert in v2 will tighten this once a second chip is added. */
    s_latest_frame.timestamp_ms = chip_frame.timestamp_ms;
    for (uint32_t i = 0U; i < (uint32_t)WP_HEART_RATE_PIXEL_NUM; i++)
    {
        s_latest_frame.hrs1_pixel[i] = chip_frame.hrs1_pixel[i];
        s_latest_frame.hrs2_pixel[i] = chip_frame.hrs2_pixel[i];
    }
    s_latest_frame.hrs1_sum = chip_frame.hrs1_sum;
    s_latest_frame.hrs2_sum = chip_frame.hrs2_sum;

    return WP_HEART_RATE_OK;
}

/**
 * @brief    Return the projection-buffer address. The caller must have
 *           seen WP_HEART_RATE_OK from heart_rate_drv_get_req() before
 *           dereferencing; otherwise the buffer holds the previous
 *           successful frame (or zero if no frame has arrived yet).
 *
 * @param[in] dev : Vtable slot (unused).
 *
 * @return   &s_latest_frame.
 * */
static wp_ppg_frame_t *em7028_get_frame_addr(heart_rate_drv_t *const dev)
{
    (void)dev;
    return &s_latest_frame;
}

/**
 * @brief    Frame-consumption acknowledge. The EM7028 handler queue is
 *           pop-and-copy, so there is nothing to release; this hook
 *           exists for vtable symmetry only.
 *
 * @param[in] dev : Vtable slot (unused).
 *
 * @return   None.
 * */
static void em7028_read_data_done(heart_rate_drv_t *const dev)
{
    (void)dev;
}

/**
 * @brief    Mount the EM7028 implementation into the wrapper vtable.
 *           Pure function-pointer assignment -- safe to call before
 *           the kernel starts.
 *
 * @return   true on successful mount.
 * */
bool drv_adapter_heart_rate_register(void)
{
    heart_rate_drv_t heart_rate_drv = {
        .idx                              = 0,
        .dev_id                           = 0,
        .user_data                        = NULL,
        .pf_heart_rate_drv_init           = em7028_drv_init,
        .pf_heart_rate_drv_deinit         = em7028_drv_deinit,
        .pf_heart_rate_drv_start          = em7028_drv_start,
        .pf_heart_rate_drv_stop           = em7028_drv_stop,
        .pf_heart_rate_drv_reconfigure    = em7028_drv_reconfigure,
        .pf_heart_rate_drv_get_req        = em7028_drv_get_req,
        .pf_heart_rate_get_frame_addr     = em7028_get_frame_addr,
        .pf_heart_rate_read_data_done     = em7028_read_data_done,
    };

    return drv_adapter_heart_rate_mount(0, &heart_rate_drv);
}

//******************************* Functions *********************************//
