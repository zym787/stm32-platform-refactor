/******************************************************************************
 * @file osal_mutex.h
 *
 * @par dependencies
 * - osal_common_types.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL public mutex wrapper interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_MUTEX_H__
#define __OSAL_MUTEX_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Initialize mutex object.
 *
 * @param[out] p_mutex_handle Output mutex handle.
 *
 * @return OSAL status code.
 */
int32_t osal_mutex_init  (osal_mutex_handle_t *p_mutex_handle);

/**
 * @brief Delete mutex object.
 *
 * @param[in] mutex_handle Mutex handle.
 */
void    osal_mutex_delete(osal_mutex_handle_t    mutex_handle);

/**
 * @brief Take mutex object.
 *
 * @param[in] mutex_handle Mutex handle.
 * @param[in] timeout Wait timeout in ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_mutex_take  (osal_mutex_handle_t    mutex_handle,
                             osal_tick_type_t         timeout);

/**
 * @brief Give mutex object.
 *
 * @param[in] mutex_handle Mutex handle.
 *
 * @return OSAL status code.
 */
int32_t osal_mutex_give  (osal_mutex_handle_t    mutex_handle);
//******************************* Functions *********************************//

#endif /* __OSAL_MUTEX_H__ */
