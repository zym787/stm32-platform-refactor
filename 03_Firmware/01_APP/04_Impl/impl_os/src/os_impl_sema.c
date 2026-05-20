/******************************************************************************
 * @file os_impl_sema.c
 *
 * @par dependencies
 * - osal_internal_sema.h
 * - osal_error.h
 * - FreeRTOS.h
 * - semphr.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL semaphore implementation based on FreeRTOS semaphore APIs.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_internal_sema.h"
#include "osal_error.h"

#include "FreeRTOS.h"
#include "semphr.h"

//******************************** Includes *********************************//

//***************************** Local Functions *****************************//
/**
 * @brief Convert OSAL timeout value to FreeRTOS timeout ticks.
 *
 * @param[in] timeout OSAL timeout value. OSAL_MAX_DELAY means wait forever.
 *
 * @return FreeRTOS TickType_t timeout value.
 */
static TickType_t osal_sema_timeout_to_ticks(osal_tick_type_t timeout)
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
 * @brief Create a counting semaphore object.
 *
 * @param[out] p_sema_handle Output semaphore handle.
 * @param[in] initial_count Initial semaphore count.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_SEM_FAILURE.
 */
int32_t osal_sema_create_impl(osal_sema_handle_t *p_sema_handle,
							  uint32_t initial_count)
{
	UBaseType_t max_count;
	SemaphoreHandle_t sema_handle;

	max_count = (initial_count == 0U) ? 1U : (UBaseType_t)initial_count;
	sema_handle = xSemaphoreCreateCounting(max_count,
										   (UBaseType_t)initial_count);
	if (NULL == sema_handle)
	{
		return OSAL_SEM_FAILURE;
	}

	*p_sema_handle = (osal_sema_handle_t)sema_handle;
	return OSAL_SUCCESS;
}

/**
 * @brief Delete a semaphore object.
 *
 * @param[in] sema_handle Semaphore handle.
 */
void osal_sema_delete_impl(osal_sema_handle_t sema_handle)
{
	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return;
	}

	vSemaphoreDelete((SemaphoreHandle_t)sema_handle);
}

/**
 * @brief Take (decrement) semaphore in task context.
 *
 * @param[in] sema_handle Semaphore handle.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERR_IN_ISR in ISR context,
 *         otherwise OSAL_SEM_TIMEOUT.
 */
int32_t osal_sema_take_impl(osal_sema_handle_t sema_handle,
							osal_tick_type_t timeout)
{
	BaseType_t result;

	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xSemaphoreTake((SemaphoreHandle_t)sema_handle,
							osal_sema_timeout_to_ticks(timeout));
	if (result == pdTRUE)
	{
		return OSAL_SUCCESS;
	}

	return OSAL_SEM_TIMEOUT;
}

/**
 * @brief Give (increment) semaphore in task context.
 *
 * @param[in] sema_handle Semaphore handle.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERR_IN_ISR in ISR context,
 *         otherwise OSAL_SEM_FAILURE.
 */
int32_t osal_sema_give_impl(osal_sema_handle_t sema_handle)
{
	BaseType_t result;

	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xSemaphoreGive((SemaphoreHandle_t)sema_handle);
	if (result == pdTRUE)
	{
		return OSAL_SUCCESS;
	}

	return OSAL_SEM_FAILURE;
}

/**
 * @brief Give semaphore from ISR context.
 *
 * @param[in] sema_handle Semaphore handle.
 * @param[out] p_higher_priority_task_woken Optional wakeup flag from ISR.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERROR when not in ISR,
 *         otherwise OSAL_SEM_FAILURE.
 */
int32_t osal_sema_give_from_isr_impl(
	osal_sema_handle_t sema_handle,
	osal_base_type_t *p_higher_priority_task_woken)
{
	BaseType_t result;

	if (xPortIsInsideInterrupt() != pdTRUE)
	{
		return OSAL_ERROR;
	}

	result = xSemaphoreGiveFromISR((SemaphoreHandle_t)sema_handle,
								   (BaseType_t *)p_higher_priority_task_woken);
	if (result == pdTRUE)
	{
		return OSAL_SUCCESS;
	}

	return OSAL_SEM_FAILURE;
}

//******************************* Functions *********************************//
