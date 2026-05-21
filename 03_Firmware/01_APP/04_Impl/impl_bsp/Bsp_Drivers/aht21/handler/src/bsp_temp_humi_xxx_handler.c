/******************************************************************************
 * @file bsp_temp_humi_xxx_handler.c
 *
 * @par dependencies:
 *  - bsp_temp_humi_xxx_handler.h
 *  - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief Temperature and humidity handler implementation
 *
 * Processing flow:
 * 1. Handler instantiation with interface mounting
 * 2. Event queue creation and AHT21 driver initialization
 * 3. Event processing with temperature/humidity reading
 * 4. Callback execution for data delivery
 *
 * @version V1.0 2025-11-28
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_temp_humi_xxx_handler.h"
#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define TEMP_HUMI_NOT_INITIATED     false  /*  Handler not initialized flag  */
#define TEMP_HUMI_INITIATED         true   /*  Handler initialized flag      */

#define HANDLER_AHT21_INSTANCE     (        handler_instance->p_aht21_instance)
#define HANDLER_IIC_DRIVER         (  handler_instance->p_iic_driver_interface)
#define HANDLER_OS_QUEUE           (    handler_instance->p_os_queue_interface)
#define HANDLER_TIMEBASE           (    handler_instance->p_timebase_interface)
#define HANDLER_YIELD              (       handler_instance->p_yield_interface)

#define OS_QUEUE_CREATE            ( handler_instance->p_os_queue_interface->\
                                                            pf_os_queue_create)

#define QUEUE_MAX_DELAY            (0xFFFFFFFFU) /* Maximum queue delay time */                                                            
//******************************** Defines **********************************//

//******************************** Variables ********************************//
/**
 * @brief Global pointer to temperature and humidity handler instance
 *        Flag to indicate if the handler has been initialized
 */
static bsp_temp_humi_xxx_handler_t *gp_temp_humi_instance = NULL;

/**
 * @brief Private data structure to manage handler state
 */
typedef struct temp_humi_handler_private_data
{
    bool is_initated;             /* Initialization flag */
} temp_humi_handler_private_data_t;

/**
 * @brief Mount handler instance to global pointer
 *
 * @param[in] instance: Pointer to handler instance to mount
 */
void __mount_handler(bsp_temp_humi_xxx_handler_t *instance)
{
    gp_temp_humi_instance = instance;
}
//******************************** Variables ********************************//

//******************************** Functions ********************************//
/**
 * @brief Get temperature and humidity data based on event type
 *
 * @param[in] event: Temperature and humidity event structure
 * @param[in] handler_instance: Pointer to handler instance
 * @param[in] temp: Pointer to store temperature value
 * @param[in] humi: Pointer to store humidity value
 *
 * @param[out] temp: Temperature value in Celsius
 * @param[out] humi: Humidity value in percentage
 *
 * @return temp_humi_status_t: Operation status (TEMP_HUMI_OK on success,
 *                             error code on failure)
 *
 * */
