/******************************************************************************
 * @file osal_internal_event_group.h
 *
 * @par dependencies
 * - osal_common_types.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL internal event group implementation interfaces.
 *
 * @version V1.0 2026-4-12
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_INTERNAL_EVENT_GROUP_H__
#define __OSAL_INTERNAL_EVENT_GROUP_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Create an event group object.
 *
 * @param[out] p_handle Output event group handle.
 *
 * @return OSAL_SUCCESS on success, OSAL_EVT_GROUP_FAILURE on failure.
 */
int32_t osal_event_group_create_impl  (osal_event_group_handle_t *p_handle);

/**
 * @brief Delete an event group object.
 *
 * @param[in] handle Event group handle.
 */
void    osal_event_group_delete_impl  (osal_event_group_handle_t  handle);

/**
 * @brief Set bits in the event group.
 *
 * @param[in] handle Event group handle.
 * @param[in] bits   Bit mask to set.
 *
 * @return OSAL_SUCCESS on success.
 */
int32_t osal_event_group_set_bits_impl(osal_event_group_handle_t  handle,
                                        uint32_t                   bits);

/**
 * @brief Clear bits in the event group.
 *
 * @param[in] handle Event group handle.
 * @param[in] bits   Bit mask to clear.
 *
 * @return OSAL_SUCCESS on success.
 */
int32_t osal_event_group_clear_bits_impl(osal_event_group_handle_t  handle,
                                          uint32_t                   bits);

/**
 * @brief Block until one or all specified bits are set.
 *
 * @param[in]  handle        Event group handle.
 * @param[in]  bits_to_wait  Bit mask to wait for.
 * @param[in]  clear_on_exit Clear matched bits before returning.
 * @param[in]  wait_for_all  Wait for all bits vs. any bit.
 * @param[in]  timeout       OSAL timeout ticks.
 * @param[out] p_bits_set    Bits set at time of return. May be NULL.
 *
 * @return OSAL_SUCCESS if condition satisfied, OSAL_ERROR_TIMEOUT otherwise.
 */
int32_t osal_event_group_wait_bits_impl(
                    osal_event_group_handle_t  handle,
                    uint32_t                   bits_to_wait,
                    bool                       clear_on_exit,
                    bool                       wait_for_all,
                    osal_tick_type_t           timeout,
                    uint32_t                  *p_bits_set);

/**
 * @brief Set bits in the event group from ISR context.
 *
 * @param[in]  handle                       Event group handle.
 * @param[in]  bits                         Bit mask to set.
 * @param[out] p_higher_priority_task_woken Wakeup flag. May be NULL.
 *
 * @return OSAL_SUCCESS on success, OSAL_EVT_GROUP_FAILURE on failure.
 */
int32_t osal_event_group_set_bits_from_isr_impl(
                    osal_event_group_handle_t  handle,
                    uint32_t                   bits,
                    osal_base_type_t          *p_higher_priority_task_woken);
//******************************* Functions *********************************//

#endif /* __OSAL_INTERNAL_EVENT_GROUP_H__ */
