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

/**
 * @brief Declare static storage for one task and a descriptor bound to it.
 *
 * Expands to a stack buffer, a TCB blob and an OsalTaskStatic descriptor,
 * all file-scope static. Pass &name as the p_static argument of
 * osal_task_create_static(). The word count must match the stack_depth
 * passed to that call.
 *
 * @param[in] name  Identifier for the generated descriptor.
 * @param[in] words Stack capacity in 32-bit words.
 */
#define OSAL_TASK_STATIC_DEFINE(name, words)                                  \
    static uint32_t           name##_stk[(words)];                            \
    static OsalTaskTcbStorage name##_tcb;                                     \
    static OsalTaskStatic     name = {                                        \
        .p_stack     = name##_stk,                                            \
        .stack_words = (words),                                               \
        .p_tcb       = &name##_tcb                                            \
    }
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
 * @brief Create a task using caller-provided static storage.
 *
 * No heap allocation: the stack buffer and TCB come from p_static
 * (typically declared with OSAL_TASK_STATIC_DEFINE). Suitable for
 * always-on tasks that are created once at boot and never deleted.
 *
 * @param[out] p_task_handle Output task handle.
 * @param[in] p_task_name Task name.
 * @param[in] p_arg Task argument.
 * @param[in] task_entry Task entry function.
 * @param[in] stack_depth Stack depth in words; must be <= p_static->stack_words.
 * @param[in] priority Task priority.
 * @param[in] p_static Static storage descriptor.
 *
 * @return OSAL_SUCCESS on success, OSAL_INVALID_POINTER for a NULL argument,
 *         OSAL_ERR_INVALID_SIZE when stack_depth is 0 or exceeds the static
 *         capacity, or OSAL_ERROR on lower-layer failure.
 */
int32_t osal_task_create_static(osal_task_handle_t *p_task_handle,
                                const char *p_task_name,
                                void *p_arg,
                                osal_task_entry_t task_entry,
                                uint32_t stack_depth,
                                uint32_t priority,
                                OsalTaskStatic *p_static);

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
