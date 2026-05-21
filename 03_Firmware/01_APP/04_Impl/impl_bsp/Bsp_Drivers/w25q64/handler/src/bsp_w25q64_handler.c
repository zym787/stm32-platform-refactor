/******************************************************************************
 * @file bsp_w25q64_handler.c
 *
 * @par dependencies
 * - bsp_w25q64_handler.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief W25Q64 flash handler implementation.
 *
 * Processing flow:
 * 1. Handler instantiation with interface mounting and driver init.
 * 2. Event queue creation and underlying W25Q64 driver instantiation.
 * 3. Event-driven dispatch in handler_thread:
 *      pop event -> dispatch by event_type -> fill status -> callback.
 *
 * @version V1.0 2026-04-27
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_w25q64_handler.h"

#include "osal_common_types.h"

#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define FLASH_NOT_INITIATED     false   /*  Handler not initialized flag    */
#define FLASH_INITIATED         true    /*  Handler initialized flag        */

#define HANDLER_DRIVER_INSTANCE  (handler_instance->p_w25q64_instance)
#define HANDLER_OS_INTERFACE     (handler_instance->p_os_interface)
#define HANDLER_OS_QUEUE_IF      (handler_instance->p_os_interface\
                                                  ->p_os_queue_interface)
#define HANDLER_SPI_INTERFACE    (handler_instance->p_spi_interface)
#define HANDLER_TIMEBASE         (handler_instance->p_timebase_interface)
#define HANDLER_OS_DELAY         (handler_instance->p_w25q64_os_delay)

#define OS_QUEUE_CREATE          (handler_instance->p_os_interface\
                                                  ->p_os_queue_interface\
                                                  ->pf_os_queue_create)

#define FLASH_QUEUE_DEPTH        (10U)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
/**
 * @brief Global pointer used by handler_flash_event_send() to locate
 *        the running handler instance.
 */
static bsp_w25q64_handler_t *gp_flash_instance = NULL;

/**
 * @brief Private state owned by the handler.
 */
struct flash_handler_private_data
{
    bool is_initated;             /* Initialization flag                 */
};

/**
 * @brief Mount the handler instance to the global pointer so the
 *        public submit API can find it.
 */
static void __mount_handler(bsp_w25q64_handler_t *instance)
{
    gp_flash_instance = instance;
}
//******************************** Variables ********************************//

//******************************** Functions ********************************//

/**
 * @brief Dispatch a single flash event onto the underlying driver.
 *
 * @param[in] handler_instance : Pointer to handler instance.
 * @param[in] event            : Event describing the requested operation.
 *
 * @return flash_handler_status_t: FLASH_HANLDER_OK on success,
 *                                 error code on failure.
 * */
static flash_handler_status_t process_flash_event(
                          bsp_w25q64_handler_t * const handler_instance,
                          flash_event_t        * const            event)
{
    flash_handler_status_t   ret = FLASH_HANLDER_OK;
    w25q64_status_t      drv_ret =          W25Q64_OK;
    bsp_w25q64_driver_t   *  drv = HANDLER_DRIVER_INSTANCE;

    if ((NULL == drv) || (NULL == event))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "process_flash_event invalid parameter");
        return FLASH_HANLDER_ERRORPARAMETER;
    }

    switch (event->event_type)
    {
    case FLASH_HANDLER_EVENT_READ:
        if ((NULL == event->data) || (0U == event->size))
        {
            DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                      "flash event READ invalid parameter");
            return FLASH_HANLDER_ERRORPARAMETER;
        }
        drv_ret = drv->pf_w25q64_read_data(drv, event->addr,
                                           event->data, event->size);
        break;

    case FLASH_HANDLER_EVENT_WRITE:
        if ((NULL == event->data) || (0U == event->size))
        {
            DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                      "flash event WRITE invalid parameter");
            return FLASH_HANLDER_ERRORPARAMETER;
        }
        drv_ret = drv->pf_w25q64_write_data_erase(drv, event->addr,
                                                  event->data, event->size);
        break;

    case FLASH_HANDLER_EVENT_WRITE_NOERASE:
        if ((NULL == event->data) || (0U == event->size))
        {
            DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                      "flash event WRITE_NOERASE invalid parameter");
            return FLASH_HANLDER_ERRORPARAMETER;
        }
        drv_ret = drv->pf_w25q64_write_data_noerase(drv, event->addr,
                                                    event->data, event->size);
        break;

    case FLASH_HANDLER_EVENT_ERASE_CHIP:
        drv_ret = drv->pf_w25q64_erase_chip(drv);
        break;

    case FLASH_HANDLER_EVENT_ERASE_SECTOR:
        drv_ret = drv->pf_w25q64_erase_sector(drv, event->addr);
        break;

    case FLASH_HANDLER_EVENT_WAKEUP:
        drv_ret = drv->pf_w25q64_wakeup(drv);
        break;

    case FLASH_HANDLER_EVENT_SLEEP:
        drv_ret = drv->pf_w25q64_sleep(drv);
        break;

    case FLASH_HANDLER_EVENT_TEST:
        if ((NULL == event->data) || (event->size < 2U))
        {
            DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                      "flash event TEST invalid parameter");
            return FLASH_HANLDER_ERRORPARAMETER;
        }
        drv_ret = drv->pf_w25q64_read_id(drv, event->data, event->size);
        break;

    default:
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "process_flash_event unsupported event type %d",
                  event->event_type);
        return FLASH_HANLDER_ERRORUNSUPPORTED;
    }

    /* Status enums share values 0..7 with w25q64_status_t. */
    ret = (flash_handler_status_t)drv_ret;
    return ret;
}

