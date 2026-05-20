/******************************************************************************
 * @file osal_heap.h
 *
 * @par dependencies
 * - osal_common_types.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL public heap wrapper interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_HEAP_H__
#define __OSAL_HEAP_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Allocate memory through OSAL wrapper.
 *
 * @param[in] size Allocation size in bytes.
 *
 * @return Allocated pointer on success, NULL on failure.
 */
void *osal_heap_malloc(size_t size);

/**
 * @brief Free memory through OSAL wrapper.
 *
 * @param[in] ptr Pointer returned by osal_heap_malloc.
 */
void  osal_heap_free  (void  * ptr);

//******************************* Functions *********************************//

#endif /* __OSAL_HEAP_H__ */
