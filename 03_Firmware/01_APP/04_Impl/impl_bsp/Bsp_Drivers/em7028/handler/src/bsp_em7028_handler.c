/******************************************************************************
 * @file bsp_em7028_handler.c
 *
 * @par dependencies
 * - bsp_em7028_handler.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief EM7028 PPG handler implementation.
 *
 * Processing flow:
 * 1. em7028_handler_thread allocates the handler / driver / private
 *    state on its own stack (the task lives forever, so the storage
 *    does too) and runs bsp_em7028_handler_inst.
 * 2. Inst creates the cmd and frame queues and instantiates the
 *    underlying EM7028 driver (which auto-inits the chip via the
 *    inst-time pf_init call).
 * 3. The dispatch loop blocks on the cmd queue with a sample-period
 *    timeout when active (forever when idle). On timeout the loop
 *    pulls one frame from the driver and pushes it onto the frame
 *    queue (drop-on-full). On message it dispatches start / stop /
 *    reconfigure / deinit.
 *
 * @version V1.0 2026-04-29
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_em7028_handler.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define HANDLER_NOT_INITED                              false
#define HANDLER_IS_INITED                               true

#define EM7028_HANDLER_CMD_QUEUE_DEPTH                  (4U)
#define EM7028_HANDLER_FRAME_QUEUE_DEPTH                (10U)
#define EM7028_HANDLER_SAMPLE_PERIOD_MS                 (25U)   /* 40 Hz   */
#define EM7028_HANDLER_QUEUE_FOREVER                    (0xFFFFFFFFU)
#define EM7028_HANDLER_QUEUE_NO_WAIT                    (0U)

/* Shorthand for repeatedly-typed instance paths inside helpers. */
#define HANDLER_OS_QUEUE_IF (handler_instance->p_os_interface\
                                             ->p_os_queue_interface)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
/**
 * @brief Private state owned by the handler. Defined here so handler.h
 *        only sees an opaque forward declaration.
 */
struct em7028_handler_private
{
    bool is_init;        /* set true after bsp_em7028_handler_inst OK   */
    bool is_active;      /* set true between START and STOP events      */
    bool exit_request;   /* set true by DEINIT to break the loop        */
};

/**
 * @brief Singleton pointer used by the public submit / get-frame APIs
 *        to locate the running handler instance. The handler thread
 *        publishes itself via __mount_handler after a successful inst.
 */
static bsp_em7028_handler_t *gp_em7028_handler = NULL;

/**
 * @brief Publish or clear the singleton pointer.
 *
 * @param[in]  instance : Handler instance to expose, or NULL on teardown.
 *
 * @return     None.
 * */
static void __mount_handler(bsp_em7028_handler_t *instance)
{
    gp_em7028_handler = instance;
}
//******************************** Variables ********************************//

//******************************** Functions ********************************//
/**
 * @brief      Push a command event onto the running handler's cmd queue.
 *             Used by all public start / stop / reconfigure helpers.
 *
 * @param[in]  p_event : Event to enqueue (deep-copied by the queue impl).
 *
 * @return     EM7028_HANDLER_OK on success;
 *             EM7028_HANDLER_ERRORNOTINIT if the handler thread has not
 *                                          completed inst yet;
 *             EM7028_HANDLER_ERRORRESOURCE on missing queue interface.
 * */
