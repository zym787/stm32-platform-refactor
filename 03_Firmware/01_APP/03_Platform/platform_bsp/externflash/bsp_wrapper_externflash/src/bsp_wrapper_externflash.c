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
static UINT32_T          s_cur_externflash_drv_idx                  =  0;
//******************************* Variables *********************************//

//******************************* Functions *********************************//

BOOL_T drv_adapter_externflash_mount(UINT8_T                  idx,
                                   externflash_drv_t *const drv)
{
    if (idx >= EXTERNFLASH_DRV_MAX_NUM || drv == NULL)
    {
        return false;
    }

    /**
    * externflash_drv_t is a flat vtable, so a whole-struct copy captures
    * every slot — replaces the per-field copy. idx is pinned to the slot.
    **/
    s_externflash_drv[idx]     = *drv;
    s_externflash_drv[idx].idx = idx;

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

platform_err_t externflash_drv_read(UINT32_T                  addr,
                                             UINT8_T  *                data,
                                             UINT32_T                  size,
                                             wp_externflash_callback_t   cb,
                                             void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_read)
    {
        return p_drv->pf_externflash_read(p_drv, addr, data, size,
                                          cb, p_user_ctx);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

platform_err_t externflash_drv_write(UINT32_T                  addr,
                                              UINT8_T  *                data,
                                              UINT32_T                  size,
                                              wp_externflash_callback_t   cb,
                                              void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_write)
    {
        return p_drv->pf_externflash_write(p_drv, addr, data, size,
                                           cb, p_user_ctx);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

platform_err_t externflash_drv_write_noerase(UINT32_T                  addr,
                                                      UINT8_T  *                data,
                                                      UINT32_T                  size,
                                                      wp_externflash_callback_t   cb,
                                                      void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_write_noerase)
    {
        return p_drv->pf_externflash_write_noerase(p_drv, addr, data, size,
                                                   cb, p_user_ctx);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

platform_err_t externflash_drv_erase_chip(wp_externflash_callback_t   cb,
                                                   void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_erase_chip)
    {
        return p_drv->pf_externflash_erase_chip(p_drv, cb, p_user_ctx);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

platform_err_t externflash_drv_erase_sector(UINT32_T                  addr,
                                                     wp_externflash_callback_t   cb,
                                                     void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_erase_sector)
    {
        return p_drv->pf_externflash_erase_sector(p_drv, addr, cb, p_user_ctx);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

platform_err_t externflash_drv_sleep(wp_externflash_callback_t   cb,
                                              void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_sleep)
    {
        return p_drv->pf_externflash_sleep(p_drv, cb, p_user_ctx);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

platform_err_t externflash_drv_wakeup(wp_externflash_callback_t   cb,
                                               void     *         p_user_ctx)
{
    externflash_drv_t *p_drv = &s_externflash_drv[s_cur_externflash_drv_idx];
    if (p_drv->pf_externflash_wakeup)
    {
        return p_drv->pf_externflash_wakeup(p_drv, cb, p_user_ctx);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

//******************************* Functions *********************************//