/**
 * @brief Internal init helper: create the event queue and instantiate
 *        the underlying W25Q64 driver.
 *
 * @param[in] handler_instance : Handler instance with mounted interfaces.
 *
 * @param[out] handler_instance : Handler with created queue and running
 *                                driver instance.
 *
 * @return flash_handler_status_t: FLASH_HANLDER_OK on success,
 *                                 error code on failure.
 * */
static flash_handler_status_t bsp_w25q64_handler_internal_init(
                              bsp_w25q64_handler_t * const handler_instance)
{
    flash_handler_status_t  ret = FLASH_HANLDER_OK;
    w25q64_status_t   drv_ret   =          W25Q64_OK;

    DEBUG_OUT(d, W25Q64_LOG_TAG, "bsp_w25q64_handler_internal_init start");

    if (NULL == handler_instance)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "bsp_w25q64_handler_internal_init invalid parameter");
        return FLASH_HANLDER_ERRORPARAMETER;
    }

    /***************** 1.Create OS Queue *****************/
    ret = OS_QUEUE_CREATE(FLASH_QUEUE_DEPTH, sizeof(flash_event_t),
                          &(handler_instance->p_event_queue_handler));
    if (FLASH_HANLDER_OK != ret)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "bsp_w25q64_handler_internal_init create queue failed");
        return ret;
    }

    /************* 2.Instantiate W25Q64 Driver ***********/
    drv_ret = w25q64_driver_inst(HANDLER_DRIVER_INSTANCE,
                                 HANDLER_SPI_INTERFACE,
                                 HANDLER_TIMEBASE,
                                 HANDLER_OS_DELAY);
    if (W25Q64_OK != drv_ret)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "bsp_w25q64_handler_internal_init driver inst failed");
        return FLASH_HANLDER_ERRORRESOURCE;
    }

    /************** 3.Hardware Init Sequence *************/
    drv_ret = HANDLER_DRIVER_INSTANCE->pf_w25q64_init(HANDLER_DRIVER_INSTANCE);
    if (W25Q64_OK != drv_ret)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "bsp_w25q64_handler_internal_init w25q64_init failed");
        return FLASH_HANLDER_ERRORRESOURCE;
    }

    DEBUG_OUT(d, W25Q64_LOG_TAG, "bsp_w25q64_handler_internal_init success");
    return ret;
}

/**
 * @brief Initialize and instantiate the W25Q64 handler.
 *
 * @param[in] handler_instance : Handler instance to initialize.
 * @param[in] input_args       : Aggregate of required external interfaces.
 *
 * @param[out] handler_instance : Initialized handler with mounted
 *                                interfaces, created queue and running
 *                                underlying driver.
 *
 * @return flash_handler_status_t: FLASH_HANLDER_OK on success,
 *                                 error code on failure.
 * */
flash_handler_status_t bsp_w25q64_handler_inst(
                          bsp_w25q64_handler_t * const handler_instance,
                          flash_input_args_t   * const       input_args)
{
    flash_handler_status_t ret = FLASH_HANLDER_OK;

    DEBUG_OUT(d, W25Q64_LOG_TAG, "bsp_w25q64_handler_init start");

    /************ 1.Checking input parameters ************/
    if ((NULL == handler_instance) || (NULL == input_args))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "bsp_w25q64_handler_init invalid parameter");
        return FLASH_HANLDER_ERRORPARAMETER;
    }

    /************* 2.Checking the Resources **************/
    if (NULL == handler_instance->p_w25q64_instance)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "bsp_w25q64_handler_init: driver instance is NULL");
        return FLASH_HANLDER_ERRORRESOURCE;
    }

    if (NULL == handler_instance->p_private_data)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "bsp_w25q64_handler_init: private data is NULL");
        return FLASH_HANLDER_ERRORRESOURCE;
    }

    if (FLASH_INITIATED == handler_instance->p_private_data->is_initated)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "bsp_w25q64_handler_init: already initialized");
        return FLASH_HANLDER_ERRORRESOURCE;
    }

    /******** 3.Mount and Checking the Interfaces ********/
    HANDLER_OS_INTERFACE  =                 input_args->p_os_interface;
    HANDLER_SPI_INTERFACE =                input_args->p_spi_interface;
    HANDLER_TIMEBASE      =           input_args->p_timebase_interface;
    HANDLER_OS_DELAY      =              input_args->p_w25q64_os_delay;

    if ((NULL == HANDLER_OS_INTERFACE)                         ||
        (NULL == HANDLER_SPI_INTERFACE)                        ||
        (NULL == HANDLER_TIMEBASE)                             ||
        (NULL == HANDLER_OS_DELAY)                             ||
        (NULL == HANDLER_OS_INTERFACE->p_os_queue_interface)   ||
        (NULL == HANDLER_OS_INTERFACE->p_os_delay_interface))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "bsp_w25q64_handler_init interface error");
        return FLASH_HANLDER_ERRORRESOURCE;
    }

    /************** 4.Internal initialization *************/
    ret = bsp_w25q64_handler_internal_init(handler_instance);
    if (FLASH_HANLDER_OK != ret)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "bsp_w25q64_handler_init internal init failed");
        return ret;
    }

    /******************* 5.Mark inited ********************/
    handler_instance->p_private_data->is_initated = FLASH_INITIATED;

    DEBUG_OUT(d, W25Q64_LOG_TAG, "bsp_w25q64_handler_init success");
    return ret;
}

