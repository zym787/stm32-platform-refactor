/******************************************************************************
 * @file osal_queue.h
 *
 * @par dependencies
 * - osal_common_types.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL public queue wrapper interfaces.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_QUEUE_H__
#define __OSAL_QUEUE_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Create queue object.
 *
 * @param[out] p_queue_handle Output queue handle.
 * @param[in] queue_depth Queue depth in items.
 * @param[in] item_size Item size in bytes.
 *
 * @return OSAL status code.
 */
int32_t osal_queue_create       (osal_queue_handle_t * p_queue_handle,
                                              size_t      queue_depth, 
                                              size_t        item_size);

/**
 * @brief Delete queue object.
 *
 * @param[in] queue_handle Queue handle.
 */
void osal_queue_delete          (osal_queue_handle_t     queue_handle);

/**
 * @brief Send queue item.
 *
 * @param[in] queue_handle Queue handle.
 * @param[in] p_data Item data pointer.
 * @param[in] timeout Wait timeout in ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_queue_send         (osal_queue_handle_t     queue_handle, 
                                          const void *         p_data,
                                    osal_tick_type_t          timeout);

/**
 * @brief Receive queue item.
 *
 * @param[in] queue_handle Queue handle.
 * @param[out] p_data Output data pointer.
 * @param[in] timeout Wait timeout in ticks.
 *
 * @return OSAL status code.
 */
int32_t osal_queue_receive      (osal_queue_handle_t     queue_handle, 
                                                void *         p_data,
                                    osal_tick_type_t          timeout);

/**
 * @brief Send queue item from ISR.
 *
 * @param[in] queue_handle Queue handle.
 * @param[in] p_data Item data pointer.
 * @param[out] p_higher_priority_task_woken Optional wakeup flag.
 *
 * @return OSAL status code.
 */
int32_t osal_queue_send_from_isr(osal_queue_handle_t     queue_handle,
                                          const void  *        p_data,
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
int32_t osal_mailbox_overwrite  (osal_queue_handle_t     queue_handle,
                                          const void  *        p_data);

/**
 * @brief Check mailbox pending item status.
 *
 * @param[in] p_queue_handle Pointer to queue handle.
 *
 * @return OSAL status code.
 */
int32_t osal_mailbox_peek       (osal_queue_handle_t  *p_queue_handle);

/**
 * @brief Get number of items currently in the queue.
 *
 * Safe to call from both task and ISR context.
 *
 * @param[in] queue_handle Queue handle.
 *
 * @return Number of items waiting, or 0 if handle is NULL.
 */
uint32_t osal_queue_messages_waiting(osal_queue_handle_t queue_handle);

//******************************* Functions *********************************//

#endif /* __OSAL_QUEUE_H__ */
