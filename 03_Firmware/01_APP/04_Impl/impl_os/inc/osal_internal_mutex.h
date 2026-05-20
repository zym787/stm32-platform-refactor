/******************************************************************************
 * @file osal_internal_mutex.h
 *
 * @par dependencies
 * - osal_mutex.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL internal mutex implementation interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_INTERNAL_MUTEX_H__
#define __OSAL_INTERNAL_MUTEX_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"

//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Create mutex object.
 *
 * @param[out] p_mutex_handle Output mutex handle.
 *
 * @return OSAL status code.
 */
int32_t osal_mutex_create_impl(osal_mutex_handle_t *p_mutex_handle);

/**
 * @brief Delete mutex object.
 *
 * @param[in] mutex_handle Mutex handle.
 */
void    osal_mutex_delete_impl(osal_mutex_handle_t    mutex_handle);

/**
 * @brief Take mutex object.
 *
 * @param[in] mutex_handle Mutex handle.
 * @param[in] timeout Wait timeout in ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_mutex_take_impl  (osal_mutex_handle_t    mutex_handle,
                                  osal_tick_type_t         timeout);

/**
 * @brief Give mutex object.
 *
 * @param[in] mutex_handle Mutex handle.
 *
 * @return OSAL status code.
 */
int32_t osal_mutex_give_impl  (osal_mutex_handle_t    mutex_handle);
//******************************* Functions *********************************//

#endif /* __OSAL_INTERNAL_MUTEX_H__ */
