/******************************************************************************
 * @file bsp_adapter_port_touch.c
 *
 * @par dependencies
 * - bsp_adapter_port_touch.h
 * - bsp_wrapper_touch.h
 * - bsp_cst816t_driver.h
 * - cst816t_integration.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief Adapter port for the CST816T capacitive touch controller.
 *
 * Owns a single static bsp_cst816t_driver_t instance and exposes the
 * bsp_wrapper_touch vtable that forwards each abstract API to the matching
 * CST816T driver function.  Stays bus-side polled — interrupt wiring is
 * intentionally unused here because LVGL's indev port polls every 10 ms.
 *
 * Bring-up is split so OSAL-dependent setup never runs before
 * osKernelStart():
 *
 *   drv_adapter_touch_register()      -- pre-kernel, vtable mount only
 *   touch_drv_inst() (wrapper API)    -- post-kernel, in-task
 *      └─ pf_touch_drv_inst (vtable)
 *           └─ touch_drv_inst_adapter (this file)
 *                └─ bsp_cst816t_inst(&s_cst816t_drv, cst816t_input_arg.*)
 *
 * @version V1.0 2026-04-26
 * @version V2.0 2026-04-28
 * @version V3.0 2026-04-28
 * @version V4.0 2026-04-28
 * @upgrade 2.0: Split bring-up — register() is pure vtable mount, inst()
 *               drives bsp_cst816t_inst() from task context.
 * @upgrade 3.0: inst hook moved into the wrapper vtable; consumers reach
 *               it through touch_drv_inst() instead of a public adapter
 *               entry point.  drv_adapter_touch_inst() removed.
 * @upgrade 4.0: Added ISR notify hook (pf_touch_isr_notify) — invokes
 *               the driver's internal interrupt callback that was
 *               captured into s_int_callback at inst time.  Adapter still
 *               always passes &s_int_callback to bsp_cst816t_inst() so
 *               the driver's writeback never sees a NULL slot; whether
 *               an EXTI handler ever calls touch_drv_isr_notify() is the
 *               application's choice (polling-by-default is fine).
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "bsp_adapter_port_touch.h"
#include "bsp_wrapper_touch.h"
#include "bsp_cst816t_driver.h"
#include "cst816t_integration.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static bsp_cst816t_driver_t s_cst816t_drv;

/**
 * Out-param for bsp_cst816t_inst().  The driver writes the address of
 * its internal interrupt callback here at inst time.  Always passed as
 * &s_int_callback so the driver's writeback never targets NULL.
 *
 * Reachable from an EXTI ISR via touch_drv_isr_notify() →
 * pf_touch_isr_notify → touch_isr_notify_adapter().  If no EXTI handler
 * is wired (the default), this slot is captured but never invoked,
 * leaving the controller in pure polling mode.
 */
static void (*s_int_callback)(void *, void *) = NULL;

/* True once bsp_cst816t_inst() has populated s_cst816t_drv successfully. */
static bool s_inst_ok = false;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/* ---- vtable hooks -------------------------------------------------------- */
/**
 * @brief Instantiate the CST816T driver from cst816t_input_arg.
 *
 * Wrapper-vtable hook reached via touch_drv_inst().  Must run from task
 * context (post-kernel) — cst816t_input_arg.p_os_interface and the I2C
 * bus mutex are only valid after osKernelStart().  Idempotent.
 */
static wp_touch_status_t touch_drv_inst_adapter(
    struct _touch_drv_t *const dev)
{
    (void)dev;
    if (s_inst_ok)
    {
        return WP_TOUCH_OK;
    }

    cst816t_status_t st = bsp_cst816t_inst(&s_cst816t_drv,
                                           cst816t_input_arg.p_iic_interface,
                                           cst816t_input_arg.p_timebase_interface,
                                           cst816t_input_arg.p_delay_interface,
                                           cst816t_input_arg.p_os_interface,
                                           &s_int_callback);
    if (CST816T_OK != st)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "touch_drv_inst_adapter: cst816t inst failed = %d",
                  (int)st);
        s_inst_ok = false;
        return WP_TOUCH_ERROR;
    }

    s_inst_ok = true;
    return WP_TOUCH_OK;
}

/**
 * @brief Forward init request to the CST816T driver.
 *
 * @param[in] dev : Wrapper driver slot (unused; adapter owns the instance).
 */
static void touch_drv_init_adapter(struct _touch_drv_t *const dev)
{
    (void)dev;
    if (s_inst_ok && NULL != s_cst816t_drv.pf_cst816t_init)
    {
        (void)s_cst816t_drv.pf_cst816t_init(s_cst816t_drv);
    }
}

/**
 * @brief Dispatch an EXTI ISR notification to the CST816T driver.
 *
 * Wrapper-vtable hook reached via touch_drv_isr_notify().  Runs in
 * interrupt context — must stay ISR-safe (no blocking, no mutex).  The
 * driver's own callback is responsible for kicking a handler task via
 * osal_notify rather than touching the I2C bus from here.
 *
 * Silently does nothing if inst hasn't run yet or the driver did not
 * publish an interrupt callback.
 */
