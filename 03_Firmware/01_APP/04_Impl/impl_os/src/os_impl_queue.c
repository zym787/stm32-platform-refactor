/******************************************************************************
 * @file os_impl_queue.c
 *
 * @par dependencies
 * - osal_internal_queue.h
 * - osal_error.h
 * - FreeRTOS.h
 * - queue.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL queue implementation based on FreeRTOS queue APIs.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_internal_queue.h"
#include "osal_error.h"

#include "FreeRTOS.h"
#include "queue.h"

//******************************** Includes *********************************//

//***************************** Local Functions *****************************//
/**
 * @brief Convert OSAL timeout value to FreeRTOS timeout ticks.
 *
 * @param[in] timeout OSAL timeout value. OSAL_MAX_DELAY means wait forever.
 *
 * @return FreeRTOS TickType_t timeout value.
 */
static TickType_t osal_queue_timeout_to_ticks(osal_tick_type_t timeout)
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
 * @brief Create a queue object.
 *
 * @param[out] p_queue_handle Output queue handle.
 * @param[in] queue_depth Queue length in items.
 * @param[in] item_size Size of each item in bytes.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_ERROR.
 */
int32_t osal_queue_create_impl(osal_queue_handle_t *p_queue_handle,
							   size_t queue_depth,
							   size_t item_size)
{
	QueueHandle_t queue_handle;

	queue_handle = xQueueCreate((UBaseType_t)queue_depth,
								(UBaseType_t)item_size);
	if (NULL == queue_handle)
	{
		return OSAL_ERROR;
	}

	*p_queue_handle = (osal_queue_handle_t)queue_handle;
	return OSAL_SUCCESS;
}

/**
 * @brief Delete a queue object.
 *
 * @param[in] queue_handle Queue handle to delete.
 */
void osal_queue_delete_impl(osal_queue_handle_t queue_handle)
{
	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return;
	}

	vQueueDelete((QueueHandle_t)queue_handle);
}

/**
 * @brief Send one item to queue tail in task context.
 *
 * @param[in] queue_handle Queue handle.
 * @param[in] p_data Pointer to item data.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERR_IN_ISR in ISR context,
 *         OSAL_QUEUE_FULL for non-blocking full queue,
 *         otherwise OSAL_QUEUE_TIMEOUT.
 */
int32_t osal_queue_send_impl(osal_queue_handle_t queue_handle,
							 const void *p_data,
							 osal_tick_type_t timeout)
{
	BaseType_t result;

	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xQueueSendToBack((QueueHandle_t)queue_handle,
							  p_data,
							  osal_queue_timeout_to_ticks(timeout));
	if (result == pdTRUE)
	{
		return OSAL_SUCCESS;
	}

	if (timeout == 0U)
	{
		return OSAL_QUEUE_FULL;
	}

	return OSAL_QUEUE_TIMEOUT;
}

/**
 * @brief Receive one item from queue in task context.
 *
 * @param[in] queue_handle Queue handle.
 * @param[out] p_data Output buffer for one queue item.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERR_IN_ISR in ISR context,
 *         OSAL_QUEUE_EMPTY for non-blocking empty queue,
 *         otherwise OSAL_QUEUE_TIMEOUT.
 */
int32_t osal_queue_receive_impl(osal_queue_handle_t queue_handle,
								void *p_data,
								osal_tick_type_t timeout)
{
	BaseType_t result;

	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xQueueReceive((QueueHandle_t)queue_handle,
						   p_data,
						   osal_queue_timeout_to_ticks(timeout));
	if (result == pdTRUE)
	{
		return OSAL_SUCCESS;
	}

	if (timeout == 0U)
	{
		return OSAL_QUEUE_EMPTY;
	}

	return OSAL_QUEUE_TIMEOUT;
}

/**
 * @brief Send one item to queue tail from ISR.
 *
 * @param[in] queue_handle Queue handle.
 * @param[in] p_data Pointer to item data.
 * @param[out] p_higher_priority_task_woken Optional wakeup flag from ISR.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_QUEUE_FULL.
 */
int32_t osal_queue_send_from_isr_impl(osal_queue_handle_t queue_handle,
									  const void *p_data,
									  osal_base_type_t *p_higher_priority_task_woken)
{
	BaseType_t result;

	result = xQueueSendToBackFromISR((QueueHandle_t)queue_handle,
									 p_data,
									 (BaseType_t *)p_higher_priority_task_woken);
	if (result == pdTRUE)
	{
		return OSAL_SUCCESS;
	}

	return OSAL_QUEUE_FULL;
}

/**
 * @brief Overwrite mailbox message in task context.
 *
 * @param[in] queue_handle Mailbox queue handle.
 * @param[in] p_data Pointer to mailbox payload.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERR_IN_ISR in ISR context,
 *         otherwise OSAL_ERROR.
 */
int32_t osal_mailbox_overwrite_impl(osal_queue_handle_t queue_handle,
									const void *p_data)
{
	BaseType_t result;

	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xQueueOverwrite((QueueHandle_t)queue_handle, p_data);
	if (result == pdTRUE)
	{
		return OSAL_SUCCESS;
	}

	return OSAL_ERROR;
}

/**
 * @brief Check whether mailbox/queue has pending messages.
 *
 * @param[in] p_queue_handle Pointer to queue handle.
 *
 * @return OSAL_SUCCESS when queue is not empty,
 *         OSAL_QUEUE_EMPTY when no pending message,
 *         OSAL_INVALID_POINTER when handle is invalid.
 */
int32_t osal_mailbox_peek_impl(osal_queue_handle_t *p_queue_handle)
{
	QueueHandle_t queue_handle;
	UBaseType_t message_count;

	if (NULL == p_queue_handle)
	{
		return OSAL_INVALID_POINTER;
	}
	queue_handle = (QueueHandle_t)(*p_queue_handle);
	
	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		message_count = uxQueueMessagesWaitingFromISR(queue_handle);
	}
	else
	{
		message_count = uxQueueMessagesWaiting(queue_handle);
	}

	if (message_count > 0U)
	{
		return OSAL_SUCCESS;
	}

	return OSAL_QUEUE_EMPTY;
}

/**
 * @brief Get number of items currently in the queue.
 *
 * Safe to call from both task and ISR context.
 *
 * @param[in] queue_handle Queue handle.
 *
 * @return Number of items waiting in the queue.
 */
uint32_t osal_queue_messages_waiting_impl(osal_queue_handle_t queue_handle)
{
	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return (uint32_t)uxQueueMessagesWaitingFromISR(
		                     (QueueHandle_t)queue_handle);
	}

	return (uint32_t)uxQueueMessagesWaiting((QueueHandle_t)queue_handle);
}

//******************************* Functions *********************************//
