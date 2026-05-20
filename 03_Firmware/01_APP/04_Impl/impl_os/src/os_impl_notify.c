/******************************************************************************
 * @file os_impl_notify.c
 *
 * @par dependencies
 * - osal_internal_notify.h
 * - osal_error.h
 * - FreeRTOS.h
 * - task.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL task notification implementation based on FreeRTOS task notify APIs.
 *
 * @version V1.0 2026-4-15
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_internal_notify.h"
#include "osal_error.h"

#include "FreeRTOS.h"
#include "task.h"
//******************************** Includes *********************************//

//***************************** Local Functions *****************************//
/**
 * @brief Convert OSAL timeout value to FreeRTOS timeout ticks.
 *
 * @param[in] timeout OSAL timeout value. OSAL_MAX_DELAY means wait forever.
 *
 * @return FreeRTOS TickType_t timeout value.
 */
static TickType_t osal_notify_timeout_to_ticks(osal_tick_type_t timeout)
{
	if (timeout == OSAL_MAX_DELAY)
	{
		return portMAX_DELAY;
	}

	return (TickType_t)timeout;
}

//***************************** Local Functions *****************************//

//******************************* Functions *********************************//
/**
 * @brief Send a task notification from an ISR.
 *
 * @param[in]  task_handle                   Handle of the task to notify.
 * @param[in]  value                         Notification value to send.
 * @param[in]  action                        Action to perform on the receiving task's value.
 * @param[out] p_higher_priority_task_woken  Set to OSAL_TRUE if a context switch is required.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERROR when not in ISR context.
 */
int32_t osal_notify_send_from_isr_impl(osal_task_handle_t    task_handle,
                                        uint32_t              value,
                                        osal_notify_action_t  action,
                                        osal_base_type_t     *p_higher_priority_task_woken)
{
	if (xPortIsInsideInterrupt() != pdTRUE)
	{
		return OSAL_ERROR;
	}

	xTaskNotifyFromISR((TaskHandle_t)task_handle,
	                   (uint32_t)value,
	                   (eNotifyAction)action,
	                   (BaseType_t *)p_higher_priority_task_woken);

	return OSAL_SUCCESS;
}

/**
 * @brief Wait for a task notification in task context.
 *
 * @param[in]  bits_to_clear_on_entry  Bits to clear in the notification value on entry.
 * @param[in]  bits_to_clear_on_exit   Bits to clear in the notification value on exit.
 * @param[out] p_notification_value    Optional pointer to receive the notification value.
 * @param[in]  timeout                 Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERR_IN_ISR in ISR context,
 *         or OSAL_ERROR_TIMEOUT on timeout.
 */
int32_t osal_notify_wait_impl(uint32_t          bits_to_clear_on_entry,
                               uint32_t          bits_to_clear_on_exit,
                               uint32_t         *p_notification_value,
                               osal_tick_type_t  timeout)
{
	BaseType_t result;

	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xTaskNotifyWait(bits_to_clear_on_entry,
	                         bits_to_clear_on_exit,
	                         (uint32_t *)p_notification_value,
	                         osal_notify_timeout_to_ticks(timeout));

	if (result == pdTRUE)
	{
		return OSAL_SUCCESS;
	}

	return OSAL_NOTIFY_TIMEOUT;
}

//******************************* Functions *********************************//
