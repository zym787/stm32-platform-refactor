/******************************************************************************
 * @file osal_internal_queue.h
 *
 * @par dependencies
 * - osal_queue.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL internal queue implementation interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_INTERNAL_QUEUE_H__
#define __OSAL_INTERNAL_QUEUE_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"

//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Create queue object.
 *
 * @param[out] p_queue_handle Output queue handle.
 * @param[in] queue_depth Queue length.
 * @param[in] item_size Item size in bytes.
 *
 * @return OSAL status code.
 */
int32_t osal_queue_create_impl       (osal_queue_handle_t  *   p_queue_handle,
                                                   size_t         queue_depth, 
                                                   size_t           item_size);

/**
 * @brief Delete queue object.
 *
 * @param[in] queue_handle Queue handle.
 */
void osal_queue_delete_impl          (osal_queue_handle_t        queue_handle);
   
/**
 * @brief Send queue item.
 *
 * @param[in] queue_handle Queue handle.
 * @param[in] p_data Item data pointer.
 * @param[in] timeout Wait timeout in ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_queue_send_impl         (osal_queue_handle_t        queue_handle, 
                                               const void  *           p_data,
                                         osal_tick_type_t             timeout);
   
/**
 * @brief Receive queue item.
 *
 * @param[in] queue_handle Queue handle.
 * @param[out] p_data Output data pointer.
 * @param[in] timeout Wait timeout in ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_queue_receive_impl      (osal_queue_handle_t        queue_handle, 
                                                     void  *           p_data,
                                         osal_tick_type_t             timeout);
   
/**
 * @brief Send queue item from ISR.
 *
 * @param[in] queue_handle Queue handle.
 * @param[in] p_data Item data pointer.
 * @param[out] p_higher_priority_task_woken Optional wakeup flag.
 *
 * @return OSAL status code.
 */
int32_t osal_queue_send_from_isr_impl(osal_queue_handle_t        queue_handle,
                                               const void  *           p_data,
                                         osal_base_type_t  *\
                                                 p_higher_priority_task_woken);

/**
 * @brief Overwrite mailbox item.
 *
 * @param[in] queue_handle Mailbox handle.
 * @param[in] p_data Mailbox payload pointer.
 *
 * @return OSAL status code.
 */
int32_t osal_mailbox_overwrite_impl  (osal_queue_handle_t         queue_handle,
                                               const void  *           p_data);

/**
 * @brief Check mailbox pending item status.
 *
 * @param[in] p_queue_handle Pointer to queue handle.
 *
 * @return OSAL status code.
 */
int32_t osal_mailbox_peek_impl       (osal_queue_handle_t  *   p_queue_handle);

/**
 * @brief Get number of items currently in the queue.
 *
 * @param[in] queue_handle Queue handle.
 *
 * @return Number of items waiting in the queue.
 */
uint32_t osal_queue_messages_waiting_impl(osal_queue_handle_t queue_handle);

//******************************* Functions *********************************//

#endif /* __OSAL_INTERNAL_QUEUE_H__ */