temp_humi_status_t get_temp_humi(
                        temp_humi_xxx_event_t         * const            event,
                        bsp_temp_humi_xxx_handler_t   * const handler_instance,
                        float                         * const             temp,
                        float                         * const             humi
                                )
{
    temp_humi_status_t ret = TEMP_HUMI_OK;
    /************ 1.Checking input parameters ************/  
    if ( NULL == handler_instance    ||
         NULL == temp                ||
         NULL == humi                ||
         NULL == event
       )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                    "get_temp_humi input error parameter");
        ret = TEMP_HUMI_ERRORPARAMETER;
        return ret;
    }

    uint32_t tim = HANDLER_TIMEBASE->pf_get_tick_count_ms();
    switch (event->event_type)
    {
    case TEMP_HUMI_EVENT_TEMP:
        if ( ( tim - handler_instance->last_temp_tick ) > event->lifetime    ||
             ( 0 ==  handler_instance->last_temp_tick ) // first reading
           )
        {
            ret = (temp_humi_status_t)HANDLER_AHT21_INSTANCE->pf_read_temp(
                                      HANDLER_AHT21_INSTANCE,
                                      temp);
            if (TEMP_HUMI_OK != ret)
            {
                DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                                    "get_temp_humi read temp failed");
                *temp = 0.0f;
                return ret;
            }
            handler_instance->last_temp_tick = tim;
        }
        break;
    case TEMP_HUMI_EVENT_HUMI:
        if ( ( tim - handler_instance->last_humi_tick ) > event->lifetime    ||
             ( 0  == handler_instance->last_humi_tick ) // first reading
           )
        {
            ret = (temp_humi_status_t)HANDLER_AHT21_INSTANCE->pf_read_humi(
                                      HANDLER_AHT21_INSTANCE,
                                      humi);
            if (TEMP_HUMI_OK != ret)
            {
                DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                                    "get_temp_humi read humi failed");
                *humi = 0.0f;
                return ret;
            }
            handler_instance->last_humi_tick = tim;
        }
        break;
    case TEMP_HUMI_EVENT_BOTH:
        // Check if either temperature or humidity data needs update
        if (
            ( ( tim - handler_instance->last_temp_tick ) > event->lifetime   ||
              ( tim - handler_instance->last_humi_tick ) > event->lifetime ) ||
              ( 0 == handler_instance->last_humi_tick                        && 
                0 == handler_instance->last_temp_tick ) // first reading
           )
        {
            // Read both temperature and humidity in one operation
            ret = (temp_humi_status_t)HANDLER_AHT21_INSTANCE->pf_read_temp_humi(
                                      HANDLER_AHT21_INSTANCE,
                                      temp,
                                      humi);
            if (TEMP_HUMI_OK != ret)
            {
                DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                                    "get_temp_humi read temp humi failed");
                *temp = *humi = 0.0f;
                return ret;
            }
            // Update both timestamps to ensure data consistency
            handler_instance->last_temp_tick = tim;
            handler_instance->last_humi_tick = tim;
            DEBUG_OUT(i, TEMP_HUMI_LOG_TAG, "New Data");
        }
        else
        {
            DEBUG_OUT(i, TEMP_HUMI_LOG_TAG, "Old Data");
        }
        break;
    default:
        *temp =            0.0f;
        *humi =            0.0f;
         ret  = TEMP_HUMI_ERROR;
        break;
    }

    return ret;
}
/**
 * @brief Initialize temperature and humidity handler internal components
 *
 * @param[in] handler_instance: Pointer to handler instance to initialize
 *
 * @param[out] handler_instance: Handler with initialized event queue and
 *                               AHT21 driver
 *
 * @return temp_humi_status_t: Operation status (TEMP_HUMI_OK on success,
 *                             error code on failure)
 *
 * */
static temp_humi_status_t bsp_temp_xxx_handler_init(
                           bsp_temp_humi_xxx_handler_t * const handler_instance
                                                   )
{
    DEBUG_OUT(d, TEMP_HUMI_LOG_TAG, "bsp_temp_xxx_handler_init start");
    temp_humi_status_t   ret = TEMP_HUMI_OK;
    aht21_status_t aht21_ret =     AHT21_OK;

    /************ 1.Checking input parameters ************/  
    if ( NULL == handler_instance )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                            "bsp_temp_xxx_handler_init input error parameter");
        ret = TEMP_HUMI_ERRORPARAMETER;
        return ret;
    }
    
    /***************** 2.Create OS Queue *****************/
    ret = OS_QUEUE_CREATE(10, sizeof(temp_humi_xxx_event_t),
                    &(handler_instance->event_queue_handle));
    if ( TEMP_HUMI_OK != ret )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                           "bsp_temp_xxx_handler_init create os queue failed");
        return ret;
    }
    DEBUG_OUT(d, TEMP_HUMI_LOG_TAG, "bsp_temp_xxx_handler_init success");
    /************* 3.Initialize AHT21 Driver *************/
    aht21_ret = aht21_inst(
                HANDLER_AHT21_INSTANCE,
                    HANDLER_IIC_DRIVER,
                         HANDLER_YIELD,
                      HANDLER_TIMEBASE
                         );
    if ( AHT21_OK != aht21_ret )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                                "bsp_temp_xxx_handler_init aht21 inst failed");
        ret = TEMP_HUMI_ERRORRESOURCE;
        return ret;
    }
    DEBUG_OUT(d, TEMP_HUMI_LOG_TAG, "bsp_temp_xxx_handler_init success");
    return ret;
}
/**
 * @brief Initialize and instantiate temperature and humidity handler
 *
 * @param[in] handler_instance: Pointer to handler instance to initialize
 * @param[in] input_argument: Pointer to input argument structure with
 *                            required interfaces
 *
 * @param[out] handler_instance: Initialized handler with mounted interfaces
 *                               and set initialization flag
 *
 * @return temp_humi_status_t: Operation status (TEMP_HUMI_OK on success,
 *                             error code on failure)
 *
 * */
