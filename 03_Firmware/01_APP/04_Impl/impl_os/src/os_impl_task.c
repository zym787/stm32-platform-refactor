/******************************************************************************
 * @file os_impl_task.c
 *
 * @par dependencies
 * - osal_internal_task.h
 * - osal_error.h
 * - FreeRTOS.h
 * - task.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL task implementation based on FreeRTOS task APIs.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_internal_task.h"
#include "osal_error.h"

#include "FreeRTOS.h"
#include "task.h"

//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Create a task object.
 *
 * @param[out] p_task_handle Output task handle.
 * @param[in] p_task_name Task name string.
 * @param[in] p_arg Task entry argument.
 * @param[in] task_entry Task entry function.
 * @param[in] stack_depth Task stack depth.
 * @param[in] priority Task priority.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_ERROR.
 */
int32_t osal_task_create_impl(osal_task_handle_t *p_task_handle,
							  const char *p_task_name,
							  void *p_arg,
							  osal_task_entry_t task_entry,
							  uint32_t stack_depth,
							  uint32_t priority)
{
	BaseType_t result;

	result = xTaskCreate((TaskFunction_t)task_entry,
						 p_task_name,
						 (configSTACK_DEPTH_TYPE)stack_depth,
						 p_arg,
						 (UBaseType_t)priority,
						 (TaskHandle_t *)p_task_handle);
	if (result == pdPASS)
	{
		return OSAL_SUCCESS;
	}

	return OSAL_ERROR;
}

/**
 * @brief Delete a task object.
 *
 * @param[in] task_handle Task handle.
 */
void osal_task_delete_impl(osal_task_handle_t task_handle)
{
	vTaskDelete((TaskHandle_t)task_handle);
}

/**
 * @brief Delay current task for specified ticks.
 *
 * @param[in] ticks_to_delay Delay ticks.
 */
void osal_task_delay_impl(osal_tick_type_t ticks_to_delay)
{
	vTaskDelay((TickType_t)ticks_to_delay);
}

/**
 * @brief Yield current task in scheduler.
 */
void osal_task_yield_impl(void)
{
	taskYIELD();
}

/**
 * @brief Get current system tick count in task context.
 *
 * @return Current OSAL tick count.
 */
osal_tick_type_t osal_task_get_tick_count_impl(void)
{
	return (osal_tick_type_t)xTaskGetTickCount();
}

/**
 * @brief Get current system tick count in ISR context.
 *
 * @return Current OSAL tick count.
 */
osal_tick_type_t osal_task_get_tick_count_from_isr_impl(void)
{
	return (osal_tick_type_t)xTaskGetTickCountFromISR();
}

/**
 * @brief Get the handle of the currently running task.
 *
 * @return Current task handle.
 */
osal_task_handle_t osal_task_get_current_handle_impl(void)
{
	return (osal_task_handle_t)xTaskGetCurrentTaskHandle();
}

/**
 * @brief Enter critical section by disabling interrupts.
 */
void osal_critical_enter_impl(void)
{
	vPortEnterCritical();
}

/**
 * @brief Exit critical section by restoring interrupts.
 */
void osal_critical_exit_impl(void)
{
	vPortExitCritical();
}

//******************************* Functions *********************************//
