/******************************************************************************
 * @file osal_event_group.c
 *
 * @par dependencies
 * - osal_event_group.h
 * - osal_internal_event_group.h
 * - osal_error.h
 * - osal_macros.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL event group wrapper interfaces.
 *
 * @version V1.0 2026-4-12
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_event_group.h"
#include "osal_internal_event_group.h"
#include "osal_error.h"
#include "osal_macros.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Create an event group object.
 *
 * @param[out] p_handle Output event group handle.
 *
 * @return OSAL_SUCCESS on success, OSAL_INVALID_POINTER or lower layer error.
 */
int32_t osal_event_group_create(osal_event_group_handle_t *p_handle)
{
    if (NULL == p_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_event_group_create_impl(p_handle);
}

/**
 * @brief Delete an event group object.
 *
 * @param[in] handle Event group handle.
 */
void osal_event_group_delete(osal_event_group_handle_t handle)
{
    if (NULL == handle)
    {
        return;
    }

    osal_event_group_delete_impl(handle);
}

/**
 * @brief Set one or more bits in the event group (task context only).
 *
 * @param[in] handle Event group handle.
 * @param[in] bits   Bit mask to set.
 *
 * @return OSAL_SUCCESS on success, OSAL_INVALID_POINTER or OSAL_ERR_IN_ISR.
 */
int32_t osal_event_group_set_bits(osal_event_group_handle_t handle,
                                   uint32_t                  bits)
{
    if (NULL == handle)
    {
        return OSAL_INVALID_POINTER;
    }

    if (OSAL_IS_IN_ISR())
    {
        return OSAL_ERR_IN_ISR;
    }

    return osal_event_group_set_bits_impl(handle, bits);
}

/**
 * @brief Clear one or more bits in the event group (task context only).
 *
 * @param[in] handle Event group handle.
 * @param[in] bits   Bit mask to clear.
 *
 * @return OSAL_SUCCESS on success, OSAL_INVALID_POINTER or OSAL_ERR_IN_ISR.
 */
int32_t osal_event_group_clear_bits(osal_event_group_handle_t handle,
                                     uint32_t                  bits)
{
    if (NULL == handle)
    {
        return OSAL_INVALID_POINTER;
    }

    if (OSAL_IS_IN_ISR())
    {
        return OSAL_ERR_IN_ISR;
    }

    return osal_event_group_clear_bits_impl(handle, bits);
}

/**
 * @brief Block until one or all specified bits are set in the event group.
 *
 * @param[in]  handle        Event group handle.
 * @param[in]  bits_to_wait  Bit mask to wait for.
 * @param[in]  clear_on_exit true: matched bits cleared atomically before return.
 * @param[in]  wait_for_all  true: wait until ALL bits set; false: any bit.
 * @param[in]  timeout       Wait timeout in OSAL ticks (OSAL_MAX_DELAY = forever).
 * @param[out] p_bits_set    Bits set at time of return. May be NULL.
 *
 * @return OSAL_SUCCESS when wait condition satisfied, OSAL_INVALID_POINTER,
 *         OSAL_ERR_IN_ISR, or OSAL_ERROR_TIMEOUT on timeout.
 */
int32_t osal_event_group_wait_bits(osal_event_group_handle_t  handle,
                                    uint32_t                   bits_to_wait,
                                    bool                       clear_on_exit,
                                    bool                       wait_for_all,
                                    osal_tick_type_t           timeout,
                                    uint32_t                  *p_bits_set)
{
    if (NULL == handle)
    {
        return OSAL_INVALID_POINTER;
    }

    if (OSAL_IS_IN_ISR())
    {
        return OSAL_ERR_IN_ISR;
    }

    return osal_event_group_wait_bits_impl(handle, bits_to_wait, clear_on_exit,
                                            wait_for_all, timeout, p_bits_set);
}

/**
 * @brief Set one or more bits in the event group from ISR context.
 *
 * @param[in]  handle                       Event group handle.
 * @param[in]  bits                         Bit mask to set.
 * @param[out] p_higher_priority_task_woken Wakeup flag. May be NULL.
 *
 * @return OSAL_SUCCESS on success, OSAL_INVALID_POINTER or lower layer error.
 */
int32_t osal_event_group_set_bits_from_isr(
                    osal_event_group_handle_t  handle,
                    uint32_t                   bits,
                    osal_base_type_t          *p_higher_priority_task_woken)
{
    if (NULL == handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_event_group_set_bits_from_isr_impl(handle, bits,
                                                    p_higher_priority_task_woken);
}
//******************************* Functions *********************************//