static em7028_handler_status_t handler_send_event(
                                em7028_handler_event_t const * const p_event)
{
    if (NULL == p_event)
    {
        return EM7028_HANDLER_ERRORPARAMETER;
    }
    if ((NULL == gp_em7028_handler)                                       ||
        (NULL == gp_em7028_handler->p_private_data)                       ||
        (!gp_em7028_handler->p_private_data->is_init))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler send event: not inited");
        return EM7028_HANDLER_ERRORNOTINIT;
    }
    if ((NULL == gp_em7028_handler->p_os_interface)                       ||
        (NULL == gp_em7028_handler->p_os_interface->p_os_queue_interface) ||
        (NULL == gp_em7028_handler->p_os_interface
                                  ->p_os_queue_interface->pf_os_queue_put))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler send event: queue interface NULL");
        return EM7028_HANDLER_ERRORRESOURCE;
    }
    if (NULL == gp_em7028_handler->p_cmd_queue)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler send event: cmd queue NULL");
        return EM7028_HANDLER_ERRORRESOURCE;
    }

    return gp_em7028_handler->p_os_interface->p_os_queue_interface
              ->pf_os_queue_put(gp_em7028_handler->p_cmd_queue,
                                (void *)p_event,
                                EM7028_HANDLER_QUEUE_FOREVER);
}

/**
 * @brief      Apply a single command event to the underlying driver.
 *             Mutates handler-private state (is_active / exit_request)
 *             so the dispatch loop reflects the new control intent on
 *             the next iteration.
 *
 * @param[in]  handler_instance : Handler instance owning the driver.
 *
 * @param[in]  event            : Event consumed from the cmd queue.
 *
 * @return     None.
 * */
static void process_event(bsp_em7028_handler_t       * const handler_instance,
                          em7028_handler_event_t const * const         event)
{
    bsp_em7028_driver_t      *drv  = handler_instance->p_em7028_instance;
    em7028_handler_private_t *priv = handler_instance->p_private_data;
    em7028_status_t           dret = EM7028_OK;

    switch (event->event_type)
    {
    case EM7028_HANDLER_EVENT_START:
        dret = drv->pf_start(drv);
        if (EM7028_OK == dret)
        {
            priv->is_active = true;
            DEBUG_OUT(i, EM7028_LOG_TAG, "em7028 handler START");
        }
        else
        {
            DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                      "em7028 handler START failed: %d", (int)dret);
        }
        break;

    case EM7028_HANDLER_EVENT_STOP:
        (void)drv->pf_stop(drv);
        priv->is_active = false;
        DEBUG_OUT(i, EM7028_LOG_TAG, "em7028 handler STOP");
        break;

    case EM7028_HANDLER_EVENT_RECONFIGURE:
        dret = drv->pf_apply_config(drv, &event->cfg);
        if (EM7028_OK != dret)
        {
            DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                      "em7028 handler RECONFIGURE failed: %d", (int)dret);
            break;
        }
        /**
         * apply_config disabled HRS_CFG to settle on a clean state; if
         * we were sampling before the reconfigure, re-enable so the
         * upper layer does not have to chase a START after every cfg.
         **/
        if (priv->is_active)
        {
            (void)drv->pf_start(drv);
        }
        DEBUG_OUT(i, EM7028_LOG_TAG, "em7028 handler RECONFIGURE");
        break;

    case EM7028_HANDLER_EVENT_DEINIT:
        (void)drv->pf_deinit(drv);
        priv->is_active    = false;
        priv->exit_request = true;
        DEBUG_OUT(i, EM7028_LOG_TAG, "em7028 handler DEINIT");
        break;

    default:
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler unsupported event %d",
                  (int)event->event_type);
        break;
    }
}

/**
 * @brief      Internal init: create both queues and instantiate the
 *             underlying EM7028 driver. Driver inst auto-invokes pf_init
 *             so the chip is fully programmed (default 40 Hz HRS1 cfg)
 *             on return -- the dispatch loop only needs to call pf_start
 *             when a START event arrives.
 *
 * @param[in]  handler_instance : Handler instance with mounted ifaces.
 *
 * @return     EM7028_HANDLER_OK on success, error code otherwise.
 * */
