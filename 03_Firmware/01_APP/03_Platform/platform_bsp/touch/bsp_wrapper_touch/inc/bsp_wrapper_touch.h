/******************************************************************************
 * @file bsp_wrapper_touch.h
 *
 * @par dependencies
 * - stdint.h
 * - stdbool.h
 * - stddef.h
 *
 * @author Ethan-Hang
 *
 * @brief Abstract interface for capacitive touch panel access.
 *        Decouples the application (LVGL indev port, gesture consumers)
 *        from any specific touch controller driver.  The adapter layer
 *        registers a concrete driver via drv_adapter_touch_mount(); the
 *        application calls the public API without knowing what hardware
 *        is underneath.
 *
 * Processing flow:
 *   1. Adapter calls drv_adapter_touch_mount() to register the driver vtable.
 *   2. Application calls touch_drv_inst() once at task entry — this drives
 *      the adapter's bsp_*_inst() (OSAL-dependent, post-kernel only).
 *   3. Application calls touch_drv_init() once at startup.
 *   4. LVGL indev / app polls touch_get_finger_num() + touch_get_xy().
 *
 * @version V1.0 2026-04-26
 * @version V2.0 2026-04-28
 * @version V3.0 2026-04-28
 * @upgrade 2.0: Added touch_drv_inst() so consumers reach the OSAL-
 *               dependent driver instantiation through the wrapper instead
 *               of the concrete adapter port header.
 * @upgrade 3.0: Added touch_drv_isr_notify() — optional ISR entry the
 *               application may wire from an EXTI handler when running
 *               the driver in interrupt-driven mode.  Polling mode stays
 *               available by simply not wiring the ISR.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_WRAPPER_TOUCH_H__
#define __BSP_WRAPPER_TOUCH_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
 * @brief Return codes for all touch abstraction-layer APIs.
 */
typedef enum
{
    WP_TOUCH_OK               = 0,             /* Operation successful       */
    WP_TOUCH_ERROR            = 1,             /* General error              */
    WP_TOUCH_ERRORTIMEOUT     = 2,             /* Timeout error              */
    WP_TOUCH_ERRORRESOURCE    = 3,             /* Resource unavailable       */
    WP_TOUCH_ERRORPARAMETER   = 4,             /* Invalid parameter          */
    WP_TOUCH_ERRORNOMEMORY    = 5,             /* Out of memory              */
    WP_TOUCH_ERRORUNSUPPORTED = 6,             /* Unsupported feature        */
    WP_TOUCH_ERRORISR         = 7,             /* ISR context error          */
    WP_TOUCH_RESERVED         = 0xFF,          /* Reserved                   */
} wp_touch_status_t;

/**
 * @brief Driver vtable for a touch panel.
 *        Filled by the adapter layer; opaque to the application.
 */
typedef struct _touch_drv_t
{
    uint32_t                       idx;        /* Slot index in wrapper      */
    uint32_t                    dev_id;        /* Hardware device id         */
    void *                   user_data;        /* Adapter private context    */

    wp_touch_status_t (*pf_touch_drv_inst  )(struct _touch_drv_t *const dev);
    void (*pf_touch_drv_init  )(struct _touch_drv_t *const dev);
    void (*pf_touch_drv_deinit)(struct _touch_drv_t *const dev);
    void (*pf_touch_isr_notify)(struct _touch_drv_t *const dev);

    wp_touch_status_t (*pf_touch_get_finger_num)(
                                  struct _touch_drv_t *const  dev,
                                              uint8_t *const  p_finger);
    wp_touch_status_t (*pf_touch_get_xy        )(
                                  struct _touch_drv_t *const  dev,
                                             uint16_t *const  p_x,
                                             uint16_t *const  p_y);
    wp_touch_status_t (*pf_touch_get_chip_id   )(
                                  struct _touch_drv_t *const  dev,
                                              uint8_t *const  p_chip_id);
    wp_touch_status_t (*pf_touch_get_gesture   )(
                                  struct _touch_drv_t *const  dev,
                                              uint8_t *const  p_gesture);
} touch_drv_t;
//******************************** Defines **********************************//

