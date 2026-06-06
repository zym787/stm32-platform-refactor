/******************************************************************************
 * @file bsp_wrapper_touch.c
 *
 * @par dependencies
 * - bsp_wrapper_touch.h
 *
 * @author Ethan-Hang
 *
 * @brief Implementation of the abstract touch interface.  Forwards each
 *        public API to the currently mounted driver vtable slot; returns
 *        PLATFORM_ERR_NO_RESOURCE if no driver was mounted.
 *
 * @version V1.0 2026-04-26
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_wrapper_touch.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define MAX_TOUCH_DRV_NUM 1
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static touch_drv_t s_touch_drv[MAX_TOUCH_DRV_NUM];
static UINT32_T    s_cur_touch_drv_idx = 0;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/**
 * @brief Register a concrete touch driver into the wrapper slot table.
 *
 * @param[in] idx : Slot index.
 * @param[in] drv : Pointer to vtable-filled driver struct.
 *
 * @return true on success, false on bad arguments.
 */
BOOL_T drv_adapter_touch_mount(UINT32_T idx, touch_drv_t *const drv)
{
    if (idx >= MAX_TOUCH_DRV_NUM || drv == NULL)
    {
        return false;
    }

    /**
    * touch_drv_t is a flat vtable, so a whole-struct copy captures every
    * slot — replaces the per-field copy. idx is pinned to the slot.
    **/
    s_touch_drv[idx]     = *drv;
    s_touch_drv[idx].idx = idx;

    s_cur_touch_drv_idx = idx;

    return true;
}

/**
 * @brief Forward driver-instantiation request to the active touch driver.
 *        Returns ERRORRESOURCE if no slot has an inst hook.
 */
platform_err_t touch_drv_inst(void)
{
    touch_drv_t *drv = &s_touch_drv[s_cur_touch_drv_idx];
    if (drv->pf_touch_drv_inst)
    {
        return drv->pf_touch_drv_inst(drv);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

/**
 * @brief Init the currently active driver.
 */
void touch_drv_init(void)
{
    touch_drv_t *drv = &s_touch_drv[s_cur_touch_drv_idx];
    if (drv->pf_touch_drv_init)
    {
        drv->pf_touch_drv_init(drv);
    }
}

/**
 * @brief Deinit the currently active driver.
 */
void touch_drv_deinit(void)
{
    touch_drv_t *drv = &s_touch_drv[s_cur_touch_drv_idx];
    if (drv->pf_touch_drv_deinit)
    {
        drv->pf_touch_drv_deinit(drv);
    }
}

/**
 * @brief Forward an ISR notification to the active touch driver.
 *
 * Runs in interrupt context; the registered hook must stay ISR-safe.
 * No-op if no driver is mounted or the slot has no ISR hook (i.e. the
 * adapter chose to leave it NULL).
 */
void touch_drv_isr_notify(void)
{
    touch_drv_t *drv = &s_touch_drv[s_cur_touch_drv_idx];
    if (drv->pf_touch_isr_notify)
    {
        drv->pf_touch_isr_notify(drv);
    }
}

/**
 * @brief Forward finger-count read to the driver.
 */
platform_err_t touch_get_finger_num(UINT8_T *const p_finger)
{
    touch_drv_t *drv = &s_touch_drv[s_cur_touch_drv_idx];
    if (drv->pf_touch_get_finger_num)
    {
        return drv->pf_touch_get_finger_num(drv, p_finger);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

/**
 * @brief Forward XY coordinate read to the driver.
 */
platform_err_t touch_get_xy(UINT16_T *const p_x, UINT16_T *const p_y)
{
    touch_drv_t *drv = &s_touch_drv[s_cur_touch_drv_idx];
    if (drv->pf_touch_get_xy)
    {
        return drv->pf_touch_get_xy(drv, p_x, p_y);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

/**
 * @brief Forward chip-id probe to the driver.
 */
platform_err_t touch_get_chip_id(UINT8_T *const p_chip_id)
{
    touch_drv_t *drv = &s_touch_drv[s_cur_touch_drv_idx];
    if (drv->pf_touch_get_chip_id)
    {
        return drv->pf_touch_get_chip_id(drv, p_chip_id);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}

/**
 * @brief Forward gesture id read to the driver.
 */
platform_err_t touch_get_gesture(UINT8_T *const p_gesture)
{
    touch_drv_t *drv = &s_touch_drv[s_cur_touch_drv_idx];
    if (drv->pf_touch_get_gesture)
    {
        return drv->pf_touch_get_gesture(drv, p_gesture);
    }
    return PLATFORM_ERR_NO_RESOURCE;
}
//******************************* Functions *********************************//
