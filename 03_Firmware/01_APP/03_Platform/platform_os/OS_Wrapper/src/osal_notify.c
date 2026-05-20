/******************************************************************************
 * @file osal_notify.c
 *
 * @par dependencies
 * - osal_notify.h
 * - osal_internal_notify.h
 * - osal_error.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL task notification wrapper interfaces.
 *
 * @version V1.0 2026-4-15
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_notify.h"
#include "osal_internal_notify.h"
#include "osal_error.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Send a task notification from an ISR.
 *
 * @param[in]  task_handle                   Handle of the task to notify.
 * @param[in]  value                         Notification value to send.
 * @param[in]  action                        Action to perform on the receiving task's value.
 * @param[out] p_higher_priority_task_woken  Set to OSAL_TRUE if a context switch is required.
 *
 * @return OSAL_SUCCESS on success, OSAL_INVALID_POINTER on NULL handle,
 *         otherwise lower layer error code.
 */
int32_t osal_notify_send_from_isr(osal_task_handle_t    task_handle,
                                   uint32_t              value,
                                   osal_notify_action_t  action,
                                   osal_base_type_t     *p_higher_priority_task_woken)
{
    if (NULL == task_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_notify_send_from_isr_impl(task_handle, value, action,
                                          p_higher_priority_task_woken);
}

/**
 * @brief Wait for a task notification in task context.
 *
 * @param[in]  bits_to_clear_on_entry  Bits to clear in the notification value on entry.
 * @param[in]  bits_to_clear_on_exit   Bits to clear in the notification value on exit.
 * @param[out] p_notification_value    Optional pointer to receive the notification value.
 * @param[in]  timeout                 Wait timeout in OSAL ticks. Use OSAL_MAX_DELAY to wait forever.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERR_IN_ISR in ISR context,
 *         or OSAL_NOTIFY_TIMEOUT on timeout.
 */
int32_t osal_notify_wait(uint32_t          bits_to_clear_on_entry,
                         uint32_t          bits_to_clear_on_exit,
                         uint32_t         *p_notification_value,
                         osal_tick_type_t  timeout)
{
    return osal_notify_wait_impl(bits_to_clear_on_entry, bits_to_clear_on_exit,
                                 p_notification_value, timeout);
}

//******************************* Functions *********************************//