//******************************* Functions *********************************//
/**
 * @brief   Register a touch driver into the wrapper slot table.
 *          Called exclusively by the adapter layer at system init.
 *
 * @param[in] idx : Slot index (0 ~ MAX_TOUCH_DRV_NUM-1).
 * @param[in] drv : Pointer to the vtable-filled driver struct.
 *
 * @return  true  - Mounted successfully.
 *          false - Invalid index or NULL drv.
 */
bool drv_adapter_touch_mount(uint32_t idx, touch_drv_t *const drv);

/**
 * @brief Instantiate the currently active touch driver.
 *
 * Forwards to pf_touch_drv_inst in the registered vtable.  Must run from
 * task context (post-kernel) because the underlying driver inst typically
 * binds OS primitives (bus mutex, os delay) that are only valid after
 * osKernelStart().  Idempotent — implementations should ignore repeated
 * calls after a successful instantiation.
 *
 * @return WP_TOUCH_OK on success, WP_TOUCH_ERRORRESOURCE if no driver
 *         is mounted or the slot has no inst hook, other values on driver
 *         failure (logged by the adapter).
 */
wp_touch_status_t touch_drv_inst(void);

/**
 * @brief Initialize the currently active touch driver.
 *        Forwards to pf_touch_drv_init in the registered vtable.
 */
void touch_drv_init  (void);

/**
 * @brief Notify the touch driver that its interrupt line just fired.
 *
 * Intended call site: an EXTI ISR wired to the controller's INT pin.
 * Whether to wire that ISR (and enable the corresponding NVIC line) is
 * entirely the application's choice — leaving it unwired keeps the
 * driver in pure polling mode.  Safe to call repeatedly; a no-op if no
 * driver is mounted or the active driver has no ISR hook.
 *
 * NOTE: Runs in interrupt context.  The adapter's hook must stay
 *       ISR-safe (no blocking, no mutex acquire) — typical pattern is
 *       to flag a handler task via osal_notify.
 */
void touch_drv_isr_notify(void);

/**
 * @brief Deinitialize the currently active touch driver.
 *        Forwards to pf_touch_drv_deinit in the registered vtable.
 */
void touch_drv_deinit(void);

/**
 * @brief Read the number of fingers currently in contact (0 or 1 on
 *        single-touch panels like CST816T).
 *
 * @param[out] p_finger : Receives the finger count.
 *
 * @return WP_TOUCH_OK on success, WP_TOUCH_ERRORRESOURCE if no driver
 *         is mounted, other values on driver error.
 */
wp_touch_status_t touch_get_finger_num(uint8_t  *const p_finger);

/**
 * @brief Read the latest touch coordinate.  Coordinates are in panel
 *        pixels, with origin at the top-left of the active area.
 *
 * @param[out] p_x : Receives the X pixel coordinate.
 * @param[out] p_y : Receives the Y pixel coordinate.
 *
 * @return WP_TOUCH_OK on success, WP_TOUCH_ERRORRESOURCE if no driver
 *         is mounted, other values on driver error.
 */
wp_touch_status_t touch_get_xy        (uint16_t *const p_x,
                                       uint16_t *const p_y);

/**
 * @brief Read the touch controller chip id.  Useful for boot-time probe.
 *
 * @param[out] p_chip_id : Receives the chip id byte.
 *
 * @return WP_TOUCH_OK on success.
 */
wp_touch_status_t touch_get_chip_id   (uint8_t  *const p_chip_id);

/**
 * @brief Read the latest gesture id (driver-defined encoding — caller
 *        translates against the driver's gesture enum).
 *
 * @param[out] p_gesture : Receives the gesture id.
 *
 * @return WP_TOUCH_OK on success.
 */
wp_touch_status_t touch_get_gesture   (uint8_t  *const p_gesture);
//******************************* Functions *********************************//

#endif /* __BSP_WRAPPER_TOUCH_H__ */