static em7028_handler_status_t bsp_em7028_handler_internal_init(
                              bsp_em7028_handler_t * const handler_instance)
{
    em7028_handler_status_t  ret     = EM7028_HANDLER_OK;
    em7028_status_t          drv_ret = EM7028_OK;

    if ((NULL == handler_instance)                                  ||
        (NULL == handler_instance->p_os_interface)                  ||
        (NULL == HANDLER_OS_QUEUE_IF)                               ||
        (NULL == HANDLER_OS_QUEUE_IF->pf_os_queue_create))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler internal init invalid parameter");
        return EM7028_HANDLER_ERRORPARAMETER;
    }

    /* 1. Create the cmd queue (start / stop / reconfigure / deinit). */
    ret = HANDLER_OS_QUEUE_IF->pf_os_queue_create(
                                EM7028_HANDLER_CMD_QUEUE_DEPTH,
                                sizeof(em7028_handler_event_t),
                                &(handler_instance->p_cmd_queue));
    if (EM7028_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler create cmd queue failed: %d", (int)ret);
        return ret;
    }

    /* 2. Create the frame queue (driver -> algorithm consumer).      */
    ret = HANDLER_OS_QUEUE_IF->pf_os_queue_create(
                                EM7028_HANDLER_FRAME_QUEUE_DEPTH,
                                sizeof(em7028_ppg_frame_t),
                                &(handler_instance->p_frame_queue));
    if (EM7028_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler create frame queue failed: %d", (int)ret);
        return ret;
    }

    /* 3. Instantiate the underlying driver -- this also auto-runs
     *    pf_init, programming HRSx_CTRL / LED_CRT etc per default cfg. */
    drv_ret = bsp_em7028_driver_inst(
                            handler_instance->p_em7028_instance,
                            handler_instance->p_iic_interface,
                            handler_instance->p_timebase_interface,
                            handler_instance->p_delay_interface,
                            handler_instance->p_driver_os_delay);
    if (EM7028_OK != drv_ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler driver inst failed: %d", (int)drv_ret);
        return EM7028_HANDLER_ERRORRESOURCE;
    }

    DEBUG_OUT(d, EM7028_LOG_TAG, "em7028 handler internal init success");
    return EM7028_HANDLER_OK;
}

/**
 * @brief      Initialize and instantiate the EM7028 handler.
 *             Mounts injected interfaces, runs internal_init, marks
 *             is_init true.
 *
 * @param[in]  handler_instance : Caller-allocated handler instance.
 *
 * @param[in]  input_args       : Aggregate of injected interfaces.
 *
 * @return     EM7028_HANDLER_OK on success, error code otherwise.
 * */
em7028_handler_status_t bsp_em7028_handler_inst(
                            bsp_em7028_handler_t        *const handler_instance,
                            em7028_handler_input_args_t *const       input_args)
{
    em7028_handler_status_t ret = EM7028_HANDLER_OK;

    DEBUG_OUT(d, EM7028_LOG_TAG, "em7028 handler inst start");

    if ((NULL == handler_instance) || (NULL == input_args))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler inst input error parameter");
        return EM7028_HANDLER_ERRORPARAMETER;
    }

    if ((NULL == handler_instance->p_em7028_instance) ||
        (NULL == handler_instance->p_private_data))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler inst: driver/priv NULL");
        return EM7028_HANDLER_ERRORRESOURCE;
    }

    if (HANDLER_IS_INITED == handler_instance->p_private_data->is_init)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler already inited");
        return EM7028_HANDLER_ERRORRESOURCE;
    }

    /* Mount all injected interfaces in one shot. */
    handler_instance->p_os_interface       = input_args->p_os_interface;
    handler_instance->p_iic_interface      = input_args->p_iic_interface;
    handler_instance->p_timebase_interface = input_args->p_timebase_interface;
    handler_instance->p_delay_interface    = input_args->p_delay_interface;
    handler_instance->p_driver_os_delay    = input_args->p_driver_os_delay;

    if ((NULL == handler_instance->p_os_interface)                       ||
        (NULL == handler_instance->p_iic_interface)                      ||
        (NULL == handler_instance->p_timebase_interface)                 ||
        (NULL == handler_instance->p_delay_interface)                    ||
        (NULL == handler_instance->p_os_interface->p_os_queue_interface))
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler inst: interface NULL");
        return EM7028_HANDLER_ERRORRESOURCE;
    }

    ret = bsp_em7028_handler_internal_init(handler_instance);
    if (EM7028_HANDLER_OK != ret)
    {
        return ret;
    }

    handler_instance->p_private_data->is_init      = HANDLER_IS_INITED;
    handler_instance->p_private_data->is_active    = false;
    handler_instance->p_private_data->exit_request = false;

    DEBUG_OUT(d, EM7028_LOG_TAG, "em7028 handler inst success");
    return EM7028_HANDLER_OK;
}