temp_humi_status_t bsp_temp_humi_xxx_handler_inst(
                        bsp_temp_humi_xxx_handler_t   * const handler_instance,
                        temp_humi_handler_input_arg_t * const   input_argument
                                                 )
{
    DEBUG_OUT(d, TEMP_HUMI_LOG_TAG, "bsp_temp_humi_xxx_handler start");
    temp_humi_status_t ret = TEMP_HUMI_OK;
    /************ 1.Checking input parameters ************/ 
    if ( 
         NULL == handler_instance                        ||
         NULL == input_argument                          
       )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                            "bsp_temp_humi_xxx_handler input error parameter");
        ret = TEMP_HUMI_ERRORPARAMETER;
        return ret;
    }                                                

    /************* 2.Checking the Resources **************/
    if ( NULL == handler_instance->p_aht21_instance )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                          "bsp_temp_humi_xxx_handler: aht21 instance is NULL");
        ret = TEMP_HUMI_ERRORRESOURCE;
        return ret;
    }

    if ( NULL == handler_instance->p_private_data )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                            "bsp_temp_humi_xxx_handler: private data is NULL");
        ret = TEMP_HUMI_ERRORRESOURCE;
        return ret;
    }

    if (TEMP_HUMI_INITIATED == handler_instance->p_private_data->is_initated)
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                     "bsp_temp_humi_xxx_handler instance already initialized");
        ret = TEMP_HUMI_ERRORRESOURCE;
        return ret;
    }

    /******** 3.Mount and Checking the Interfaces ********/
    // 3.1 mount external interfaces
    HANDLER_IIC_DRIVER         =        input_argument->p_iic_driver_interface;
    HANDLER_OS_QUEUE           =          input_argument->p_os_queue_interface;
    HANDLER_TIMEBASE           =          input_argument->p_timebase_interface;
    HANDLER_YIELD              =             input_argument->p_yield_interface;
    if (
        NULL == HANDLER_IIC_DRIVER                       ||
        NULL == HANDLER_OS_QUEUE                         ||
        NULL == HANDLER_TIMEBASE                         ||
        NULL == HANDLER_YIELD
       )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                                  "bsp_temp_humi_xxx_handler interface error");
        ret = TEMP_HUMI_ERRORRESOURCE;
        return ret;
    }

    ret = bsp_temp_xxx_handler_init(handler_instance);
    if (TEMP_HUMI_OK != ret)
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                                      "bsp_temp_humi_xxx_handler init failed");
        return ret;
    }

    // 4.Set initialization flag
    handler_instance->p_private_data->is_initated = TEMP_HUMI_INITIATED;

    return ret;
}

/**
 * @brief Read temperature and humidity data by putting event into handler queue
 *
 * @param[in] event: Pointer to temperature and humidity event structure
 *
 * @param[out] event: Event queued for processing by handler thread
 *
 * @return temp_humi_status_t: Operation status (TEMP_HUMI_OK on success,
 *                             error code on failure)
 *
 * */
