/******************************************************************************
 * @file bsp_temp_humi_xxx_handler.h
  * * @par dependencies: 
 *  - bsp_aht21_driver.h
 *
 * @author Ethan-Hang
 *
 * @brief Temperature and humidity handler interface definition
 *
 * Processing flow:
 * 1. Handler instantiation with required interfaces
 * 2. Event queue creation and management
 * 3. Temperature/humidity data reading and callback execution
 *
 * @version V1.0 2025-11-28
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/
#ifndef __BSP_TEMP_HUMI_XXX_HANDLER_H__
#define __BSP_TEMP_HUMI_XXX_HANDLER_H__

//******************************** Includes *********************************//
#include "bsp_aht21_driver.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************** Typedefs *********************************//
/**
 * @brief Temperature and humidity handler status enumeration
 *        Emulation of return case
 */
typedef enum
{
    TEMP_HUMI_OK               = 0,       /* Operation successful            */
    TEMP_HUMI_ERROR            = 1,       /* General error                   */
    TEMP_HUMI_ERRORTIMEOUT     = 2,       /* Timeout error                   */
    TEMP_HUMI_ERRORRESOURCE    = 3,       /* Resource unavailable            */
    TEMP_HUMI_ERRORPARAMETER   = 4,       /* Invalid parameter               */
    TEMP_HUMI_ERRORNOMEMORY    = 5,       /* Out of memory                   */
    TEMP_HUMI_ERRORUNSUPPORTED = 6,       /* Unsupported feature             */
    TEMP_HUMI_ERRORISR         = 7,       /* ISR context error               */
    TEMP_HUMI_RESERVED         = 0xFF,    /* AHT21 Reserved                  */
} temp_humi_status_t;

/**
 * @brief Temperature and humidity data type event enumeration
 */
typedef enum
{
    TEMP_HUMI_EVENT_TEMP       = 0,       /* Temperature only event          */
    TEMP_HUMI_EVENT_HUMI       = 1,       /* Humidity only event             */
    TEMP_HUMI_EVENT_BOTH       = 2,       /* Both temperature and humidity   */
} temp_humi_data_type_event_t;

/**
 * @brief Temperature and humidity event structure
 *        Class of Event
 */
typedef struct
{
    float                       temperature;
    float                          humidity;
    uint32_t                       lifetime;
    uint32_t                      timestamp;
    temp_humi_data_type_event_t  event_type;
    void                        *p_user_ctx;
    void   (*pf_callback)(float *, float *, void*);
} temp_humi_xxx_event_t;
//******************************** Typedefs *********************************//

//**************************** Interface Structs ****************************//
/**
 * @brief OS queue service interface
 * */
typedef struct 
{   
    /** Create a message queue */
    temp_humi_status_t (*pf_os_queue_create) (
                                                  uint32_t const      item_num,
                                                  uint32_t const     item_size,
                                                  void **  const queue_handler
                                             );
    /** Put message into queue */
    temp_humi_status_t (*pf_os_queue_put   ) (
                                                  void *  const  queue_handler,
                                                  void *  const           item,
                                                  uint32_t             timeout
                                             );    
    /** Get message from queue */
    temp_humi_status_t (*pf_os_queue_get   ) (
                                                  void *  const  queue_handler,
                                                  void *  const            msg,
                                                  uint32_t             timeout
                                             );
    /** Delete queue */
    temp_humi_status_t (*pf_os_queue_delete) (
                                                  void *  const  queue_handler
                                             );
} handler_os_queue_t;

/**
 * @brief Forward declaration of private data structure
 */
typedef struct temp_humi_handler_private_data temp_humi_handler_private_data_t;

/**
 * @brief Input interface structure for handler instantiation
 *        Input Interface when instantiated
 */
typedef struct
{
                                                    /* I2C driver interface  */
    aht21_iic_driver_interface_t  * p_iic_driver_interface; 
                                                    /* OS queue interface    */
    handler_os_queue_t            *   p_os_queue_interface; 
                                                    /* Timebase interface    */
    aht21_timebase_interface_t    *   p_timebase_interface; 
                                                    /* Yield interface       */
    aht21_yield_interface_t       *      p_yield_interface;
} temp_humi_handler_input_arg_t;
//**************************** Interface Structs ****************************//


/**
 * @brief Temperature and humidity handler class structure
 */
typedef struct
{
    /*                    Private data                   */
    temp_humi_handler_private_data_t\
                                  *         p_private_data;
    /*          Interface passed to driver layer         */

    aht21_iic_driver_interface_t  * p_iic_driver_interface;
    aht21_timebase_interface_t    *   p_timebase_interface;

    /*              Interface from OS layer              */
    handler_os_queue_t            *   p_os_queue_interface;
    aht21_yield_interface_t       *      p_yield_interface;

    /*            Instance of driver instance            */
    bsp_aht21_driver_t            *       p_aht21_instance;

    /*              Handler of event queue               */
    void                          *     event_queue_handle;

    /*       Timestamp of last temperature reading       */
    uint32_t                                last_temp_tick;
    /*        Timestamp of last humidity reading         */
    uint32_t                                last_humi_tick;
} bsp_temp_humi_xxx_handler_t;
//******************************** Classes **********************************//

//******************************** APIs *************************************//
/**
 * @brief Initialize and instantiate the temperature and humidity handler
 *
 * @param[in] handler_instance: Pointer to the temperature and humidity
 *                               handler instance
 * @param[in] input_argument: Pointer to the input argument structure
 *                            containing required interfaces
 *
 * @param[out] handler_instance: Initialized handler instance with
 *                               mounted interfaces
 *
 * @return temp_humi_status_t: Operation status (TEMP_HUMI_OK on success,
 *                             error code on failure)
 *
 * */
temp_humi_status_t bsp_temp_humi_xxx_handler_inst(
                        bsp_temp_humi_xxx_handler_t   * const handler_instance,
                        temp_humi_handler_input_arg_t * const   input_argument
                                                 );

/**
 * @brief Read temperature and humidity data by putting event into the
 *        handler queue
 *
 * @param[in] event: Pointer to the temperature and humidity event
 *                   structure
 *
 * @param[out] event: Event queued for processing by the handler thread
 *
 * @return temp_humi_status_t: Operation status (TEMP_HUMI_OK on success,
 *                             error code on failure)
 *
 * */                                                 
temp_humi_status_t bsp_temp_humi_xxx_read(
                              temp_humi_xxx_event_t   * const            event
                                         );

//******************************** APIs *************************************//

#endif // End of __BSP_TEMP_HUMI_XXX_HANDLER_H__