/**
 * @brief      Submit EM7028_HANDLER_EVENT_START.
 *
 * @return     em7028_handler_status_t code returned by the queue put.
 * */
em7028_handler_status_t bsp_em7028_handler_start(void)
{
    em7028_handler_event_t event = {
        .event_type = EM7028_HANDLER_EVENT_START,
    };
    return handler_send_event(&event);
}

/**
 * @brief      Submit EM7028_HANDLER_EVENT_STOP.
 *
 * @return     em7028_handler_status_t code returned by the queue put.
 * */
em7028_handler_status_t bsp_em7028_handler_stop(void)
{
    em7028_handler_event_t event = {
        .event_type = EM7028_HANDLER_EVENT_STOP,
    };
    return handler_send_event(&event);
}

/**
 * @brief      Submit EM7028_HANDLER_EVENT_RECONFIGURE with a deep copy
 *             of the requested cfg.
 *
 * @param[in]  p_cfg : Configuration to apply (must not be NULL).
 *
 * @return     em7028_handler_status_t code returned by the queue put.
 * */
em7028_handler_status_t bsp_em7028_handler_reconfigure(
                                          em7028_config_t const *const p_cfg)
{
    if (NULL == p_cfg)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028 handler reconfigure NULL cfg");
        return EM7028_HANDLER_ERRORPARAMETER;
    }

    em7028_handler_event_t event = {
        .event_type = EM7028_HANDLER_EVENT_RECONFIGURE,
        .cfg        = *p_cfg,
    };
    return handler_send_event(&event);
}

/**
 * @brief      Pop the next PPG frame from the output queue. Blocks for
 *             up to timeout_ms (0 = non-blocking, 0xFFFFFFFFU = wait
 *             forever).
 *
 * @param[out] p_frame    : Output frame (only written on OK return).
 *
 * @param[in]  timeout_ms : Wait timeout in milliseconds.
 *
 * @return     EM7028_HANDLER_OK on success;
 *             EM7028_HANDLER_ERRORTIMEOUT when no frame arrives in time.
 * */
em7028_handler_status_t bsp_em7028_handler_get_frame(
                                  em7028_ppg_frame_t * const p_frame,
                                  uint32_t                   timeout_ms)
{
    if (NULL == p_frame)
    {
        return EM7028_HANDLER_ERRORPARAMETER;
    }
    if ((NULL == gp_em7028_handler)                                       ||
        (NULL == gp_em7028_handler->p_private_data)                       ||
        (!gp_em7028_handler->p_private_data->is_init))
    {
        return EM7028_HANDLER_ERRORNOTINIT;
    }
    if ((NULL == gp_em7028_handler->p_os_interface)                       ||
        (NULL == gp_em7028_handler->p_os_interface->p_os_queue_interface) ||
        (NULL == gp_em7028_handler->p_os_interface
                                  ->p_os_queue_interface->pf_os_queue_get))
    {
        return EM7028_HANDLER_ERRORRESOURCE;
    }
    if (NULL == gp_em7028_handler->p_frame_queue)
    {
        return EM7028_HANDLER_ERRORRESOURCE;
    }

    return gp_em7028_handler->p_os_interface->p_os_queue_interface
              ->pf_os_queue_get(gp_em7028_handler->p_frame_queue,
                                p_frame,
                                timeout_ms);
}

