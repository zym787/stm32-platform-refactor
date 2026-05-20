/******************************************************************************
 * @file osal_task.h
 *
 * @par dependencies
 * - osal_common_types.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL public task wrapper interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_TASK_H__
#define __OSAL_TASK_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
typedef void (*osal_task_entry_t)(void *p_arg);
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief Create task object.
 *
 * @param[out] p_task_handle Output task handle.
 * @param[in] p_task_name Task name.
 * @param[in] p_arg Task argument.
 * @param[in] task_entry Task entry function.
 * @param[in] stack_depth Task stack depth.
 * @param[in] priority Task priority.
 *
 * @return OSAL status code.
 */
int32_t osal_task_create(osal_task_handle_t *p_task_handle,
                         const char *p_task_name, 
                         void *p_arg, 
                         osal_task_entry_t task_entry,
                         uint32_t stack_depth, 
                         uint32_t priority);

/**
 * @brief Delete task object.
 *
 * @param[in] task_handle Task handle.
 *
 * @return OSAL status code.
 */
int32_t osal_task_delete   (osal_task_handle_t task_handle);

/**
 * @brief Delay current task.
 *
 * @param[in] ticks_to_delay Delay ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_task_delay    (osal_tick_type_t ticks_to_delay);

/**
 * @brief Yield current task.
 *
 * @return OSAL status code.
 */
int32_t osal_task_yield    (void);

/**
 * @brief Get current OS tick count.
 *
 * @return Current OSAL tick count.
 */
osal_tick_type_t osal_task_get_tick_count(void);

/**
 * @brief Get the handle of the currently running task.
 *
 * @return Current task handle, or NULL if called from ISR context.
 */
osal_task_handle_t osal_task_get_current_handle(void);

/**
 * @brief Enter critical section (disable interrupts).
 *        Must be paired with osal_critical_exit().
 */
void osal_critical_enter(void);

/**
 * @brief Exit critical section (restore interrupts).
 *        Must be paired with osal_critical_enter().
 */
void osal_critical_exit(void);

//******************************* Functions *********************************//

#endif /* __OSAL_TASK_H__ */
