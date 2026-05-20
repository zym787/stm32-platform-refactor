/******************************************************************************
 * @file osal_sema.h
 *
 * @par dependencies
 * - osal_common_types.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL public semaphore wrapper interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_SEMA_H__
#define __OSAL_SEMA_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Initialize semaphore object.
 *
 * @param[out] p_sema_handle Output semaphore handle.
 * @param[in] initial_count Initial count value.
 *
 * @return OSAL status code.
 */
int32_t osal_sema_init         (osal_sema_handle_t * p_sema_handle,
                                          uint32_t   initial_count);

/**
 * @brief Delete semaphore object.
 *
 * @param[in] sema_handle Semaphore handle.
 */
void    osal_sema_delete       (osal_sema_handle_t     sema_handle);

/**
 * @brief Take semaphore object.
 *
 * @param[in] sema_handle Semaphore handle.
 * @param[in] timeout Wait timeout in ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_sema_take         (osal_sema_handle_t     sema_handle,
                                  osal_tick_type_t         timeout);

/**
 * @brief Give semaphore object.
 *
 * @param[in] sema_handle Semaphore handle.
 *
 * @return OSAL status code.
 */
int32_t osal_sema_give         (osal_sema_handle_t     sema_handle);

/**
 * @brief Give semaphore object from ISR.
 *
 * @param[in] sema_handle Semaphore handle.
 * @param[out] p_higher_priority_task_woken Optional wakeup flag.
 *
 * @return OSAL status code.
 */
int32_t osal_sema_give_from_isr(osal_sema_handle_t     sema_handle,
                                  osal_base_type_t * \
                                       p_higher_priority_task_woken);

//******************************* Functions *********************************//

#endif /* __OSAL_SEMA_H__ */
