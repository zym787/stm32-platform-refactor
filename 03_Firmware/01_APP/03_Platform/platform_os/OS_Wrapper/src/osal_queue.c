/******************************************************************************
 * @file osal_queue.c
 *
 * @par dependencies
 * - osal_queue.h
 * - osal_internal_queue.h
 * - osal_error.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL queue wrapper implementation.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "osal_queue.h"
#include "osal_internal_queue.h"
#include "osal_error.h"

//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Create queue object.
 *
 * @param[out] p_queue_handle Output queue handle.
 * @param[in] queue_depth Queue depth in items.
 * @param[in] item_size Item size in bytes.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER,
 *         OSAL_QUEUE_INVALID_SIZE or lower layer error code.
 */
int32_t osal_queue_create(osal_queue_handle_t *p_queue_handle,
                                       size_t     queue_depth, 
                                       size_t       item_size)
{
    if (NULL == p_queue_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    if ((0U == queue_depth) || (0U == item_size))
    {
        return OSAL_QUEUE_INVALID_SIZE;
    }

    return osal_queue_create_impl(p_queue_handle, queue_depth, item_size);
}

/**
 * @brief Delete queue object.
 *
 * @param[in] queue_handle Queue handle.
 */
void osal_queue_delete(osal_queue_handle_t queue_handle)
{
    if (NULL == queue_handle)
    {
        return;
    }

    osal_queue_delete_impl(queue_handle);
}

/**
 * @brief Send one item to queue.
 *
 * @param[in] queue_handle Queue handle.
 * @param[in] p_data Pointer to item data.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_queue_send(osal_queue_handle_t  queue_handle, 
                                 const void   *    p_data,
                           osal_tick_type_t       timeout)
{
    if ((NULL == queue_handle) || (NULL == p_data))
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_queue_send_impl(queue_handle, p_data, timeout);
}

/**
 * @brief Receive one item from queue.
 *
 * @param[in] queue_handle Queue handle.
 * @param[out] p_data Output buffer for one item.
 * @param[in] timeout Wait timeout in OSAL ticks.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_queue_receive(osal_queue_handle_t  queue_handle, 
                            void   *    p_data,
                    osal_tick_type_t       timeout)
{
    if ((NULL == queue_handle) || (NULL == p_data))
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_queue_receive_impl(queue_handle, p_data, timeout);
}

/**
 * @brief Send one item to queue from ISR.
 *
 * @param[in] queue_handle Queue handle.
 * @param[in] p_data Pointer to item data.
 * @param[out] p_higher_priority_task_woken Optional wakeup flag from ISR.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_queue_send_from_isr(osal_queue_handle_t queue_handle,
                                          const void  *    p_data,
                   osal_base_type_t *p_higher_priority_task_woken)
{
    if ((NULL == queue_handle) || (NULL == p_data))
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_queue_send_from_isr_impl(queue_handle, p_data,
                                        p_higher_priority_task_woken);
}

/**
 * @brief Overwrite mailbox message.
 *
 * @param[in] queue_handle Mailbox handle.
 * @param[in] p_data Pointer to mailbox data.
 *
 * @return OSAL_SUCCESS on success, otherwise OSAL_INVALID_POINTER or lower
 *         layer error code.
 */
int32_t osal_mailbox_overwrite(osal_queue_handle_t queue_handle,
                                        const void   *   p_data)
{
    if ((NULL == queue_handle) || (NULL == p_data))
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_mailbox_overwrite_impl(queue_handle, p_data);
}

/**
 * @brief Check mailbox pending message status.
 *
 * @param[in] p_queue_handle Pointer to queue handle.
 *
 * @return OSAL_SUCCESS when queue not empty, otherwise OSAL_INVALID_POINTER
 *         or lower layer error code.
 */
int32_t osal_mailbox_peek(osal_queue_handle_t *p_queue_handle)
{
    if (NULL == p_queue_handle)
    {
        return OSAL_INVALID_POINTER;
    }

    return osal_mailbox_peek_impl(p_queue_handle);
}

/**
 * @brief Get number of items currently in the queue.
 *
 * @param[in] queue_handle Queue handle.
 *
 * @return Number of items waiting, or 0 if handle is NULL.
 */
uint32_t osal_queue_messages_waiting(osal_queue_handle_t queue_handle)
{
    if (NULL == queue_handle)
    {
        return 0U;
    }

    return osal_queue_messages_waiting_impl(queue_handle);
}

//******************************* Functions *********************************//
