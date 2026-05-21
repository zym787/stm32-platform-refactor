/******************************************************************************
 * @file bsp_em7028_handler.h
 *
 * @par dependencies
 * - bsp_em7028_driver.h
 *
 * @author Ethan-Hang
 *
 * @brief EM7028 PPG handler interface definition.
 *
 * Processing flow:
 * 1. Handler instantiation with required interfaces and queue creation.
 * 2. Underlying driver instantiation (auto-init via inst).
 * 3. Streaming dispatch in handler_thread:
 *      block on cmd queue with sample-period timeout
 *        - timeout fired and is_active=true -> read PPG frame, push to
 *          frame queue (drop on full)
 *        - cmd received                     -> dispatch start/stop/
 *          reconfigure/deinit
 *      The same loop handles control and pacing -- no separate timer.
 *
 * @version V1.0 2026-04-29
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_EM7028_HANDLER_H__
#define __BSP_EM7028_HANDLER_H__

//******************************** Includes *********************************//
#include "bsp_em7028_driver.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    EM7028_HANDLER_OK               = 0,    /* Operation successful          */
    EM7028_HANDLER_ERROR            = 1,    /* General error                 */
    EM7028_HANDLER_ERRORTIMEOUT     = 2,    /* Timeout (queue empty)         */
    EM7028_HANDLER_ERRORRESOURCE    = 3,    /* Resource unavailable          */
    EM7028_HANDLER_ERRORPARAMETER   = 4,    /* Invalid parameter             */
    EM7028_HANDLER_ERRORNOMEMORY    = 5,    /* Out of memory                 */
    EM7028_HANDLER_ERRORUNSUPPORTED = 6,    /* Unsupported feature           */
    EM7028_HANDLER_ERRORISR         = 7,    /* ISR context error             */
    EM7028_HANDLER_ERRORNOTINIT     = 8,    /* API used before inst          */
    EM7028_HANDLER_RESERVED         = 0xFF, /* Reserved                      */
} em7028_handler_status_t;

/* Control commands consumed by the handler thread via the cmd queue. */
typedef enum
{
    EM7028_HANDLER_EVENT_START       = 0,   /* Begin periodic sampling      */
    EM7028_HANDLER_EVENT_STOP        = 1,   /* Halt sampling                */
    EM7028_HANDLER_EVENT_RECONFIGURE = 2,   /* Apply new cfg + resume       */
    EM7028_HANDLER_EVENT_DEINIT      = 3,   /* Tear down driver, exit loop  */
} em7028_handler_event_type_t;

/* Cmd queue payload. cfg is only consulted by the RECONFIGURE branch. */
typedef struct
{
    em7028_handler_event_type_t event_type;
    em7028_config_t             cfg;
} em7028_handler_event_t;

/**
 * @brief OS queue service interface used by the handler.
 *
 * timeout fields use the project's OSAL convention: 0 = non-blocking,
 * 0xFFFFFFFFU (OSAL_MAX_DELAY) = wait forever, otherwise milliseconds.
 */
typedef struct
{
    em7028_handler_status_t (*pf_os_queue_create)(
                                              uint32_t const      item_num,
                                              uint32_t const     item_size,
                                              void  ** const  queue_handler);
    em7028_handler_status_t (*pf_os_queue_put   )(
                                              void  * const   queue_handler,
                                              void  * const            item,
                                              uint32_t              timeout);
    em7028_handler_status_t (*pf_os_queue_get   )(
                                              void  * const   queue_handler,
                                              void  * const             msg,
                                              uint32_t              timeout);
    em7028_handler_status_t (*pf_os_queue_delete)(
                                              void  * const   queue_handler);
} em7028_handler_os_queue_t;

/* Aggregate of OS-side services consumed by the handler itself. */
typedef struct
{
    em7028_handler_os_queue_t   *  p_os_queue_interface;
} em7028_handler_os_interface_t;

/* Forward declaration of private state owned by the handler. */
typedef struct em7028_handler_private em7028_handler_private_t;

/**
 * @brief Argument structure passed to the handler thread on startup.
 *
 * The integration layer fills this struct with the OS queue interface
 * (consumed by the handler) plus the four interfaces the underlying
 * driver needs (iic / timebase / delay / os_delay) and hands it off via
 * the FreeRTOS task argument pointer.
 */