temp_humi_status_t bsp_temp_humi_xxx_read(
                        temp_humi_xxx_event_t   * const            event
                                         )
{
    DEBUG_OUT(d, TEMP_HUMI_LOG_TAG, "bsp_temp_humi_xxx_read start");
    temp_humi_status_t ret = TEMP_HUMI_OK;

    /************ 1.Checking input parameters ************/ 
    if ( NULL == gp_temp_humi_instance ) 
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                                   "bsp_temp_humi_xxx_read: instance is NULL");
        return TEMP_HUMI_ERRORRESOURCE;
    }
    if ( NULL == gp_temp_humi_instance->p_private_data ) 
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                               "bsp_temp_humi_xxx_read: private data is NULL");
        return TEMP_HUMI_ERRORRESOURCE;
    }
    if ( TEMP_HUMI_NOT_INITIATED ==
            gp_temp_humi_instance->p_private_data->is_initated )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                                  "bsp_temp_humi_xxx_read handler not inited");
        ret = TEMP_HUMI_ERRORRESOURCE;
        return ret;
    }
    if ( NULL == gp_temp_humi_instance->p_os_queue_interface ) 
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                            "bsp_temp_humi_xxx_read: queue interface is NULL");
        return TEMP_HUMI_ERRORRESOURCE;
    }
    if ( NULL == gp_temp_humi_instance->p_os_queue_interface->pf_os_queue_put )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                         "bsp_temp_humi_xxx_read: queue put function is NULL");
        return TEMP_HUMI_ERRORRESOURCE;
    }
    if ( NULL == gp_temp_humi_instance->event_queue_handle ) 
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                               "bsp_temp_humi_xxx_read: queue handle is NULL");
        return TEMP_HUMI_ERRORRESOURCE;
    }

    /*********** 2.Put Event into Handler Queue **********/
    ret = gp_temp_humi_instance->p_os_queue_interface->pf_os_queue_put(
                                    gp_temp_humi_instance->event_queue_handle,
                                    event,
                                    QUEUE_MAX_DELAY    
                                           );
    if ( TEMP_HUMI_OK != ret )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                         "bsp_temp_humi_xxx_read put event from queue failed");
        return ret;
    }

    return ret;
}

/**
 * @brief Temperature and humidity handler thread function
 *
 * @param[in] argument: Pointer to input argument structure with required
 *                      interfaces for handler instantiation
 *
 * @param[out] None
 *
 * @return None
 *
 * */
void temp_humi_handler_thread(void *argument)
{
    DEBUG_OUT(d, TEMP_HUMI_LOG_TAG, "temp_humi_handler_thread start");
    temp_humi_handler_input_arg_t * p_input_arg   =   NULL;
    temp_humi_status_t              ret   =   TEMP_HUMI_OK;
    temp_humi_xxx_event_t                            event;

    float temperature = 0.0f;
    float humidity    = 0.0f;

    bsp_aht21_driver_t          aht21_driver_instance = {0};
    bsp_temp_humi_xxx_handler_t      handler_instance = {0};
    temp_humi_handler_private_data_t     private_data = {0};

    if ( NULL == argument )
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
         "temp_humi_handler_thread input error parameter");
        return;
    }
    p_input_arg = (temp_humi_handler_input_arg_t *)argument;

    // Mount private data and driver instance
    handler_instance.p_private_data   =          &private_data;
    handler_instance.p_aht21_instance = &aht21_driver_instance;

    ret = bsp_temp_humi_xxx_handler_inst(
                            &handler_instance,
                            p_input_arg
                                        );
    if ( TEMP_HUMI_OK == ret )
    {
        __mount_handler(&handler_instance);
    }
    else
    {
        DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
           "temp_humi_handler_thread handler inst failed");
        return;
    }

    /* Infinite loop */
    for(;;)
    {
        // Wait for event from queue
        ret = handler_instance.p_os_queue_interface->pf_os_queue_get(
                                    handler_instance.event_queue_handle,
                                    (void *)&event,
                                    QUEUE_MAX_DELAY    
                                           );
        if ( TEMP_HUMI_OK != ret )
        {
            DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                       "temp_humi_handler_thread get event from queue failed");
            continue;
        }
        if ( NULL == event.pf_callback )
        {
            DEBUG_OUT(e, TEMP_HUMI_ERR_LOG_TAG, 
                            "temp_humi_handler_thread event callback is NULL");
            continue;
        }

        // Process event to get temperature and humidity
        ret = get_temp_humi(
                     &event,
                     &handler_instance,
                     &temperature,
                     &humidity
                           );
        if ( TEMP_HUMI_OK == ret )
        {
            event.pf_callback(&temperature, &humidity, event.p_user_ctx);
        }
    }
}

//******************************** Functions ********************************//
