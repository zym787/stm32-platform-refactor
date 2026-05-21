/******************************************************************************
 * @file bsp_wrapper_externflash.c
 *
 * @par dependencies
 * - bsp_wrapper_externflash.h
 *
 * @author Ethan-Hang
 *
 * @brief Abstract wrapper implementation for external flash access.
 *        Maintains a static array of registered driver vtables and
 *        dispatches all public API calls to the currently active slot.
 *
 * Processing flow:
 *   1. drv_adapter_externflash_mount() copies the adapter vtable into
 *      s_externflash_drv[].
 *   2. All public API functions resolve the active driver via
 *      s_cur_externflash_drv_idx and forward through the vtable.
 *
 * @version V1.0 2026-05-02
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_wrapper_externflash.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define EXTERNFLASH_DRV_MAX_NUM 1
//******************************** Defines **********************************//

//******************************* Variables *********************************//
static externflash_drv_t s_externflash_drv[EXTERNFLASH_DRV_MAX_NUM] = {0};
static uint32_t          s_cur_externflash_drv_idx                  =  0;
//******************************* Variables *********************************//

//******************************* Functions *********************************//

bool drv_adapter_externflash_mount(uint8_t                  idx,
                                   externflash_drv_t *const drv)
{
    if (idx >= EXTERNFLASH_DRV_MAX_NUM || drv == NULL)
    {
        return false;
    }

    s_externflash_drv[idx].idx = \
                          idx;
    s_externflash_drv[idx].dev_id = \
                          drv->dev_id;
    s_externflash_drv[idx].user_data = \
                          drv->user_data;

    s_externflash_drv[idx].pf_externflash_drv_init = \
                          drv->pf_externflash_drv_init;
    s_externflash_drv[idx].pf_externflash_drv_deinit = \
                          drv->pf_externflash_drv_deinit;
    s_externflash_drv[idx].pf_externflash_read = \
                          drv->pf_externflash_read;
    s_externflash_drv[idx].pf_externflash_write = \
                          drv->pf_externflash_write;
    s_externflash_drv[idx].pf_externflash_write_noerase = \
                          drv->pf_externflash_write_noerase;
    s_externflash_drv[idx].pf_externflash_erase_chip = \
                          drv->pf_externflash_erase_chip;
    s_externflash_drv[idx].pf_externflash_erase_sector = \
                          drv->pf_externflash_erase_sector;
    s_externflash_drv[idx].pf_externflash_sleep = \
                          drv->pf_externflash_sleep;
    s_externflash_drv[idx].pf_externflash_wakeup = \
                          drv->pf_externflash_wakeup;

    s_cur_externflash_drv_idx = idx;
    return true;
}

void externflash_drv_init(void)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_drv_init)
    {
        p_drv->pf_externflash_drv_init(p_drv);
    }
}

void externflash_drv_deinit(void)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_drv_deinit)
    {
        p_drv->pf_externflash_drv_deinit(p_drv);
    }
}

wp_externflash_status_t externflash_drv_read(uint32_t                  addr,
                                             uint8_t  *                data,
                                             uint32_t                  size,
                                             wp_externflash_callback_t   cb,
                                             void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_read)
    {
        return p_drv->pf_externflash_read(p_drv, addr, data, size,
                                          cb, p_user_ctx);
    }
    return WP_EXTERNFLASH_ERRORRESOURCE;
}

wp_externflash_status_t externflash_drv_write(uint32_t                  addr,
                                              uint8_t  *                data,
                                              uint32_t                  size,
                                              wp_externflash_callback_t   cb,
                                              void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_write)
    {
        return p_drv->pf_externflash_write(p_drv, addr, data, size,
                                           cb, p_user_ctx);
    }
    return WP_EXTERNFLASH_ERRORRESOURCE;
}

wp_externflash_status_t externflash_drv_write_noerase(uint32_t                  addr,
                                                      uint8_t  *                data,
                                                      uint32_t                  size,
                                                      wp_externflash_callback_t   cb,
                                                      void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_write_noerase)
    {
        return p_drv->pf_externflash_write_noerase(p_drv, addr, data, size,
                                                   cb, p_user_ctx);
    }
    return WP_EXTERNFLASH_ERRORRESOURCE;
}

wp_externflash_status_t externflash_drv_erase_chip(wp_externflash_callback_t   cb,
                                                   void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_erase_chip)
    {
        return p_drv->pf_externflash_erase_chip(p_drv, cb, p_user_ctx);
    }
    return WP_EXTERNFLASH_ERRORRESOURCE;
}

wp_externflash_status_t externflash_drv_erase_sector(uint32_t                  addr,
                                                     wp_externflash_callback_t   cb,
                                                     void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_erase_sector)
    {
        return p_drv->pf_externflash_erase_sector(p_drv, addr, cb, p_user_ctx);
    }
    return WP_EXTERNFLASH_ERRORRESOURCE;
}

wp_externflash_status_t externflash_drv_sleep(wp_externflash_callback_t   cb,
                                              void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_sleep)
    {
        return p_drv->pf_externflash_sleep(p_drv, cb, p_user_ctx);
    }
    return WP_EXTERNFLASH_ERRORRESOURCE;
}

wp_externflash_status_t externflash_drv_wakeup(wp_externflash_callback_t   cb,
                                               void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_wakeup)
    {
        return p_drv->pf_externflash_wakeup(p_drv, cb, p_user_ctx);
    }
    return WP_EXTERNFLASH_ERRORRESOURCE;
}

//******************************* Functions *********************************//
