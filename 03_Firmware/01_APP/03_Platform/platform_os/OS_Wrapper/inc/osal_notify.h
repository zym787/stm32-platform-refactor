/******************************************************************************
 * @file osal_notify.h
 *
 * @par dependencies
 * - osal_common_types.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL public task notification wrapper interfaces.
 *
 * @version V1.0 2026-4-15
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_NOTIFY_H__
#define __OSAL_NOTIFY_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
/**
 * @brief Action to perform when a task notification is sent.
 *
 * @note Enum values are kept identical to FreeRTOS eNotifyAction so that
 *       the implementation layer can cast directly without a translation table.
 */
typedef enum
{
    OSAL_NOTIFY_NO_ACTION                   = 0, /**< @brief No action; receiver's value is unchanged */
    OSAL_NOTIFY_SET_BITS                    = 1, /**< @brief OR the value into the receiver's notification value */
    OSAL_NOTIFY_INCREMENT                   = 2, /**< @brief Increment the receiver's notification value */
    OSAL_NOTIFY_SET_VALUE_WITH_OVERWRITE    = 3, /**< @brief Unconditionally set the notification value */
    OSAL_NOTIFY_SET_VALUE_WITHOUT_OVERWRITE = 4  /**< @brief Set only when the receiver has no pending notification */
} osal_notify_action_t;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief Send a task notification from an ISR.
 *
 * @param[in]  task_handle                   Handle of the task to notify.
 * @param[in]  value                         Notification value to send.
 * @param[in]  action                        Action to perform on the receiving task's value.
 * @param[out] p_higher_priority_task_woken  Set to OSAL_TRUE if a context switch is required.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERROR when not in ISR context,
 *         or OSAL_INVALID_POINTER on NULL handle.
 */
int32_t osal_notify_send_from_isr(osal_task_handle_t    task_handle,
                                   uint32_t              value,
                                   osal_notify_action_t  action,
                                   osal_base_type_t     *p_higher_priority_task_woken);

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
                         osal_tick_type_t  timeout);

//******************************* Functions *********************************//

#endif /* __OSAL_NOTIFY_H__ */
