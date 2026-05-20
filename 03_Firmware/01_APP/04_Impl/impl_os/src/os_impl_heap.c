/******************************************************************************
 * @file os_impl_heap.c
 *
 * @par dependencies
 * - osal_internal_heap.h
 * - FreeRTOS.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL heap implementation based on FreeRTOS heap APIs.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_internal_heap.h"

#include "FreeRTOS.h"

//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Allocate heap memory from FreeRTOS heap.
 *
 * @param[in] size Requested allocation size in bytes.
 *
 * @return Allocated memory pointer, or NULL when allocation fails.
 */
void *osal_heap_malloc_impl(size_t size)
{
	return pvPortMalloc(size);
}

/**
 * @brief Free heap memory allocated by osal_heap_malloc_impl.
 *
 * @param[in] ptr Pointer returned by osal_heap_malloc_impl.
 */
void osal_heap_free_impl(void *ptr)
{
	vPortFree(ptr);
}

//******************************* Functions *********************************//
