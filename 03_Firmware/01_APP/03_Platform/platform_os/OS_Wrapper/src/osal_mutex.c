/******************************************************************************
 * @file osal_mutex.c
 *
 * @par dependencies
 * - osal_mutex.h
 * - osal_internal_mutex.h
 * - osal_error.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL mutex wrapper implementation.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_mutex.h"
#include "osal_internal_mutex.h"
#include "osal_error.h"

//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Initialize mutex object.
 *
 * @param[out] p_mutex_handle Output mutex handle.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_mutex_init(osal_mutex_handle_t *p_mutex_handle)
{
    if (NULL == p_mutex_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_mutex_create_impl(p_mutex_handle);
}

/**
 * @brief Delete mutex object.
 *
 * @param[in] mutex_handle Mutex handle.
 */
void osal_mutex_delete(osal_mutex_handle_t mutex_handle)
{
    if (NULL == mutex_handle)
    {
        return;
    }

    osal_mutex_delete_impl(mutex_handle);
}

/**
 * @brief Take mutex object.
 *
 * @param[in] mutex_handle Mutex handle.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_mutex_take(osal_mutex_handle_t mutex_handle,
                           osal_tick_type_t      timeout)
{
    if (NULL == mutex_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_mutex_take_impl(mutex_handle, timeout);
}

/**
 * @brief Give mutex object.
 *
 * @param[in] mutex_handle Mutex handle.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_mutex_give(osal_mutex_handle_t mutex_handle)
{
    if (NULL == mutex_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_mutex_give_impl(mutex_handle);
}

//******************************* Functions *********************************//
