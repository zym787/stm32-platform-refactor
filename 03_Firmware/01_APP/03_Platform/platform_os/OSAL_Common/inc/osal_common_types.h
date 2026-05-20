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

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

//******************************* Functions *********************************//

#endif /* __OSAL_COMMON_TYPES_H__ */
