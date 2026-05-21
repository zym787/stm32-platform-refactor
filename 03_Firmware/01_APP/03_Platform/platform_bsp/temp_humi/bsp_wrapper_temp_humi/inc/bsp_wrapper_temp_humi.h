/******************************************************************************
 * @file bsp_wrapper_temp_humi.h
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Abstract interface for temperature/humidity sensor access.
 *
 * Exposes two read modes to the application layer:
 *
 *  _sync  — blocks the caller until fresh data is available.
 *           Returns wp_temp_humi_status_t; values via float * out-parameters.
 *
 *  _async — posts a read request and returns immediately.
 *           The supplied callback is invoked from the handler thread context
 *           once data is ready.
 *
 * A concrete driver is registered at startup via drv_adapter_temp_humi_mount().
 *
 * @version V1.0 2026-04-12
 * @version V2.0 2026-04-12
 * @upgrade 2.0: Added _sync / _async API variants.
 * @version V3.0 2026-04-12
 * @upgrade 3.0: wp_temp_humi_status_t moved here from handler header;
 *               sync API returns wp_temp_humi_status_t.
 * @version V4.0 2026-04-13
 * @upgrade 4.0: Introduced temp_humi_cb_async_t (two-parameter, no user_ctx);
 *               async vtable slots and public async API use this type.
 * @version V5.0 2026-04-22
 * @upgrade 5.0: life_time parameter added to all vtable slots and public APIs;
 *               caller controls per-request data-cache TTL in milliseconds.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_WRAPPER_TEMP_HUMI_H__
#define __BSP_WRAPPER_TEMP_HUMI_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//******************************** Includes *********************************//

//******************************* Declaring *********************************//

/**
 * @brief Return codes for all temperature/humidity abstraction-layer APIs.
 */
typedef enum
{
    WP_TEMP_HUMI_OK               = 0,         /* Operation successful       */
    WP_TEMP_HUMI_ERROR            = 1,         /* General error              */
    WP_TEMP_HUMI_ERRORTIMEOUT     = 2,         /* Timeout error              */
    WP_TEMP_HUMI_ERRORRESOURCE    = 3,         /* Resource unavailable       */
    WP_TEMP_HUMI_ERRORPARAMETER   = 4,         /* Invalid parameter          */
    WP_TEMP_HUMI_ERRORNOMEMORY    = 5,         /* Out of memory              */
    WP_TEMP_HUMI_ERRORUNSUPPORTED = 6,         /* Unsupported feature        */
    WP_TEMP_HUMI_ERRORISR         = 7,         /* ISR context error          */
    WP_TEMP_HUMI_RESERVED         = 0xFF,      /* Reserved                   */
} wp_temp_humi_status_t;

/**
 * @brief Callback type for asynchronous reads.
 *
 * Both data pointers are always provided; for single-axis reads the unused
 * pointer carries 0.0f.
 */
typedef void (*temp_humi_cb_async_t)(float *temp, float *humi);

/**
 * @brief Driver vtable — populated at startup by drv_adapter_temp_humi_mount()
 */
typedef struct _temp_humi_drv_t
{
    uint32_t                       idx;
    uint32_t                    dev_id;
    void *                   user_data;

    void (*pf_temp_humi_drv_init  )(struct _temp_humi_drv_t *const dev);
    void (*pf_temp_humi_drv_deinit)(struct _temp_humi_drv_t *const dev);

    /* Synchronous — block caller until data is ready */
    wp_temp_humi_status_t (*pf_temp_humi_read_temp_sync)(
                            struct _temp_humi_drv_t *const  dev,
                                              float *const temp,
                                            uint32_t life_time);
    wp_temp_humi_status_t (*pf_temp_humi_read_humi_sync)(
                            struct _temp_humi_drv_t *const dev,
                                              float *const humi,
                                            uint32_t life_time);
    wp_temp_humi_status_t (*pf_temp_humi_read_all_sync )(
                            struct _temp_humi_drv_t *const dev,
                                              float *const temp,
                                              float *const humi,
                                             uint32_t life_time);

    /* Asynchronous — return immediately, result delivered via callback */
    void (*pf_temp_humi_read_temp_async)(
                            struct _temp_humi_drv_t *const dev,
                                    temp_humi_cb_async_t    cb,
                                            uint32_t life_time);
    void (*pf_temp_humi_read_humi_async)(
                            struct _temp_humi_drv_t *const dev,
                                    temp_humi_cb_async_t    cb,
                                            uint32_t life_time);
    void (*pf_temp_humi_read_all_async )(
                            struct _temp_humi_drv_t *const dev,
                                    temp_humi_cb_async_t    cb,
                                            uint32_t life_time);
} temp_humi_drv_t;

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/** Register a concrete driver into the wrapper vtable slot @p idx. */
bool drv_adapter_temp_humi_mount(uint32_t idx, temp_humi_drv_t *const drv);

void temp_humi_drv_init  (void);
void temp_humi_drv_deinit(void);

/**
 * @brief Synchronous API — caller blocks until sensor data is ready.
 * @return TEMP_HUMI_OK on success, TEMP_HUMI_ERRORTIMEOUT on timeout,
 *         TEMP_HUMI_ERRORRESOURCE if no driver is mounted.
 */
wp_temp_humi_status_t temp_humi_read_temp_sync(float *const      temp, 
                                                   uint32_t life_time);
wp_temp_humi_status_t temp_humi_read_humi_sync(float *const      humi, 
                                                   uint32_t life_time);
wp_temp_humi_status_t temp_humi_read_all_sync (float *const      temp,
                                               float *const      humi, 
                                                   uint32_t life_time);

/**
 * @brief Asynchronous API — returns immediately.
 * @p callback fires in handler thread context once data is ready.
 */
void temp_humi_read_temp_async(temp_humi_cb_async_t  callback, 
                                           uint32_t life_time);
void temp_humi_read_humi_async(temp_humi_cb_async_t  callback, 
                                           uint32_t life_time);
void temp_humi_read_all_async (temp_humi_cb_async_t  callback, 
                                           uint32_t life_time);

//******************************* Functions *********************************//

#endif /* __BSP_WRAPPER_TEMP_HUMI_H__ */