/**
 * @brief      EM7028 handler thread. Allocates the handler / driver /
 *             private state on the task stack (lifetime = task lifetime),
 *             runs inst, mounts the singleton, and runs the dispatch
 *             loop until DEINIT is received.
 *
 * @param[in]  argument : Pointer to a em7028_handler_input_args_t.
 *
 * @return     None.
 * */
void em7028_handler_thread(void *argument)
{
    DEBUG_OUT(d, EM7028_LOG_TAG, "em7028_handler_thread start");

    em7028_handler_input_args_t   *p_input_arg = NULL;
    em7028_handler_status_t                ret = EM7028_HANDLER_OK;
    em7028_handler_event_t               event = {0};
    em7028_ppg_frame_t                   frame = {0};
    em7028_status_t                       dret = EM7028_OK;

    bsp_em7028_driver_t        driver_instance = {0};
    bsp_em7028_handler_t      handler_instance = {0};
    struct em7028_handler_private private_data = {0};

    if (NULL == argument)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028_handler_thread input error parameter");
        return;
    }
    p_input_arg = (em7028_handler_input_args_t *)argument;

    /* Mount the stack-local driver / private slots into the handler. */
    handler_instance.p_private_data    =          &private_data;
    handler_instance.p_em7028_instance =       &driver_instance;

    ret = bsp_em7028_handler_inst(&handler_instance, p_input_arg);
    if (EM7028_HANDLER_OK != ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028_handler_thread inst failed: %d", (int)ret);
        return;
    }
    __mount_handler(&handler_instance);

    /**
     * Dispatch loop:
     * - Active state: queue_get with EM7028_HANDLER_SAMPLE_PERIOD_MS
     *   timeout. On timeout we fall through to read_frame (the timeout
     *   IS the 40 Hz tick). On message we process_event then still
     *   sample (so an early START or RECONFIGURE arrival does not skip
     *   that interval entirely).
     * - Idle state: queue_get with FOREVER -- the thread is fully
     *   asleep until something interesting happens.
     **/
    for (;;)
    {
        if (private_data.exit_request)
        {
            break;
        }

        const uint32_t wait =
            (private_data.is_active) ? EM7028_HANDLER_SAMPLE_PERIOD_MS
                                     : EM7028_HANDLER_QUEUE_FOREVER;

        const em7028_handler_status_t qret =
            handler_instance.p_os_interface->p_os_queue_interface
                ->pf_os_queue_get(handler_instance.p_cmd_queue,
                                  &event, wait);

        if (EM7028_HANDLER_OK == qret)
        {
            process_event(&handler_instance, &event);
        }
        /* Any other qret (including TIMEOUT) is the 40 Hz tick.        */

        if (private_data.is_active)
        {
            dret = handler_instance.p_em7028_instance->pf_read_frame(
                       handler_instance.p_em7028_instance, &frame);
            if (EM7028_OK == dret)
            {
                /**
                 * Drop the new frame if the algorithm consumer is
                 * behind. Frame queue is the slow side, and dropping
                 * keeps the producer at a clean 40 Hz cadence.
                 **/
                (void)handler_instance.p_os_interface->p_os_queue_interface
                          ->pf_os_queue_put(handler_instance.p_frame_queue,
                                            &frame,
                                            EM7028_HANDLER_QUEUE_NO_WAIT);
            }
            else
            {
                DEBUG_OUT(w, EM7028_ERR_LOG_TAG,
                          "em7028 handler read_frame failed: %d",
                          (int)dret);
            }
        }
    }

    /**
     * Exit path: the singleton is cleared so any subsequent submit
     * call from outside the thread fails cleanly with NOTINIT.
     **/
    __mount_handler(NULL);
    DEBUG_OUT(i, EM7028_LOG_TAG, "em7028_handler_thread exit");
}
//******************************** Functions ********************************//