static void touch_isr_notify_adapter(struct _touch_drv_t *const dev)
{
    (void)dev;
    if (s_inst_ok && NULL != s_int_callback)
    {
        s_int_callback(NULL, NULL);
    }
}

/**
 * @brief Forward deinit request to the CST816T driver.
 */
static void touch_drv_deinit_adapter(struct _touch_drv_t *const dev)
{
    (void)dev;
    if (s_inst_ok && NULL != s_cst816t_drv.pf_cst816t_deinit)
    {
        (void)s_cst816t_drv.pf_cst816t_deinit(&s_cst816t_drv);
    }
}

/**
 * @brief Read the current finger count from the CST816T.
 */
static wp_touch_status_t touch_get_finger_num_adapter(
    struct _touch_drv_t *const dev, uint8_t *const p_finger)
{
    (void)dev;
    if (!s_inst_ok || NULL == s_cst816t_drv.pf_cst816t_get_finger_num)
    {
        return WP_TOUCH_ERRORRESOURCE;
    }
    if (NULL == p_finger)
    {
        return WP_TOUCH_ERRORPARAMETER;
    }
    cst816t_status_t st =
        s_cst816t_drv.pf_cst816t_get_finger_num(s_cst816t_drv, p_finger);
    return (CST816T_OK == st) ? WP_TOUCH_OK : (wp_touch_status_t)st;
}

/**
 * @brief Read the current touch XY coordinate.
 */
static wp_touch_status_t touch_get_xy_adapter(
    struct _touch_drv_t *const dev, uint16_t *const p_x, uint16_t *const p_y)
{
    (void)dev;
    if (!s_inst_ok || NULL == s_cst816t_drv.pf_cst816t_get_xy_axis)
    {
        return WP_TOUCH_ERRORRESOURCE;
    }
    if (NULL == p_x || NULL == p_y)
    {
        return WP_TOUCH_ERRORPARAMETER;
    }

    cst816t_xy_t xy = {0u, 0u};
    cst816t_status_t st =
        s_cst816t_drv.pf_cst816t_get_xy_axis(s_cst816t_drv, &xy);
    if (CST816T_OK != st)
    {
        return (wp_touch_status_t)st;
    }
    *p_x = xy.x_pos;
    *p_y = xy.y_pos;
    return WP_TOUCH_OK;
}

/**
 * @brief Probe the CST816T chip id.
 */
static wp_touch_status_t touch_get_chip_id_adapter(
    struct _touch_drv_t *const dev, uint8_t *const p_chip_id)
{
    (void)dev;
    if (!s_inst_ok || NULL == s_cst816t_drv.pf_cst816t_get_chip_id)
    {
        return WP_TOUCH_ERRORRESOURCE;
    }
    if (NULL == p_chip_id)
    {
        return WP_TOUCH_ERRORPARAMETER;
    }
    cst816t_status_t st =
        s_cst816t_drv.pf_cst816t_get_chip_id(s_cst816t_drv, p_chip_id);
    return (CST816T_OK == st) ? WP_TOUCH_OK : (wp_touch_status_t)st;
}

/**
 * @brief Read the latest gesture id (CST816T-defined encoding).
 */
static wp_touch_status_t touch_get_gesture_adapter(
    struct _touch_drv_t *const dev, uint8_t *const p_gesture)
{
    (void)dev;
    if (!s_inst_ok || NULL == s_cst816t_drv.pf_cst816t_get_gesture_id)
    {
        return WP_TOUCH_ERRORRESOURCE;
    }
    if (NULL == p_gesture)
    {
        return WP_TOUCH_ERRORPARAMETER;
    }

    cst816t_gesture_id_t gesture = NOGESTURE;
    cst816t_status_t st =
        s_cst816t_drv.pf_cst816t_get_gesture_id(s_cst816t_drv, &gesture);
    if (CST816T_OK != st)
    {
        return (wp_touch_status_t)st;
    }
    *p_gesture = (uint8_t)gesture;
    return WP_TOUCH_OK;
}

/* ---- Wrapper vtable registration ----------------------------------------- */
/**
 * @brief Mount the adapter vtable into bsp_wrapper_touch.
 *
 * Pre-kernel safe: touches no hardware and no OSAL primitive.  The driver
 * instance behind the vtable stays uninstantiated until
 * drv_adapter_touch_inst() runs; wrapper calls return
 * WP_TOUCH_ERRORRESOURCE until then.
 */
void drv_adapter_touch_register(void)
{
    touch_drv_t touch_drv = {
        .idx                      = 0,
        .dev_id                   = 0,
        .user_data                = NULL,
        .pf_touch_drv_inst        = touch_drv_inst_adapter,
        .pf_touch_drv_init        = touch_drv_init_adapter,
        .pf_touch_drv_deinit      = touch_drv_deinit_adapter,
        .pf_touch_isr_notify      = touch_isr_notify_adapter,
        .pf_touch_get_finger_num  = touch_get_finger_num_adapter,
        .pf_touch_get_xy          = touch_get_xy_adapter,
        .pf_touch_get_chip_id     = touch_get_chip_id_adapter,
        .pf_touch_get_gesture     = touch_get_gesture_adapter,
    };

    (void)drv_adapter_touch_mount(0u, &touch_drv);
}
//******************************* Functions *********************************//
