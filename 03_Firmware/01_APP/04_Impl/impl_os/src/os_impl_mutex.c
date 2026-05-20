/******************************************************************************
 * @file os_impl_mutex.c
 *
 * @par dependencies
 * - osal_internal_mutex.h
 * - osal_error.h
 * - FreeRTOS.h
 * - semphr.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL mutex implementation based on FreeRTOS mutex APIs.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_internal_mutex.h"
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
static TickType_t osal_mutex_timeout_to_ticks(osal_tick_type_t timeout)
{
	/* Map OSAL wait-forever sentinel to FreeRTOS infinite timeout. */
	if (timeout == OSAL_MAX_DELAY)
	{
		return portMAX_DELAY;
	}

	return (TickType_t)timeout;
}

//***************************** Local Functions *****************************//

//******************************* Functions *********************************//
/**
 * @brief Create a mutex object.
 *
 * @param[out] p_mutex_handle Output handle for the created mutex.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_ERROR.
 */
int32_t osal_mutex_create_impl(osal_mutex_handle_t *p_mutex_handle)
{
	SemaphoreHandle_t mutex_handle;

	/* Create native FreeRTOS mutex and map handle to OSAL type. */
	mutex_handle = xSemaphoreCreateMutex();
	if (NULL == mutex_handle)
	{
		return OSAL_ERROR;
	}

	*p_mutex_handle = (osal_mutex_handle_t)mutex_handle;
	return OSAL_SUCCESS;
}

/**
 * @brief Delete a mutex object.
 *
 * @param[in] mutex_handle Mutex handle to delete.
 */
void osal_mutex_delete_impl(osal_mutex_handle_t mutex_handle)
{
	/* FreeRTOS mutex delete must run in task context. */
	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return;
	}

	vSemaphoreDelete((SemaphoreHandle_t)mutex_handle);
}

/**
 * @brief Take (lock) a mutex.
 *
 * @param[in] mutex_handle Mutex handle.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERR_IN_ISR in ISR context,
 *         OSAL_ERROR_TIMEOUT on timeout/failure.
 */
int32_t osal_mutex_take_impl(osal_mutex_handle_t mutex_handle,
							 osal_tick_type_t timeout)
{
	BaseType_t result;

	/* Mutex take API is task-context only in this wrapper. */
	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xSemaphoreTake((SemaphoreHandle_t)mutex_handle,
							osal_mutex_timeout_to_ticks(timeout));
	if (result == pdTRUE)
	{
		return OSAL_SUCCESS;
	}

	return OSAL_ERROR_TIMEOUT;
}

/**
 * @brief Give (unlock) a mutex.
 *
 * @param[in] mutex_handle Mutex handle.
 *
 * @return OSAL_SUCCESS on success, OSAL_ERR_IN_ISR in ISR context,
 *         otherwise OSAL_ERROR.
 */
int32_t osal_mutex_give_impl(osal_mutex_handle_t mutex_handle)
{
	BaseType_t result;

	/* Mutex give API is task-context only in this wrapper. */
	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xSemaphoreGive((SemaphoreHandle_t)mutex_handle);
	if (result == pdTRUE)
	{
		return OSAL_SUCCESS;
	}

	return OSAL_ERROR;
}

//******************************* Functions *********************************//
