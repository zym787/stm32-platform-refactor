/******************************************************************************
 * @file os_impl_event_group.c
 *
 * @par dependencies
 * - osal_internal_event_group.h
 * - osal_error.h
 * - FreeRTOS.h
 * - event_groups.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL event group implementation based on FreeRTOS event group APIs.
 *
 * @version V1.0 2026-4-12
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_internal_event_group.h"
#include "osal_error.h"

#include "FreeRTOS.h"
#include "event_groups.h"
//******************************** Includes *********************************//

//***************************** Local Functions *****************************//
/**
 * @brief Convert OSAL timeout value to FreeRTOS timeout ticks.
 *
 * @param[in] timeout OSAL timeout value. OSAL_MAX_DELAY means wait forever.
 *
 * @return FreeRTOS TickType_t timeout value.
 */
static TickType_t osal_eg_timeout_to_ticks(osal_tick_type_t timeout)
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
 * @brief Create an event group object.
 *
 * @param[out] p_handle Output event group handle.
 *
 * @return OSAL_SUCCESS on success, OSAL_EVT_GROUP_FAILURE on failure.
 */
int32_t osal_event_group_create_impl(osal_event_group_handle_t *p_handle)
{
    EventGroupHandle_t eg_handle;

    eg_handle = xEventGroupCreate();
    if (NULL == eg_handle)
    {
        return OSAL_EVT_GROUP_FAILURE;
    }

    *p_handle = (osal_event_group_handle_t)eg_handle;
    return OSAL_SUCCESS;
}

/**
 * @brief Delete an event group object.
 *
 * @param[in] handle Event group handle.
 */
void osal_event_group_delete_impl(osal_event_group_handle_t handle)
{
    vEventGroupDelete((EventGroupHandle_t)handle);
}

/**
 * @brief Set bits in the event group from task context.
 *
 * @param[in] handle Event group handle.
 * @param[in] bits   Bit mask to set.
 *
 * @return OSAL_SUCCESS on success.
 */
int32_t osal_event_group_set_bits_impl(osal_event_group_handle_t handle,
                                        uint32_t                  bits)
{
    xEventGroupSetBits((EventGroupHandle_t)handle, (EventBits_t)bits);
    return OSAL_SUCCESS;
}

/**
 * @brief Clear bits in the event group from task context.
 *
 * @param[in] handle Event group handle.
 * @param[in] bits   Bit mask to clear.
 *
 * @return OSAL_SUCCESS on success.
 */
int32_t osal_event_group_clear_bits_impl(osal_event_group_handle_t handle,
                                          uint32_t                  bits)
{
    xEventGroupClearBits((EventGroupHandle_t)handle, (EventBits_t)bits);
    return OSAL_SUCCESS;
}

/**
 * @brief Block until one or all specified bits are set in the event group.
 *
 * @param[in]  handle        Event group handle.
 * @param[in]  bits_to_wait  Bit mask to wait for.
 * @param[in]  clear_on_exit Clear matched bits before returning.
 * @param[in]  wait_for_all  Wait for all bits vs. any bit.
 * @param[in]  timeout       OSAL timeout in ticks.
 * @param[out] p_bits_set    Bits set at time of return. May be NULL.
 *
 * @return OSAL_SUCCESS if condition satisfied, OSAL_ERROR_TIMEOUT otherwise.
 */
int32_t osal_event_group_wait_bits_impl(osal_event_group_handle_t handle,
                                         uint32_t                  bits_to_wait,
                                         bool                      clear_on_exit,
                                         bool                      wait_for_all,
                                         osal_tick_type_t          timeout,
                                         uint32_t                 *p_bits_set)
{
    EventBits_t result;

    result = xEventGroupWaitBits(
                (EventGroupHandle_t)handle,
                (EventBits_t)bits_to_wait,
                clear_on_exit  ? pdTRUE : pdFALSE,
                wait_for_all   ? pdTRUE : pdFALSE,
                osal_eg_timeout_to_ticks(timeout));

    if (p_bits_set != NULL)
    {
        *p_bits_set = (uint32_t)result;
    }

    if (wait_for_all)
    {
        if ((result & (EventBits_t)bits_to_wait) == (EventBits_t)bits_to_wait)
        {
            return OSAL_SUCCESS;
        }
    }
    else
    {
        if ((result & (EventBits_t)bits_to_wait) != 0U)
        {
            return OSAL_SUCCESS;
        }
    }

    return OSAL_ERROR_TIMEOUT;
}

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
    osal_base_type_t          *p_higher_priority_task_woken)
{
    BaseType_t result;
    BaseType_t woken = pdFALSE;

    result = xEventGroupSetBitsFromISR(
                (EventGroupHandle_t)handle,
                (EventBits_t)bits,
                &woken);

    if (p_higher_priority_task_woken != NULL)
    {
        *p_higher_priority_task_woken = (osal_base_type_t)woken;
    }

    return (result == pdPASS) ? OSAL_SUCCESS : OSAL_EVT_GROUP_FAILURE;
}
//******************************* Functions *********************************//