/**
 * @brief Submit a flash event to the handler queue.
 *
 * @param[in] event : Pointer to the flash event structure.
 *
 * @return flash_handler_status_t: FLASH_HANLDER_OK on success,
 *                                 error code on failure.
 * */
flash_handler_status_t handler_flash_event_send(flash_event_t * const event)
{
    flash_handler_status_t ret = FLASH_HANLDER_OK;

    /************ 1.Checking input parameters ************/
    if (NULL == event)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "handler_flash_event_send: event is NULL");
        return FLASH_HANLDER_ERRORPARAMETER;
    }

    if (NULL == gp_flash_instance)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "handler_flash_event_send: instance is NULL");
        return FLASH_HANLDER_ERRORRESOURCE;
    }

    if (NULL == gp_flash_instance->p_private_data)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "handler_flash_event_send: private data is NULL");
        return FLASH_HANLDER_ERRORRESOURCE;
    }

    if (FLASH_NOT_INITIATED == gp_flash_instance->p_private_data->is_initated)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "handler_flash_event_send: handler not inited");
        return FLASH_HANLDER_ERRORRESOURCE;
    }

    if ((NULL == gp_flash_instance->p_os_interface)                       ||
        (NULL == gp_flash_instance->p_os_interface->p_os_queue_interface) ||
        (NULL == gp_flash_instance->p_os_interface
                                  ->p_os_queue_interface->pf_os_queue_put))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "handler_flash_event_send: queue interface NULL");
        return FLASH_HANLDER_ERRORRESOURCE;
    }

    if (NULL == gp_flash_instance->p_event_queue_handler)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "handler_flash_event_send: queue handle is NULL");
        return FLASH_HANLDER_ERRORRESOURCE;
    }

    /*********** 2.Put Event into Handler Queue **********/
    ret = gp_flash_instance->p_os_interface->p_os_queue_interface
              ->pf_os_queue_put(gp_flash_instance->p_event_queue_handler,
                                event,
                                OSAL_MAX_DELAY);
    if (FLASH_HANLDER_OK != ret)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "handler_flash_event_send put event failed");
        return ret;
    }

    return ret;
}

/**
 * @brief W25Q64 handler thread.
 *
 * Builds the handler instance on the task stack, instantiates the
 * underlying driver, then runs the queue dispatch loop forever.
 *
 * @param[in] argument : Pointer to a flash_input_args_t instance.
 * */
void flash_handler_thread(void *argument)
{
    DEBUG_OUT(d, W25Q64_LOG_TAG, "flash_handler_thread start");

    flash_input_args_t           *p_input_arg = NULL;
    flash_handler_status_t                ret = FLASH_HANLDER_OK;
    flash_event_t                                          event;

    bsp_w25q64_driver_t          driver_instance = {0};
    bsp_w25q64_handler_t        handler_instance = {0};
    flash_handler_private_data_t    private_data = {0};

    if (NULL == argument)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "flash_handler_thread input error parameter");
        return;
    }
    p_input_arg = (flash_input_args_t *)argument;

    /* Mount stack-allocated driver and private data into the handler. */
    handler_instance.p_private_data    =          &private_data;
    handler_instance.p_w25q64_instance =       &driver_instance;

    ret = bsp_w25q64_handler_inst(&handler_instance, p_input_arg);
    if (FLASH_HANLDER_OK == ret)
    {
        __mount_handler(&handler_instance);
    }
    else
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "flash_handler_thread handler init failed");
        return;
    }

    /* Infinite event-dispatch loop. */
    for (;;)
    {
        ret = handler_instance.p_os_interface->p_os_queue_interface
                  ->pf_os_queue_get(handler_instance.p_event_queue_handler,
                                    (void *)&event,
                                    OSAL_MAX_DELAY);
        if (FLASH_HANLDER_OK != ret)
        {
            DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                      "flash_handler_thread get event failed");
            continue;
        }

        event.status = process_flash_event(&handler_instance, &event);

        if (NULL != event.pf_callback)
        {
            event.pf_callback(&event);
        }
    }
}

//******************************** Functions ********************************//
