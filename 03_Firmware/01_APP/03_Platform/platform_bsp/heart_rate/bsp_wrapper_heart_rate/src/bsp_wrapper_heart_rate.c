/******************************************************************************
 * @file bsp_wrapper_heart_rate.c
 *
 * @par dependencies
 * - bsp_wrapper_heart_rate.h
 *
 * @author Ethan-Hang
 *
 * @brief Wrapper-vtable dispatch for PPG heart-rate sensors.
 *
 *        The wrapper holds a small static vtable table that the adapter
 *        layer fills via drv_adapter_heart_rate_mount(); every public
 *        API is a thin forwarder that resolves the active slot and
 *        invokes the corresponding function pointer.
 *
 * @version V1.0 2026-05-07
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_wrapper_heart_rate.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define HEART_RATE_DRV_MAX_NUM      (2U)
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static heart_rate_drv_t s_heart_rate_drv[HEART_RATE_DRV_MAX_NUM];
static UINT8_T          s_cur_heart_rate_drv_idx = 0U;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/**
 * @brief    Copy the caller-prepared vtable into the slot table and mark
 *           the slot as the currently active driver.
 *
 * @param[in] idx : Slot index (0 .. HEART_RATE_DRV_MAX_NUM-1).
 * @param[in] drv : Vtable-filled driver struct (non-NULL).
 *
 * @return   true on success; false on invalid idx or NULL drv.
 * */
BOOL_T drv_adapter_heart_rate_mount(UINT8_T idx, heart_rate_drv_t *const drv)
{
    /* Out-of-range idx or NULL drv would corrupt the table; reject. */
    if (idx >= HEART_RATE_DRV_MAX_NUM || NULL == drv)
    {
        return false;
    }

    /**
    * heart_rate_drv_t is a flat vtable, so a whole-struct copy captures
    * every slot — replaces the per-field copy. idx is pinned to the slot.
    **/
    s_heart_rate_drv[idx]     = *drv;
    s_heart_rate_drv[idx].idx = idx;

    /* Last successful mount becomes the active driver -- single-driver
     * deployments do not need to call any extra "select" API.        */
    s_cur_heart_rate_drv_idx = idx;

    return true;
}

/**
 * @brief    Forward init request to the active driver.
 *
 * @return   None.
 * */
void heart_rate_drv_init(void)
{
    /* Resolve the active vtable up front; calling through a NULL slot is
     * undefined behaviour and would be hard to debug at the call site. */
    heart_rate_drv_t *drv = &s_heart_rate_drv[s_cur_heart_rate_drv_idx];
    if (NULL != drv->pf_heart_rate_drv_init)
    {
        drv->pf_heart_rate_drv_init(drv);
    }
}

/**
 * @brief    Forward deinit request to the active driver.
 *
 * @return   None.
 * */
void heart_rate_drv_deinit(void)
{
    heart_rate_drv_t *drv = &s_heart_rate_drv[s_cur_heart_rate_drv_idx];
    if (NULL != drv->pf_heart_rate_drv_deinit)
    {
        drv->pf_heart_rate_drv_deinit(drv);
    }
}

/**
 * @brief    Forward "begin sampling" to the active driver.
 *
 * @return   PLATFORM_ERR_NO_RESOURCE if no driver is mounted, else
 *           the chip-specific status returned by the adapter.
 * */
platform_err_t heart_rate_drv_start(void)
{
    heart_rate_drv_t *drv = &s_heart_rate_drv[s_cur_heart_rate_drv_idx];
    if (NULL != drv->pf_heart_rate_drv_start)
    {
        return drv->pf_heart_rate_drv_start(drv);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

/**
 * @brief    Forward "halt sampling" to the active driver.
 *
 * @return   PLATFORM_ERR_NO_RESOURCE if no driver is mounted, else
 *           the chip-specific status.
 * */
platform_err_t heart_rate_drv_stop(void)
{
    heart_rate_drv_t *drv = &s_heart_rate_drv[s_cur_heart_rate_drv_idx];
    if (NULL != drv->pf_heart_rate_drv_stop)
    {
        return drv->pf_heart_rate_drv_stop(drv);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

/**
 * @brief    Forward "apply new configuration" to the active driver.
 *
 * @param[in] p_cfg : Configuration to apply; NULL is rejected here so the
 *                    adapter can assume a non-NULL pointer.
 *
 * @return   PLATFORM_ERR_PARAM on NULL p_cfg;
 *           PLATFORM_ERR_NO_RESOURCE  on missing slot;
 *           otherwise the adapter-level status.
 * */
platform_err_t heart_rate_drv_reconfigure(
                                  wp_heart_rate_config_t const *const p_cfg)
{
    /* Cheaper to reject up front than to push a NULL down the vtable;
     * the adapter would have to repeat this guard anyway.            */
    if (NULL == p_cfg)
    {
        return PLATFORM_ERR_PARAM;
    }

    heart_rate_drv_t *drv = &s_heart_rate_drv[s_cur_heart_rate_drv_idx];
    if (NULL != drv->pf_heart_rate_drv_reconfigure)
    {
        return drv->pf_heart_rate_drv_reconfigure(drv, p_cfg);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

/**
 * @brief    Block until the next frame is available on the active driver.
 *
 * @param[in] timeout_ms : Maximum wait in milliseconds.
 *
 * @return   PLATFORM_OK / PLATFORM_ERR_TIMEOUT / RESOURCE.
 * */
platform_err_t heart_rate_drv_get_req(UINT32_T timeout_ms)
{
    heart_rate_drv_t *drv = &s_heart_rate_drv[s_cur_heart_rate_drv_idx];
    if (NULL != drv->pf_heart_rate_drv_get_req)
    {
        return drv->pf_heart_rate_drv_get_req(drv, timeout_ms);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

/**
 * @brief    Pointer accessor for the most recently delivered frame.
 *
 * @return   Pointer into the adapter-owned frame buffer, or NULL if no
 *           driver is mounted.
 * */
wp_ppg_frame_t *heart_rate_get_frame_addr(void)
{
    heart_rate_drv_t *drv = &s_heart_rate_drv[s_cur_heart_rate_drv_idx];
    if (NULL != drv->pf_heart_rate_get_frame_addr)
    {
        return drv->pf_heart_rate_get_frame_addr(drv);
    }
    return NULL;
}

/**
 * @brief    Forward "frame consumed" to the active driver. No-op for
 *           queue-pop backends; reserved for future ring-buffer adapters
 *           that need an explicit release.
 *
 * @return   None.
 * */
void heart_rate_read_data_done(void)
{
    heart_rate_drv_t *drv = &s_heart_rate_drv[s_cur_heart_rate_drv_idx];
    if (NULL != drv->pf_heart_rate_read_data_done)
    {
        drv->pf_heart_rate_read_data_done(drv);
    }
}

//******************************* Functions *********************************//
