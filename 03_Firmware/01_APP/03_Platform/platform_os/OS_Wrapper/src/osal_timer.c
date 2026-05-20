/******************************************************************************
 * @file osal_timer.c
 *
 * @par dependencies
 * - osal_timer.h
 * - osal_internal_timer.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL timer wrapper interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_timer.h"
#include "osal_internal_timer.h"
#include "osal_error.h"

//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Create software timer object.
 *
 * @param[out] p_timer_handle Output timer handle.
 * @param[in] p_timer_name Timer name.
 * @param[in] period_ticks Timer period in ticks.
 * @param[in] auto_reload Auto reload flag.
 * @param[in] p_arg User callback argument.
 * @param[in] callback User callback function.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER,
 *         OSAL_TIMER_ERR_INVALID_ARGS or lower layer error code.
 */
int32_t osal_timer_create(osal_timer_handle_t *p_timer_handle,
                          const char          *  p_timer_name,
                          osal_tick_type_t       period_ticks,
                          osal_base_type_t        auto_reload, 
                          void                *         p_arg,
                          osal_timer_callback_t      callback)
{
    if ((NULL == p_timer_handle) || (NULL == p_timer_name) ||
        (NULL == callback))
    {
        return OSAL_INVALID_POINTER;
    }

    if ((0U == period_ticks) ||
        ((OSAL_TRUE != auto_reload) && (OSAL_FALSE != auto_reload)))
    {
        return OSAL_TIMER_ERR_INVALID_ARGS;
    }

    return osal_timer_create_impl(p_timer_handle, p_timer_name, period_ticks,
                                  auto_reload, p_arg, callback);
}

/**
 * @brief Delete software timer object.
 *
 * @param[in] timer_handle Timer handle.
 */
void osal_timer_delete(osal_timer_handle_t timer_handle)
{
    if (NULL == timer_handle)
    {
        return;
    }

    osal_timer_delete_impl(timer_handle);
}

/**
 * @brief Start software timer object.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_timer_start(osal_timer_handle_t timer_handle,
                            osal_tick_type_t      timeout)
{
    if (NULL == timer_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_timer_start_impl(timer_handle, timeout);
}

/**
 * @brief Stop software timer object.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_timer_stop(osal_timer_handle_t timer_handle,
                            osal_tick_type_t     timeout)
{
    if (NULL == timer_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_timer_stop_impl(timer_handle, timeout);
}

/**
 * @brief Reset software timer object.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_timer_reset(osal_timer_handle_t timer_handle,
                            osal_tick_type_t      timeout)
{
    if (NULL == timer_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_timer_reset_impl(timer_handle, timeout);
}

/**
 * @brief Change software timer period.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] new_period_ticks New period ticks.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER,
 *         OSAL_TIMER_ERR_INVALID_ARGS or lower layer error code.
 */
int32_t osal_timer_change_period(osal_timer_handle_t        timer_handle,
                                    osal_tick_type_t    new_period_ticks,
                                    osal_tick_type_t             timeout)
{
    if (NULL == timer_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    if (0U == new_period_ticks)
    {
        return OSAL_TIMER_ERR_INVALID_ARGS;
    }

    return osal_timer_change_period_impl(timer_handle, new_period_ticks,
                                         timeout);
}

//******************************* Functions *********************************//
