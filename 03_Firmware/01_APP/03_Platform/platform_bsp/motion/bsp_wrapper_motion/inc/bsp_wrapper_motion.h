/******************************************************************************
 * @file bsp_wrapper_motion.h
 *
 * @par dependencies
 * - platform_type.h
 *
 * @author Ethan-Hang
 *
 * @brief Abstract interface for motion sensor access.
 *        Decouples the application from any specific motion sensor driver.
 *        The adapter layer registers a concrete driver via
 *        drv_adapter_motion_mount(); the application calls the public API
 *        without knowing which hardware is underneath.
 *
 * Processing flow:
 *   1. Adapter calls drv_adapter_motion_mount() to register the driver vtable.
 *   2. Application calls motion_drv_init() once at startup.
 *   3. Application calls motion_drv_get_req() to block until new data arrives.
 *   4. Application reads raw data via motion_get_data_addr().
 *   5. Application signals data consumed via motion_read_data_done().
 *
 * @version V1.0 2026-04-16
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_WRAPPER_MOTION_H__
#define __BSP_WRAPPER_MOTION_H__

//******************************** Includes *********************************//
#include "platform_type.h"
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

/**
 * @brief Driver vtable for a motion sensor.
 *        Filled by the adapter layer; opaque to the application.
 */
typedef struct _motion_drv_t
{
    uint8_t                        idx;       /* Slot index in wrapper array */
    uint32_t                    dev_id;       /* Hardware device identifier  */
    void *                   user_data;       /* Adapter private context     */

    void              (*pf_motion_drv_init  )(struct _motion_drv_t *const dev);
    void              (*pf_motion_drv_deinit)(struct _motion_drv_t *const dev);

    platform_err_t(*pf_motion_drv_get_req    )\
                                             (struct _motion_drv_t *const dev);
    uint8_t *         (*pf_motion_get_data_addr  )\
                                             (struct _motion_drv_t *const dev);
    void              (*pf_motion_read_data_done )\
                                             (struct _motion_drv_t *const dev);
} motion_drv_t;
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/**
 * @brief   Register a motion driver into the wrapper slot table.
 *          Called exclusively by the adapter layer at system init.
 *
 * @param[in] idx : Slot index (0 ~ MOTION_DRV_MAX_NUM-1).
 * @param[in] drv : Pointer to the vtable-filled driver struct.
 *
 * @return  true  - Mounted successfully.
 *          false - Invalid index or NULL drv.
 */
bool drv_adapter_motion_mount(uint8_t idx, motion_drv_t *const drv);

/**
 * @brief   Initialize the currently active motion sensor driver.
 *          Forwards to pf_motion_drv_init in the registered vtable.
 */
void               motion_drv_init      (void);

/**
 * @brief   Deinitialize the currently active motion sensor driver.
 *          Forwards to pf_motion_drv_deinit in the registered vtable.
 */
void               motion_drv_deinit    (void);

/**
 * @brief   Block until a new motion data packet is available.
 *          Internally waits on the driver's OS queue or equivalent mechanism.
 *
 * @return  PLATFORM_OK           - A new data packet is ready.
 *          PLATFORM_ERR_NO_RESOURCE - Driver not registered or not initialized.
 *          Other                   - Driver-specific error.
 */
platform_err_t motion_drv_get_req   (void);

/**
 * @brief   Get the address of the current raw data packet in the ring buffer.
 *          Must be called after motion_drv_get_req() returns PLATFORM_OK.
 *
 * @return  Pointer to the raw data byte array, or NULL on error.
 */
uint8_t *          motion_get_data_addr (void);

/**
 * @brief   Signal that the caller has finished reading the current packet.
 *          Advances the ring buffer read pointer.
 *          Must be called once per motion_drv_get_req() / motion_get_data_addr() pair.
 */
void               motion_read_data_done(void);
//******************************* Declaring *********************************//

#endif /* __BSP_WRAPPER_MOTION_H__ */
