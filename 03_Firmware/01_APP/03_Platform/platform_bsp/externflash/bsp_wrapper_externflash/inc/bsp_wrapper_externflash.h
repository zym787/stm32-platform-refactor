/******************************************************************************
 * @file bsp_wrapper_externflash.h
 *
 * @par dependencies
 * - stdint.h
 * - stdbool.h
 * - stddef.h
 *
 * @author Ethan-Hang
 *
 * @brief Abstract interface for external flash (NOR) access.
 *        Decouples the application from any specific NOR flash driver.
 *        The adapter layer registers a concrete driver via
 *        drv_adapter_externflash_mount(); the application calls the public API
 *        without knowing which hardware is underneath.
 *
 * Processing flow:
 *   1. Adapter calls drv_adapter_externflash_mount() to register the vtable.
 *   2. Application calls externflash_drv_init() once at startup.
 *   3. Application uses externflash_drv_read / write / erase / sleep / wakeup
 *      to operate the underlying device.  All operations are queued to the
 *      handler thread; an optional callback signals completion.
 *
 * @version V1.0 2026-05-02
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_WRAPPER_EXTERNFLASH_H__
#define __BSP_WRAPPER_EXTERNFLASH_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    WP_EXTERNFLASH_OK               = 0,    /* Operation successful           */
    WP_EXTERNFLASH_ERROR            = 1,    /* General error                  */
    WP_EXTERNFLASH_ERRORTIMEOUT     = 2,    /* Timeout error                  */
    WP_EXTERNFLASH_ERRORRESOURCE    = 3,    /* Resource unavailable           */
    WP_EXTERNFLASH_ERRORPARAMETER   = 4,    /* Invalid parameter              */
    WP_EXTERNFLASH_ERRORNOMEMORY    = 5,    /* Out of memory                  */
    WP_EXTERNFLASH_ERRORUNSUPPORTED = 6,    /* Unsupported feature            */
    WP_EXTERNFLASH_ERRORISR         = 7,    /* ISR context error              */
    WP_EXTERNFLASH_RESERVED         = 0xFF, /* Reserved                       */
} wp_externflash_status_t;

/**
 * @brief Completion record handed to the user callback.
 *
 * Mirrors the per-event payload (addr / size / data / status) without
 * leaking the lower-layer flash_event_t into the application.
 */
typedef struct
{
    uint32_t                       addr;
    uint32_t                       size;
    uint8_t              *         data;
    wp_externflash_status_t      status;
    void                 *   p_user_ctx;
} wp_externflash_result_t;

/**
 * @brief Application-level completion callback.
 *
 * Invoked by the handler thread (NOT in ISR context) after each
 * asynchronous request finishes.  Keep work inside the callback short.
 */
typedef void (*wp_externflash_callback_t)(wp_externflash_result_t *result);

/**
 * @brief Driver vtable for an external flash device.
 *        Filled by the adapter layer; opaque to the application.
 */
typedef struct _externflash_drv_t
{
    uint8_t                                   idx;       /* Slot index        */
    uint32_t                               dev_id;       /* Device id         */
    void                       *        user_data;       /* Adapter context   */

    void                    (*pf_externflash_drv_init  )\
                                  (struct _externflash_drv_t *const dev);
    void                    (*pf_externflash_drv_deinit)\
                                  (struct _externflash_drv_t *const dev);

    wp_externflash_status_t (*pf_externflash_read       )\
                                  (struct _externflash_drv_t *const       dev,
                                                  uint32_t                addr,
                                                  uint8_t  *              data,
                                                  uint32_t                size,
                                                  wp_externflash_callback_t cb,
                                                  void     *       p_user_ctx);

    wp_externflash_status_t (*pf_externflash_write      )\
                                  (struct _externflash_drv_t *const       dev,
                                                  uint32_t                addr,
                                                  uint8_t  *              data,
                                                  uint32_t                size,
                                                  wp_externflash_callback_t cb,
                                                  void     *       p_user_ctx);

    wp_externflash_status_t (*pf_externflash_write_noerase)\
                                  (struct _externflash_drv_t *const       dev,
                                                  uint32_t                addr,
                                                  uint8_t  *              data,
                                                  uint32_t                size,
                                                  wp_externflash_callback_t cb,
                                                  void     *       p_user_ctx);

    wp_externflash_status_t (*pf_externflash_erase_chip )\
                                  (struct _externflash_drv_t *const       dev,
                                                  wp_externflash_callback_t cb,
                                                  void     *       p_user_ctx);

    wp_externflash_status_t (*pf_externflash_erase_sector)\
                                  (struct _externflash_drv_t *const       dev,
                                                  uint32_t                addr,
                                                  wp_externflash_callback_t cb,
                                                  void     *       p_user_ctx);

    wp_externflash_status_t (*pf_externflash_sleep      )\
                                  (struct _externflash_drv_t *const       dev,
                                                  wp_externflash_callback_t cb,
                                                  void     *       p_user_ctx);

    wp_externflash_status_t (*pf_externflash_wakeup     )\
                                  (struct _externflash_drv_t *const       dev,
                                                  wp_externflash_callback_t cb,
                                                  void     *       p_user_ctx);
} externflash_drv_t;
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/**
 * @brief   Register an external flash driver into the wrapper slot table.
 *          Called exclusively by the adapter layer at system init.
 *
 * @param[in] idx : Slot index (0 ~ EXTERNFLASH_DRV_MAX_NUM-1).
 * @param[in] drv : Pointer to the vtable-filled driver struct.
 *
 * @return  true  - Mounted successfully.
 *          false - Invalid index or NULL drv.
 */
