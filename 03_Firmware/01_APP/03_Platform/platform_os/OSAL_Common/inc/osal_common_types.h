/******************************************************************************
 * @file osal_common_types.h
 *
 * @par dependencies
 * - stddef.h
 * - stdint.h
 * - stdbool.h
 *
 * @author Ethan-Hang
 *
 * @brief Common OSAL base types and handle definitions.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_COMMON_TYPES_H__
#define __OSAL_COMMON_TYPES_H__

//******************************** Includes *********************************//
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
 * @brief Select 16-bit (1) or 32-bit (0) OS tick type.
 */
#ifndef OSAL_USE_16_BIT_TICKS
#define OSAL_USE_16_BIT_TICKS (0)
#endif

#if (1 == OSAL_USE_16_BIT_TICKS)
/**
 * @brief Maximum tick delay value when 16-bit tick type is enabled.
 */
#define OSAL_MAX_DELAY (0xFFFFUL)
typedef uint16_t osal_tick_type_t;
#else
/**
 * @brief Maximum tick delay value when 32-bit tick type is enabled.
 */
#define OSAL_MAX_DELAY (0xFFFFFFFFUL)
typedef uint32_t osal_tick_type_t;
#endif

/**
 * @brief OSAL portable base integer type.
 */
typedef long  osal_base_type_t;

/**
 * @brief Opaque task object handle.
 */
typedef void *osal_task_handle_t;

/**
 * @brief Opaque queue object handle.
 */
typedef void *osal_queue_handle_t;

/**
 * @brief Opaque semaphore object handle.
 */
typedef void *osal_sema_handle_t;

/**
 * @brief Opaque mutex object handle.
 */
typedef void *osal_mutex_handle_t;

/**
 * @brief Opaque software timer object handle.
 */
typedef void *osal_timer_handle_t;

/**
 * @brief Opaque event group object handle.
 */
typedef void *osal_event_group_handle_t;

/**
 * @brief OSAL true constant.
 */
#define OSAL_TRUE  ((osal_base_type_t)1)

/**
 * @brief OSAL false constant.
 */
#define OSAL_FALSE ((osal_base_type_t)0)

/**
 * @brief Word count reserved for one statically-allocated task control block.
 *
 * Sized conservatively to hold the underlying RTOS TCB (Cortex-M4
 * FreeRTOS StaticTask_t is ~88 B = 22 words). The actual fit is verified
 * by a _Static_assert in the OS impl layer, keeping the RTOS type out of
 * this public header.
 */
#define OSAL_TCB_STORAGE_WORDS (24U)

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/**
 * @brief Task allocation strategy selector.
 */
typedef enum
{
    OSAL_TASK_ALLOC_DYNAMIC = 0, /**< Stack + TCB from the RTOS heap.       */
    OSAL_TASK_ALLOC_STATIC  = 1  /**< Caller-provided static storage.       */
} OsalTaskAlloc;

/**
 * @brief Opaque storage for one statically-allocated task control block.
 *
 * Treated as an opaque blob by callers; the OS impl layer casts it to the
 * concrete RTOS TCB type.
 */
typedef struct
{
    uint32_t tcb[OSAL_TCB_STORAGE_WORDS];
} OsalTaskTcbStorage;

/**
 * @brief Descriptor binding a static task's stack buffer to its TCB storage.
 *
 * Populated once (typically via OSAL_TASK_STATIC_DEFINE) and passed to
 * osal_task_create_static().
 */
typedef struct
{
    uint32_t           *p_stack;     /**< Word-aligned stack buffer.        */
    uint32_t            stack_words; /**< Stack capacity in 32-bit words.   */
    OsalTaskTcbStorage *p_tcb;       /**< TCB storage.                      */
} OsalTaskStatic;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//

//******************************* Functions *********************************//

#endif /* __OSAL_COMMON_TYPES_H__ */
