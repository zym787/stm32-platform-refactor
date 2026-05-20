/******************************************************************************
 * @file os_impl_timer.c
 *
 * @par dependencies
 * - osal_internal_timer.h
 * - osal_error.h
 * - FreeRTOS.h
 * - timers.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL timer implementation based on FreeRTOS timer APIs.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_internal_timer.h"
#include "osal_error.h"

#include "FreeRTOS.h"
#include "timers.h"

//******************************** Includes *********************************//

//******************************* Declaring *********************************//
typedef struct
{
	osal_timer_callback_t callback;
	void *p_arg;
} osal_timer_context_t;

//******************************* Declaring *********************************//

//***************************** Local Functions *****************************//
/**
 * @brief Convert OSAL timeout value to FreeRTOS timeout ticks.
 *
 * @param[in] timeout OSAL timeout value. OSAL_MAX_DELAY means wait forever.
 *
 * @return FreeRTOS TickType_t timeout value.
 */
static TickType_t osal_timer_timeout_to_ticks(osal_tick_type_t timeout)
{
	if (timeout == OSAL_MAX_DELAY)
	{
		return portMAX_DELAY;
	}

	return (TickType_t)timeout;
}

/**
 * @brief Map FreeRTOS timer command result to OSAL return code.
 *
 * @param[in] result FreeRTOS command result.
 * @param[in] timeout Command timeout in OSAL ticks.
 *
 * @return OSAL mapped status code.
 */
static int32_t osal_timer_command_result(BaseType_t result,
										 osal_tick_type_t timeout)
{
	if (result == pdPASS)
	{
		return OSAL_SUCCESS;
	}

	if (timeout == 0U)
	{
		return OSAL_TIMER_ERR_UNAVAILABLE;
	}

	return OSAL_ERROR_TIMEOUT;
}

/**
 * @brief Adapter callback called by FreeRTOS timer service task.
 *
 * @param[in] timer_handle Native FreeRTOS timer handle.
 */
static void osal_timer_callback_adapter(TimerHandle_t timer_handle)
{
	osal_timer_context_t *p_context;

	p_context = (osal_timer_context_t *)pvTimerGetTimerID(timer_handle);
	if ((NULL == p_context) || (NULL == p_context->callback))
	{
		return;
	}

	p_context->callback(p_context->p_arg);
}

//***************************** Local Functions *****************************//

//******************************* Functions *********************************//
/**
 * @brief Create a software timer.
 *
 * @param[out] p_timer_handle Output timer handle.
 * @param[in] p_timer_name Timer name string.
 * @param[in] period_ticks Timer period in ticks.
 * @param[in] auto_reload OSAL_TRUE for periodic mode.
 * @param[in] p_arg User callback context pointer.
 * @param[in] callback User callback function.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_TIMER_ERR_UNAVAILABLE.
 */
int32_t osal_timer_create_impl(osal_timer_handle_t *p_timer_handle,
							   const char *p_timer_name,
							   osal_tick_type_t period_ticks,
							   osal_base_type_t auto_reload,
							   void *p_arg,
							   osal_timer_callback_t callback)
{
	TimerHandle_t timer_handle;
	osal_timer_context_t *p_context;

	p_context = (osal_timer_context_t *)pvPortMalloc(sizeof(*p_context));
	if (NULL == p_context)
	{
		return OSAL_TIMER_ERR_UNAVAILABLE;
	}

	p_context->callback = callback;
	p_context->p_arg = p_arg;

	timer_handle = xTimerCreate(p_timer_name,
								(TickType_t)period_ticks,
								(auto_reload == OSAL_TRUE) ? pdTRUE : pdFALSE,
								(void *)p_context,
								osal_timer_callback_adapter);
	if (NULL == timer_handle)
	{
		vPortFree(p_context);
		return OSAL_TIMER_ERR_UNAVAILABLE;
	}

	*p_timer_handle = (osal_timer_handle_t)timer_handle;
	return OSAL_SUCCESS;
}

/**
 * @brief Delete a software timer.
 *
 * @param[in] timer_handle Timer handle.
 */
void osal_timer_delete_impl(osal_timer_handle_t timer_handle)
{
	TimerHandle_t native_timer;
	osal_timer_context_t *p_context;
	BaseType_t result;

	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return;
	}

	native_timer = (TimerHandle_t)timer_handle;
	p_context = (osal_timer_context_t *)pvTimerGetTimerID(native_timer);
	result = xTimerDelete(native_timer, 0U);
	if ((result == pdPASS) && (NULL != p_context))
	{
		vPortFree(p_context);
	}
}

/**
 * @brief Start a software timer.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] timeout Command timeout in OSAL ticks.
 *
 * @return OSAL mapped status code.
 */
int32_t osal_timer_start_impl(osal_timer_handle_t timer_handle,
							  osal_tick_type_t timeout)
{
	BaseType_t result;

	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xTimerStart((TimerHandle_t)timer_handle,
						 osal_timer_timeout_to_ticks(timeout));
	return osal_timer_command_result(result, timeout);
}

/**
 * @brief Stop a software timer.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] timeout Command timeout in OSAL ticks.
 *
 * @return OSAL mapped status code.
 */
int32_t osal_timer_stop_impl(osal_timer_handle_t timer_handle,
							 osal_tick_type_t timeout)
{
	BaseType_t result;

	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xTimerStop((TimerHandle_t)timer_handle,
						osal_timer_timeout_to_ticks(timeout));
	return osal_timer_command_result(result, timeout);
}

/**
 * @brief Reset a software timer.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] timeout Command timeout in OSAL ticks.
 *
 * @return OSAL mapped status code.
 */
int32_t osal_timer_reset_impl(osal_timer_handle_t timer_handle,
							  osal_tick_type_t timeout)
{
	BaseType_t result;

	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xTimerReset((TimerHandle_t)timer_handle,
						 osal_timer_timeout_to_ticks(timeout));
	return osal_timer_command_result(result, timeout);
}

/**
 * @brief Change timer period.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] new_period_ticks New timer period in ticks.
 * @param[in] timeout Command timeout in OSAL ticks.
 *
 * @return OSAL mapped status code.
 */
int32_t osal_timer_change_period_impl(osal_timer_handle_t timer_handle,
									  osal_tick_type_t new_period_ticks,
									  osal_tick_type_t timeout)
{
	BaseType_t result;

	if (xPortIsInsideInterrupt() == pdTRUE)
	{
		return OSAL_ERR_IN_ISR;
	}

	result = xTimerChangePeriod((TimerHandle_t)timer_handle,
								(TickType_t)new_period_ticks,
								osal_timer_timeout_to_ticks(timeout));
	return osal_timer_command_result(result, timeout);
}

//******************************* Functions *********************************//
