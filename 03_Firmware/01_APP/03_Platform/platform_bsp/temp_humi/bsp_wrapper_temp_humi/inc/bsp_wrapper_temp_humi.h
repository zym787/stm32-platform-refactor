/******************************************************************************
 * @file bsp_wrapper_temp_humi.h
 *
 * @par dependencies
 * - platform_type.h
 *
 * @author Ethan-Hang
 *
 * @brief Abstract interface for temperature/humidity sensor access.
 *
 * Exposes two read modes to the application layer:
 *
 *  _sync  — blocks the caller until fresh data is available.
 *           Returns platform_err_t; values via FLOAT32_T * out-parameters.
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
 * @upgrade 3.0: platform_err_t moved here from handler header;
 *               sync API returns platform_err_t.
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
#include "platform_type.h"
#include "platform_error.h"

//******************************** Includes *********************************//

//******************************* Declaring *********************************//

/**
 * @brief Return codes for all temperature/humidity abstraction-layer APIs.
 */

/**
 * @brief Callback type for asynchronous reads.
 *
 * Both data pointers are always provided; for single-axis reads the unused
 * pointer carries 0.0f.
 */
typedef void (*temp_humi_cb_async_t)(FLOAT32_T *temp, FLOAT32_T *humi);

/**
 * @brief Driver vtable — populated at startup by drv_adapter_temp_humi_mount()
 */
typedef struct _temp_humi_drv_t
{
    UINT32_T                       idx;
    UINT32_T                    dev_id;
    void *                   user_data;

    void (*pf_temp_humi_drv_init  )(struct _temp_humi_drv_t *const dev);
    void (*pf_temp_humi_drv_deinit)(struct _temp_humi_drv_t *const dev);

    /* Synchronous — block caller until data is ready */
    platform_err_t (*pf_temp_humi_read_temp_sync)(
                            struct _temp_humi_drv_t *const  dev,
                                              FLOAT32_T *const temp,
                                            UINT32_T life_time);
    platform_err_t (*pf_temp_humi_read_humi_sync)(
                            struct _temp_humi_drv_t *const dev,
                                              FLOAT32_T *const humi,
                                            UINT32_T life_time);
    platform_err_t (*pf_temp_humi_read_all_sync )(
                            struct _temp_humi_drv_t *const dev,
                                              FLOAT32_T *const temp,
                                              FLOAT32_T *const humi,
                                             UINT32_T life_time);

    /* Asynchronous — return immediately, result delivered via callback */
    void (*pf_temp_humi_read_temp_async)(
                            struct _temp_humi_drv_t *const dev,
                                    temp_humi_cb_async_t    cb,
                                            UINT32_T life_time);
    void (*pf_temp_humi_read_humi_async)(
                            struct _temp_humi_drv_t *const dev,
                                    temp_humi_cb_async_t    cb,
                                            UINT32_T life_time);
    void (*pf_temp_humi_read_all_async )(
                            struct _temp_humi_drv_t *const dev,
                                    temp_humi_cb_async_t    cb,
                                            UINT32_T life_time);
} temp_humi_drv_t;

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/** Register a concrete driver into the wrapper vtable slot @p idx. */
BOOL_T drv_adapter_temp_humi_mount(UINT32_T idx, temp_humi_drv_t *const drv);

void temp_humi_drv_init  (void);
void temp_humi_drv_deinit(void);

/**
 * @brief Synchronous API — caller blocks until sensor data is ready.
 * @return TEMP_HUMI_OK on success, TEMP_HUMI_ERRORTIMEOUT on timeout,
 *         TEMP_HUMI_ERRORRESOURCE if no driver is mounted.
 */
platform_err_t temp_humi_read_temp_sync(FLOAT32_T *const      temp, 
                                                   UINT32_T life_time);
platform_err_t temp_humi_read_humi_sync(FLOAT32_T *const      humi, 
                                                   UINT32_T life_time);
platform_err_t temp_humi_read_all_sync (FLOAT32_T *const      temp,
                                               FLOAT32_T *const      humi, 
                                                   UINT32_T life_time);

/**
 * @brief Asynchronous API — returns immediately.
 * @p callback fires in handler thread context once data is ready.
 */
void temp_humi_read_temp_async(temp_humi_cb_async_t  callback, 
                                           UINT32_T life_time);
void temp_humi_read_humi_async(temp_humi_cb_async_t  callback, 
                                           UINT32_T life_time);
void temp_humi_read_all_async (temp_humi_cb_async_t  callback, 
                                           UINT32_T life_time);

//******************************* Functions *********************************//

#endif /* __BSP_WRAPPER_TEMP_HUMI_H__ */