bool drv_adapter_externflash_mount(uint8_t                  idx,
                                   externflash_drv_t *const drv);

/**
 * @brief   Initialise the currently active external flash driver.
 */
void                    externflash_drv_init        (void);

/**
 * @brief   Deinitialise the currently active external flash driver.
 */
void                    externflash_drv_deinit      (void);

/**
 * @brief   Submit an asynchronous read.  The caller owns @p data until
 *          @p cb fires (or, if @p cb is NULL, until the operation has
 *          completed by other means).
 *
 * @param[in]  addr       : Flash byte address to start reading from.
 * @param[out] data       : Destination buffer (caller-owned).
 * @param[in]  size       : Number of bytes to read.
 * @param[in]  cb         : Optional completion callback (may be NULL).
 * @param[in]  p_user_ctx : Opaque context pointer forwarded to @p cb.
 *
 * @return  WP_EXTERNFLASH_OK if the request was queued, error code otherwise.
 */
wp_externflash_status_t externflash_drv_read         (uint32_t                addr,
                                                      uint8_t  *              data,
                                                      uint32_t                size,
                                                      wp_externflash_callback_t cb,
                                                      void     *       p_user_ctx);

/**
 * @brief   Submit an asynchronous write that erases the affected sectors first.
 *          Same ownership and threading rules as externflash_drv_read().
 */
wp_externflash_status_t externflash_drv_write        (uint32_t                addr,
                                                      uint8_t  *              data,
                                                      uint32_t                size,
                                                      wp_externflash_callback_t cb,
                                                      void     *       p_user_ctx);

/**
 * @brief   Submit an asynchronous write WITHOUT a preceding erase.  The
 *          caller must guarantee the target region is already erased.
 */
wp_externflash_status_t externflash_drv_write_noerase(uint32_t                addr,
                                                      uint8_t  *              data,
                                                      uint32_t                size,
                                                      wp_externflash_callback_t cb,
                                                      void     *       p_user_ctx);

/**
 * @brief   Submit an asynchronous chip-erase.
 */
wp_externflash_status_t externflash_drv_erase_chip   (wp_externflash_callback_t cb,
                                                      void     *       p_user_ctx);

/**
 * @brief   Submit an asynchronous sector-erase at @p addr.
 */
wp_externflash_status_t externflash_drv_erase_sector (uint32_t                addr,
                                                      wp_externflash_callback_t cb,
                                                      void     *       p_user_ctx);

/**
 * @brief   Put the device into deep power-down.
 */
wp_externflash_status_t externflash_drv_sleep        (wp_externflash_callback_t cb,
                                                      void     *       p_user_ctx);

/**
 * @brief   Bring the device out of deep power-down.
 */
wp_externflash_status_t externflash_drv_wakeup       (wp_externflash_callback_t cb,
                                                      void     *       p_user_ctx);
//******************************* Declaring *********************************//

#endif /* __BSP_WRAPPER_EXTERNFLASH_H__ */