typedef struct
{
    em7028_handler_os_interface_t *           p_os_interface;
    em7028_iic_driver_interface_t *          p_iic_interface;
    em7028_timebase_interface_t   *     p_timebase_interface;
    em7028_delay_interface_t      *        p_delay_interface;
    em7028_os_delay_interface_t   *        p_driver_os_delay;
} em7028_handler_input_args_t;

/* Handler instance class (stack-allocated inside the handler thread). */
typedef struct bsp_em7028_handler
{
    /*                  Private data                       */
    em7028_handler_private_t        *           p_private_data;

    /*           Interface from OS layer                   */
    em7028_handler_os_interface_t   *           p_os_interface;

    /*    Interfaces forwarded down to the driver layer    */
    em7028_iic_driver_interface_t   *          p_iic_interface;
    em7028_timebase_interface_t     *     p_timebase_interface;
    em7028_delay_interface_t        *        p_delay_interface;
    em7028_os_delay_interface_t     *        p_driver_os_delay;

    /*    Instance of underlying EM7028 driver             */
    bsp_em7028_driver_t             *        p_em7028_instance;

    /*    Queue handles owned by this handler              */
    void                            *           p_cmd_queue;
    void                            *         p_frame_queue;
} bsp_em7028_handler_t;
//******************************** Defines **********************************//

//******************************* Functions *********************************//
/**
 * @brief Initialize and instantiate the EM7028 handler.
 *        Mounts injected interfaces, creates the cmd and frame queues,
 *        and instantiates the underlying driver (which auto-inits the
 *        chip per bsp_em7028_driver_inst). Does NOT start sampling --
 *        the caller must submit EM7028_HANDLER_EVENT_START afterwards.
 *
 * @param[in]  handler_instance : Caller-allocated handler instance.
 * @param[in]  input_args       : Aggregate of injected interfaces.
 *
 * @param[out] handler_instance : Populated with mounted interfaces,
 *                                created queues, and a running driver.
 *
 * @return EM7028_HANDLER_OK on success, error code on failure.
 * */
em7028_handler_status_t bsp_em7028_handler_inst(
                            bsp_em7028_handler_t        *const handler_instance,
                            em7028_handler_input_args_t *const       input_args);

/**
 * @brief Submit EM7028_HANDLER_EVENT_START -- request the handler to
 *        begin periodic sampling at 40 Hz.
 *
 * @return EM7028_HANDLER_OK if the event was queued.
 * */
em7028_handler_status_t bsp_em7028_handler_start(void);

/**
 * @brief Submit EM7028_HANDLER_EVENT_STOP -- request the handler to
 *        halt sampling. Cached config is preserved.
 *
 * @return EM7028_HANDLER_OK if the event was queued.
 * */
em7028_handler_status_t bsp_em7028_handler_stop(void);

/**
 * @brief Submit EM7028_HANDLER_EVENT_RECONFIGURE -- apply a new cfg
 *        through the driver and (if active) resume sampling.
 *
 * @param[in] p_cfg : Configuration to apply (deep-copied into the event).
 *
 * @return EM7028_HANDLER_OK if the event was queued.
 * */
em7028_handler_status_t bsp_em7028_handler_reconfigure(
                                          em7028_config_t const *const p_cfg);

/**
 * @brief Pop the next PPG frame from the output queue.
 *        Blocks for up to timeout_ms (0 = non-blocking, 0xFFFFFFFFU =
 *        wait forever). Use this from the heart-rate algorithm task.
 *
 * @param[out] p_frame    : Output frame.
 * @param[in]  timeout_ms : Wait timeout in milliseconds.
 *
 * @return EM7028_HANDLER_OK on success, EM7028_HANDLER_ERRORTIMEOUT
 *         when no frame arrives within the timeout.
 * */
em7028_handler_status_t bsp_em7028_handler_get_frame(
                                  em7028_ppg_frame_t * const p_frame,
                                  uint32_t                   timeout_ms);

/**
 * @brief EM7028 handler thread entry point. Allocates the handler /
 *        driver / private-state structs on the task stack, runs inst,
 *        mounts the singleton, and runs the dispatch loop forever.
 *
 * @param[in] argument : Pointer to a em7028_handler_input_args_t.
 * */
void em7028_handler_thread(void *argument);
//******************************* Functions *********************************//

#endif /* __BSP_EM7028_HANDLER_H__ */
