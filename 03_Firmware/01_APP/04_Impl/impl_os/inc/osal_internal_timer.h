/******************************************************************************
 * @file osal_internal_timer.h
 *
 * @par dependencies
 * - osal_timer.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL internal timer implementation interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_INTERNAL_TIMER_H__
#define __OSAL_INTERNAL_TIMER_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"

//******************************** Includes *********************************//

//******************************* Declaring *********************************//
typedef void (*osal_timer_callback_t)(void *p_arg);

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief Create software timer object.
 *
 * @param[out] p_timer_handle Output timer handle.
 * @param[in] p_timer_name Timer name.
 * @param[in] period_ticks Timer period in ticks.
 * @param[in] auto_reload Auto reload flag.
 * @param[in] p_arg Callback argument.
 * @param[in] callback Callback function.
 *
 * @return OSAL status code.
 */
int32_t osal_timer_create_impl       (osal_timer_handle_t   *  p_timer_handle,
                                      const char            *    p_timer_name,
                                      osal_tick_type_t           period_ticks,
                                      osal_base_type_t            auto_reload, 
                                      void                  *           p_arg,
                                      osal_timer_callback_t          callback);

/**
 * @brief Delete software timer object.
 *
 * @param[in] timer_handle Timer handle.
 */
void    osal_timer_delete_impl       (osal_timer_handle_t        timer_handle);

/**
 * @brief Start software timer object.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] timeout Command timeout in ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_timer_start_impl        (osal_timer_handle_t        timer_handle,
                                        osal_tick_type_t              timeout);

/**
 * @brief Stop software timer object.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] timeout Command timeout in ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_timer_stop_impl         (osal_timer_handle_t        timer_handle,
                                        osal_tick_type_t              timeout);

/**
 * @brief Reset software timer object.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] timeout Command timeout in ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_timer_reset_impl        (osal_timer_handle_t        timer_handle,
                                         osal_tick_type_t             timeout);

/**
 * @brief Change software timer period.
 *
 * @param[in] timer_handle Timer handle.
 * @param[in] new_period_ticks New period in ticks.
 * @param[in] timeout Command timeout in ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_timer_change_period_impl(osal_timer_handle_t        timer_handle,
                                         osal_tick_type_t    new_period_ticks,
                                         osal_tick_type_t             timeout);

//******************************* Functions *********************************//

#endif /* __OSAL_INTERNAL_TIMER_H__ */
