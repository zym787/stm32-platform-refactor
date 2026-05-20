/******************************************************************************
 * @file osal_internal_heap.h
 *
 * @par dependencies
 * - osal_heap.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL internal heap implementation interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_INTERNAL_HEAP_H__
#define __OSAL_INTERNAL_HEAP_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"

//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Allocate heap memory in OS implementation layer.
 *
 * @param[in] size Allocation size in bytes.
 *
 * @return Allocated pointer on success, NULL on failure.
 */
void *osal_heap_malloc_impl(size_t size);

/**
 * @brief Free heap memory in OS implementation layer.
 *
 * @param[in] ptr Pointer returned by osal_heap_malloc_impl.
 */
void  osal_heap_free_impl  (void  * ptr);
//******************************* Functions *********************************//

#endif /* __OSAL_INTERNAL_HEAP_H__ */
