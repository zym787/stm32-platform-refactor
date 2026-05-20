/******************************************************************************
 * @file osal_sema.c
 *
 * @par dependencies
 * - osal_sema.h
 * - osal_internal_sema.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL semaphore wrapper interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_sema.h"
#include "osal_internal_sema.h"
#include "osal_error.h"

//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Initialize semaphore object.
 *
 * @param[out] p_sema_handle Output semaphore handle.
 * @param[in] initial_count Initial semaphore count.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_sema_init(osal_sema_handle_t *p_sema_handle,
                       uint32_t            initial_count)
{
    if (NULL == p_sema_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_sema_create_impl(p_sema_handle, initial_count);
}

/**
 * @brief Delete semaphore object.
 *
 * @param[in] sema_handle Semaphore handle.
 */
void osal_sema_delete(osal_sema_handle_t sema_handle)
{
    if (NULL == sema_handle)
    {
        return;
    }

    osal_sema_delete_impl(sema_handle);
}

/**
 * @brief Take semaphore object.
 *
 * @param[in] sema_handle Semaphore handle.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_sema_take(osal_sema_handle_t sema_handle, osal_tick_type_t timeout)
{
    if (NULL == sema_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_sema_take_impl(sema_handle, timeout);
}

/**
 * @brief Give semaphore object.
 *
 * @param[in] sema_handle Semaphore handle.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_sema_give(osal_sema_handle_t sema_handle)
{
    if (NULL == sema_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_sema_give_impl(sema_handle);
}

/**
 * @brief Give semaphore object from ISR.
 *
 * @param[in] sema_handle Semaphore handle.
 * @param[out] p_higher_priority_task_woken Optional wakeup flag from ISR.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_sema_give_from_isr(osal_sema_handle_t sema_handle,
                                osal_base_type_t  *p_higher_priority_task_woken)
{
    if (NULL == sema_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_sema_give_from_isr_impl(sema_handle,
                                        p_higher_priority_task_woken);
}

//******************************* Functions *********************************//
