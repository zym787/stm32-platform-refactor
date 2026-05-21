/******************************************************************************
 * @file wt588_intergration.c
 *
 * @par dependencies
 * - wt588_intergration.h
 * - bsp_wt588_hal_port.h
 * - osal_wrapper_adapter.h
 * - osal_error.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection for the WT588 voice handler.
 *
 * Provides concrete implementations for every OS interface the WT588 handler
 * needs (tasks/queues/mutexes, heap) and wires them together with the HAL
 * port interfaces into wt588_handler_input_args / wt588_handler_instance,
 * which are passed to wt588_handler_inst() at startup.
 *
 * Hardware-specific implementations (GPIO, busy-detect, PWM-DMA) live in
 * bsp_wt588_hal_port.c. Changing the target MCU or RTOS requires editing
 * that file and this file respectively.
 *
 * @version V1.1 2026-04-17
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "wt588_integration.h"
#include "bsp_wt588_hal_port.h"
#include "osal_wrapper_adapter.h"
#include "osal_error.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/* ========================================================================= */
/*  OS adapter functions                                                      */
/* ========================================================================= */

static void os_delay_ms_adapter(uint32_t ms)
{
    (void)osal_task_delay((osal_tick_type_t)ms);
}

static wt_handler_status_t task_create_adapter(void **const   task_handle,
                                               void          *parameters,
                                      void (*entry)(void *const args),
                                               char const    *name,
                                               uint16_t       stack_depth,
                                               uint32_t       priority)
{
    int32_t ret = osal_task_create(
                      (osal_task_handle_t *)task_handle,
                      name,
                      parameters,
                      (osal_task_entry_t)entry,
                      (uint32_t)stack_depth,
                      priority);
    return (OSAL_SUCCESS == ret) ? WT_HANDLER_OK : WT_HANDLER_ERROR;
}

static wt_handler_status_t task_delete_adapter(void *const task_handle)
{
    int32_t ret = osal_task_delete((osal_task_handle_t)task_handle);
    return (OSAL_SUCCESS == ret) ? WT_HANDLER_OK : WT_HANDLER_ERROR;
}

static wt_handler_status_t queue_create_adapter(void **const  queue_handle,
                                                uint32_t const queue_length,
                                                uint32_t const item_size)
{
    int32_t ret = osal_queue_create(
                      (osal_queue_handle_t *)queue_handle,
                      (size_t)queue_length,
                      (size_t)item_size);
    return (OSAL_SUCCESS == ret) ? WT_HANDLER_OK : WT_HANDLER_ERROR;
}

static wt_handler_status_t queue_delete_adapter(void *const queue_handle)
{
    osal_queue_delete((osal_queue_handle_t)queue_handle);
    return WT_HANDLER_OK;
}

static wt_handler_status_t queue_send_adapter(void *const queue_handle,
                                              void *const item,
                                              uint32_t    timeout)
{
    int32_t ret = osal_queue_send((osal_queue_handle_t)queue_handle,
                                  item,
                                  (osal_tick_type_t)timeout);
    return (OSAL_SUCCESS == ret) ? WT_HANDLER_OK : WT_HANDLER_ERROR;
}

static wt_handler_status_t queue_get_adapter(void *const queue_handle,
                                             void *const item,
                                             uint32_t    timeout)
{
    int32_t ret = osal_queue_receive((osal_queue_handle_t)queue_handle,
                                     item,
                                     (osal_tick_type_t)timeout);
    return (OSAL_SUCCESS == ret) ? WT_HANDLER_OK : WT_HANDLER_ERROR;
}

static uint32_t queue_count_adapter(void *const queue_handle)
{
    int32_t ret = osal_queue_messages_waiting((osal_queue_handle_t)queue_handle);
    return (ret >= 0) ? (uint32_t)ret : 0U;
}

static wt_handler_status_t sema_create_adapter(void **const sema_handle)
{
    int32_t ret = osal_sema_init((osal_sema_handle_t *)sema_handle, 0U);
    return (OSAL_SUCCESS == ret) ? WT_HANDLER_OK : WT_HANDLER_ERROR;
}

static wt_handler_status_t sema_delete_adapter(void *const sema_handle)
{
    osal_sema_delete((osal_sema_handle_t)sema_handle);
    return WT_HANDLER_OK;
}

static wt_handler_status_t sema_take_adapter(void *const sema_handle,
                                             uint32_t    timeout)
{
    int32_t ret = osal_sema_take((osal_sema_handle_t)sema_handle,
                                 (osal_tick_type_t)timeout);
    if (OSAL_SUCCESS == ret)
    {
        return WT_HANDLER_OK;
    }
    return (OSAL_SEM_TIMEOUT == ret) ? WT_HANDLER_ERRORTIMEOUT
                                     : WT_HANDLER_ERROR;
}

static wt_handler_status_t sema_give_adapter(void *const sema_handle)
{
    int32_t ret = osal_sema_give((osal_sema_handle_t)sema_handle);
    return (OSAL_SUCCESS == ret) ? WT_HANDLER_OK : WT_HANDLER_ERROR;
}

/* ========================================================================= */
/*  Static interface structs                                                  */
/* ========================================================================= */

static wt_os_delay_interface_t s_os_delay_interface = {
    .pf_os_delay_ms = os_delay_ms_adapter,
};

static wt_os_heap_interface_t s_os_heap_interface = {
    .pf_os_malloc = osal_heap_malloc,
    .pf_os_free   = osal_heap_free,
};

static list_malloc_interface_t s_list_malloc_interface = {
    .pf_list_malloc = osal_heap_malloc,
    .pf_list_free   = osal_heap_free,
};

static wt_os_interface_t s_os_interface = {
    .pf_task_create         = task_create_adapter,
    .pf_task_delete         = task_delete_adapter,
    .pf_os_queue_create     = queue_create_adapter,
    .pf_os_queue_delete     = queue_delete_adapter,
    .pf_os_queue_send       = queue_send_adapter,
    .pf_os_queue_get        = queue_get_adapter,
    .pf_os_queue_count      = queue_count_adapter,
    .pf_os_sema_create      = sema_create_adapter,
    .pf_os_sema_delete      = sema_delete_adapter,
    .pf_os_sema_take        = sema_take_adapter,
    .pf_os_sema_give        = sema_give_adapter,
    .p_os_delay_interface   = &s_os_delay_interface,
    .p_os_heap_interface    = &s_os_heap_interface,
};

/* ========================================================================= */
/*  Exported instances                                                       */
/* ========================================================================= */

/**
 * @brief Assembled input args — all const-pointer fields initialised here.
 *        Passed to wt588_handler_inst() as the second argument.
 */
wt_handler_input_args_t wt588_handler_input_args = {
    .p_os_interface        = &s_os_interface,
    .p_busy_interface      = &wt588_hal_busy_interface,
    .p_gpio_interface      = &wt588_hal_gpio_interface,
    .p_pwm_dma_interface   = &wt588_hal_pwm_dma_interface,
    .list_malloc_interface = &s_list_malloc_interface,
};

//******************************* Functions *********************************//
