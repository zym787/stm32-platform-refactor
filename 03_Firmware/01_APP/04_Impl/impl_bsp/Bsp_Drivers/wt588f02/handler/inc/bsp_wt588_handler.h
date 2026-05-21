/******************************************************************************
 * @file bsp_wt588_handler.h
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Define WT588 handler data structures and OS adaptation interfaces.
 *
 * Processing flow:
 *
 *
 * @version V1.0 2026-4-7
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/
#pragma once
#ifndef __BSP_WT588_HANDLER_H__
#define __BSP_WT588_HANDLER_H__

//******************************** Includes *********************************//
#include "bsp_wt588_driver.h"

#include "osal_task.h"

#include "linklist.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    WT_HANDLER_OK               = 0,       /* Operation successful           */
    WT_HANDLER_ERROR            = 1,       /* General error                  */
    WT_HANDLER_ERRORTIMEOUT     = 2,       /* Timeout error                  */
    WT_HANDLER_ERRORRESOURCE    = 3,       /* Resource unavailable           */
    WT_HANDLER_ERRORPARAMETER   = 4,       /* Invalid parameter              */
    WT_HANDLER_ERRORNOMEMORY    = 5,       /* Out of memory                  */
    WT_HANDLER_ERRORUNSUPPORTED = 6,       /* Unsupported feature            */
    WT_HANDLER_ERRORISR         = 7,       /* ISR context error              */
    WT_HANDLER_RESERVED         = 0xFF,    /* WT588 Handler Reserved         */
} wt_handler_status_t;

typedef struct 
{
    void (*pf_os_delay_ms)(uint32_t ms);
} wt_os_delay_interface_t;

typedef struct
{
    void *(*pf_os_malloc)(size_t size);
    void  (*pf_os_free  )(void  * ptr);
} wt_os_heap_interface_t;

typedef struct
{
    wt_handler_status_t (*pf_task_create    )(void   **   const   task_handle,
                                              void   *             parameters,
                                     void (*)(void   *    const          args), 
                                              char        const         *name, 
                                              uint16_t            stack_depth, 
                                              uint32_t               priority);
    wt_handler_status_t (*pf_task_delete    )(void   *    const   task_handle);

    wt_handler_status_t (*pf_os_queue_create)(void   **   const  queue_handle,
                                              uint32_t    const  queue_length,
                                              uint32_t    const     item_size);
    wt_handler_status_t (*pf_os_queue_delete)(void   *    const  queue_handle);
    wt_handler_status_t (*pf_os_queue_send  )(void   *    const  queue_handle, 
                                              void   *    const          item, 
                                              uint32_t                timeout);
    wt_handler_status_t (*pf_os_queue_get   )(void   *    const  queue_handle, 
                                              void   *    const          item, 
                                              uint32_t                timeout);
    uint32_t            (*pf_os_queue_count )(void   *    const  queue_handle);        

    wt_handler_status_t (*pf_os_sema_create )(void   **   const   sema_handle);
    wt_handler_status_t (*pf_os_sema_delete )(void   *    const   sema_handle);
    wt_handler_status_t (*pf_os_sema_take   )(void   *    const   sema_handle,
                                              uint32_t                timeout);
    wt_handler_status_t (*pf_os_sema_give   )(void   *    const   sema_handle);
    
    wt_os_delay_interface_t * const p_os_delay_interface;
    wt_os_heap_interface_t  * const  p_os_heap_interface;
} wt_os_interface_t;

typedef struct 
{
    wt_os_interface_t       *  const        p_os_interface;
    wt_busy_interface_t     *  const      p_busy_interface;
    wt_gpio_interface_t     *  const      p_gpio_interface;
    wt_pwm_dma_interface_t  *  const   p_pwm_dma_interface;
    list_malloc_interface_t *  const list_malloc_interface;
} wt_handler_input_args_t;

typedef struct 
{
    bsp_wt588_driver_t      *               p_wt588_driver;
    list_handler_t          *          p_play_request_list;

    wt_handler_input_args_t *           handler_input_args;

    void                    *        handler_thread_handle;
    void                    *       executor_thread_handle;
    void                    * busy_detection_thread_handle;

    void                    *              voice_add_queue;
    void                    *               executor_queue;
    void                    *         busy_detection_queue;
    void                    *           voice_finish_queue;

    void                    *            semaphore_handler;
    void                    *               stop_semaphore;
    void                    *           stop_ack_semaphore;
} bsp_wt588_handler_t;

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static inline wt_handler_status_t wt588_status_to_handler_status(
                                             wt588_status_t const wt588Status)
{
    wt_handler_status_t ret = WT_HANDLER_ERROR;

    switch (wt588Status)
    {
        case WT588_OK:
            ret = WT_HANDLER_OK;
            break;
        case WT588_ERROR:
            ret = WT_HANDLER_ERROR;
            break;
        case WT588_ERRORTIMEOUT:
            ret = WT_HANDLER_ERRORTIMEOUT;
            break;
        case WT588_ERRORRESOURCE:
            ret = WT_HANDLER_ERRORRESOURCE;
            break;
        case WT588_ERRORPARAMETER:
            ret = WT_HANDLER_ERRORPARAMETER;
            break;
        case WT588_ERRORNOMEMORY:
            ret = WT_HANDLER_ERRORNOMEMORY;
            break;
        case WT588_ERRORUNSUPPORTED:
            ret = WT_HANDLER_ERRORUNSUPPORTED;
            break;
        case WT588_ERRORISR:
            ret = WT_HANDLER_ERRORISR;
            break;
        default:
            ret = WT_HANDLER_ERROR;
            break;
    }

    return ret;
}
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
wt_handler_status_t wt588_handler_inst(
                            bsp_wt588_handler_t  * const   p_handler_instance,
                        wt_handler_input_args_t  * const p_handler_input_args);

wt_handler_status_t wt588_handler_play_request(uint8_t volume_addr,
                                               uint8_t volume,
                                               uint8_t priority);

wt_handler_status_t wt588_handler_stop(void);
//******************************* Functions *********************************//

#endif /* __BSP_WT588_HANDLER_H__ */
