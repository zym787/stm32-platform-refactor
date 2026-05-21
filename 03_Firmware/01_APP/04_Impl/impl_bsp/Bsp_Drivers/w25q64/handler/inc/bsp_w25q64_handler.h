/******************************************************************************
 * @file bsp_w25q64_handler.h
 *
 * @par dependencies
 * - bsp_w25q64_driver.h
 *
 * @author Ethan-Hang
 *
 * @brief W25Q64 flash handler interface definition.
 *
 * Processing flow:
 * 1. Handler instantiation with required interfaces
 * 2. Event queue creation and W25Q64 driver initialization
 * 3. Event-driven flash operations (read / write / erase / sleep / wakeup)
 *    with optional callback for asynchronous result delivery
 *
 * @version V1.0 2026-04-27
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_W25Q64_HANDLER_H__
#define __BSP_W25Q64_HANDLER_H__

//******************************** Includes *********************************//
#include "bsp_w25q64_driver.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    FLASH_HANLDER_OK               = 0,    /* Operation successful           */
    FLASH_HANLDER_ERROR            = 1,    /* General error                  */
    FLASH_HANLDER_ERRORTIMEOUT     = 2,    /* Timeout error                  */
    FLASH_HANLDER_ERRORRESOURCE    = 3,    /* Resource unavailable           */
    FLASH_HANLDER_ERRORPARAMETER   = 4,    /* Invalid parameter              */
    FLASH_HANLDER_ERRORNOMEMORY    = 5,    /* Out of memory                  */
    FLASH_HANLDER_ERRORUNSUPPORTED = 6,    /* Unsupported feature            */
    FLASH_HANLDER_ERRORISR         = 7,    /* ISR context error              */
    FLASH_HANLDER_RESERVED         = 0xFF, /* Reserved                       */
} flash_handler_status_t;

typedef enum
{
    FLASH_HANDLER_EVENT_READ            = 0,
    FLASH_HANDLER_EVENT_WRITE           = 1,
    FLASH_HANDLER_EVENT_WRITE_NOERASE   = 2,
    FLASH_HANDLER_EVENT_ERASE_CHIP      = 3,
    FLASH_HANDLER_EVENT_ERASE_SECTOR    = 4,
    FLASH_HANDLER_EVENT_WAKEUP          = 5,
    FLASH_HANDLER_EVENT_SLEEP           = 6,
    FLASH_HANDLER_EVENT_TEST            = 7,
} flash_handler_event_type_t;

/**
 * @brief Flash event structure passed through the queue.
 *
 * The handler thread fills `status` with the operation result before
 * invoking `pf_callback(&event)`.  For READ operations, `data` points
 * to the caller-supplied buffer that receives the read payload; for
 * WRITE operations, it points to the data to be programmed.
 */
typedef struct
{
    uint32_t                            addr;
    uint32_t                            size;
    uint8_t                       *     data;
    flash_handler_status_t            status;
    flash_handler_event_type_t    event_type;
    void                       *  p_user_ctx;
    void (*pf_callback)(void *args);
} flash_event_t;

/**
 * @brief OS queue service interface used by the handler.
 */
typedef struct
{
    /** Create a message queue */
    flash_handler_status_t (*pf_os_queue_create) (
                                                 uint32_t const      item_num,
                                                 uint32_t const     item_size,
                                                 void **  const queue_handler);
    /** Put message into queue */
    flash_handler_status_t (*pf_os_queue_put   ) (
                                                 void *  const  queue_handler,
                                                 void *  const           item,
                                                 uint32_t             timeout);
    /** Get message from queue */
    flash_handler_status_t (*pf_os_queue_get   ) (
                                                 void *  const  queue_handler,
                                                 void *  const            msg,
                                                 uint32_t             timeout);
    /** Delete queue */
    flash_handler_status_t (*pf_os_queue_delete) (
                                                 void *  const  queue_handler);
} flash_handler_os_queue_t;

/**
 * @brief OS task-delay interface used by the handler.
 */
typedef struct
{
    flash_handler_status_t (*pf_os_delay_ms)(uint32_t ms);
} flash_handler_os_delay_t;

/**
 * @brief Aggregate of all OS-side interfaces consumed by the handler.
 */
typedef struct
{
    flash_handler_os_queue_t    *     p_os_queue_interface;
    flash_handler_os_delay_t    *     p_os_delay_interface;
} flash_os_interface_t;

/**
 * @brief Forward declaration of private state owned by the handler.
 */
typedef struct flash_handler_private_data flash_handler_private_data_t;

/**
 * @brief Argument structure passed to the handler thread on startup.
 *
 * The integration layer assembles this aggregate (concrete OS, SPI,
 * timebase, and driver-side OS-delay interfaces) and hands it to
 * `flash_handler_thread()` via the task argument pointer.
 */
typedef struct
{
    flash_os_interface_t        *           p_os_interface;
    w25q64_spi_interface_t      *          p_spi_interface;
    w25q64_timebase_interface_t *     p_timebase_interface;
    w25q64_os_delay_t           *        p_w25q64_os_delay;
} flash_input_args_t;

/**
 * @brief Handler instance class.
 */
typedef struct bsp_w25q64_handler
{
    /*                    Private data                   */
    flash_handler_private_data_t *           p_private_data;

    /*               Interface from OS layer              */
    flash_os_interface_t         *           p_os_interface;

    /*         Interfaces passed down to driver layer     */
    w25q64_spi_interface_t       *          p_spi_interface;
    w25q64_timebase_interface_t  *     p_timebase_interface;
    w25q64_os_delay_t            *        p_w25q64_os_delay;

    /*           Instance of underlying driver            */
    bsp_w25q64_driver_t          *        p_w25q64_instance;

    /*              Handle of event queue                 */
    void                         *    p_event_queue_handler;
} bsp_w25q64_handler_t;

//******************************** Defines **********************************//

//******************************* Functions *********************************//
/**
 * @brief Initialize and instantiate the W25Q64 handler.
 *
 * @param[in] handler_instance : Handler instance to initialize.
 * @param[in] input_args       : Aggregate of required external interfaces.
 *
 * @param[out] handler_instance : Initialized handler with mounted
 *                                interfaces, created event queue and
 *                                running underlying driver.
 *
 * @return flash_handler_status_t: FLASH_HANLDER_OK on success,
 *                                 error code on failure.
 * */
flash_handler_status_t bsp_w25q64_handler_inst(
                                 bsp_w25q64_handler_t *const handler_instance,
                                 flash_input_args_t   *const       input_args);

/**
 * @brief Submit a flash event to the handler queue for asynchronous
 *        processing.
 *
 * @param[in] event : Pointer to the flash event structure.
 *
 * @param[out] event : Event queued for processing by the handler thread.
 *
 * @return flash_handler_status_t: FLASH_HANLDER_OK on success,
 *                                 error code on failure.
 * */
flash_handler_status_t handler_flash_event_send(flash_event_t * const event);

//******************************* Functions *********************************//

#endif /* __BSP_W25Q64_HANDLER_H__ */
