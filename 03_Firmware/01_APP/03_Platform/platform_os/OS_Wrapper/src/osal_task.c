/******************************************************************************
 * @file osal_task.c
 *
 * @par dependencies
 * - osal_task.h
 * - osal_internal_task.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL task wrapper interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_task.h"
#include "osal_internal_task.h"
#include "osal_error.h"
#include "osal_macros.h"

//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Create task object.
 *
 * @param[out] p_task_handle Output task handle.
 * @param[in] p_task_name Task name.
 * @param[in] p_arg Task entry argument.
 * @param[in] task_entry Task entry function.
 * @param[in] stack_depth Task stack depth.
 * @param[in] priority Task priority.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER,
 *         OSAL_ERR_INVALID_SIZE or lower layer error code.
 */
int32_t osal_task_create(osal_task_handle_t *p_task_handle,
                         const char *p_task_name, 
                         void *p_arg, 
                         osal_task_entry_t task_entry,
                         uint32_t stack_depth, 
                         uint32_t priority)
{
    if ((NULL == p_task_handle) || (NULL == p_task_name) ||
        (NULL == task_entry))
    {
        return OSAL_INVALID_POINTER;
    }

    if (0U == stack_depth)
    {
        return OSAL_ERR_INVALID_SIZE;
    }

    return osal_task_create_impl(p_task_handle, p_task_name, p_arg, task_entry,
                                 stack_depth, priority);
}

/**
 * @brief Delete task object.
 *
 * @param[in] task_handle Task handle.
 *
 * @return OSAL_SUCCESS on success, OSAL_INVALID_POINTER when handle is
 *         invalid, or OSAL_ERR_IN_ISR when called in ISR.
 */
int32_t osal_task_delete(osal_task_handle_t task_handle)
{
    if (NULL == task_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    if (OSAL_IS_IN_ISR())
    {
        return OSAL_ERR_IN_ISR;
    }

    osal_task_delete_impl(task_handle);
    return OSAL_SUCCESS;
}

/**
 * @brief Delay current task.
 *
 * @param[in] ticks_to_delay Delay ticks.
 *
 * @return OSAL_SUCCESS on success, or OSAL_ERR_IN_ISR when called in ISR.
 */
int32_t osal_task_delay(osal_tick_type_t ticks_to_delay)
{
    if (OSAL_IS_IN_ISR())
    {
        return OSAL_ERR_IN_ISR;
    }

    osal_task_delay_impl(ticks_to_delay);
    return OSAL_SUCCESS;
}

/**
 * @brief Yield current task.
 *
 * @return OSAL_SUCCESS on success, or OSAL_ERR_IN_ISR when called in ISR.
 */
int32_t osal_task_yield(void)
{
    if (OSAL_IS_IN_ISR())
    {
        return OSAL_ERR_IN_ISR;
    }

    osal_task_yield_impl();
    return OSAL_SUCCESS;
}

/**
 * @brief Get current OS tick count.
 *
 * @return Current OSAL tick count.
 */
osal_tick_type_t osal_task_get_tick_count(void)
{
    if (OSAL_IS_IN_ISR())
    {
        return osal_task_get_tick_count_from_isr_impl();
    }

    return osal_task_get_tick_count_impl();
}

/**
 * @brief Get the handle of the currently running task.
 *
 * @return Current task handle, or NULL when invoked from ISR context.
 */
osal_task_handle_t osal_task_get_current_handle(void)
{
    if (OSAL_IS_IN_ISR())
    {
        return NULL;
    }

    return osal_task_get_current_handle_impl();
}

/**
 * @brief Enter critical section (disable interrupts).
 *        Must be paired with osal_critical_exit().
 */
void osal_critical_enter(void)
{
    osal_critical_enter_impl();
}

/**
 * @brief Exit critical section (restore interrupts).
 *        Must be paired with osal_critical_enter().
 */
void osal_critical_exit(void)
{
    osal_critical_exit_impl();
}

//******************************* Functions *********************************//
