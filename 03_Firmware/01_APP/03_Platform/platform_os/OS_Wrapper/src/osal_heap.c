/******************************************************************************
 * @file osal_heap.c
 *
 * @par dependencies
 * - osal_heap.h
 * - osal_internal_heap.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL heap wrapper implementation.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_heap.h"
#include "osal_internal_heap.h"

//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Allocate memory through OSAL wrapper.
 *
 * @param[in] size Allocation size in bytes.
 *
 * @return Memory pointer on success, NULL on invalid size or allocation
 *         failure.
 */
void *osal_heap_malloc(size_t size)
{
    if (0U == size)
    {
        return NULL;
    }

    return osal_heap_malloc_impl(size);
}

/**
 * @brief Free memory through OSAL wrapper.
 *
 * @param[in] ptr Pointer returned by osal_heap_malloc.
 */
void osal_heap_free(void *ptr)
{
    if (NULL == ptr)
    {
        return;
    }

    osal_heap_free_impl(ptr);
}

//******************************* Functions *********************************//
