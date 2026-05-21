/******************************************************************************
 * @file cst816t_hal_test_task.c
 *
 * @par dependencies
 * - main.h  (TP_TINT_Pin / TP_TINT_GPIO_Port)
 * - i2c.h   (hi2c1)
 * - bsp_cst816t_driver.h
 * - bsp_cst816t_reg.h
 * - osal_wrapper_adapter.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief CST816T driver smoke test against real I2C1 + EXTI2 (PB2) hardware.
 *
 * Processing flow:
 * Mount HAL passthrough vtables on hi2c1 (CubeMX MX_I2C1_Init has already
 * run before the scheduler), wire a static ISR hook into the EXTI2 dispatch
 * in user_isr_handlers.c so PB2 falling/rising edges notify this task, then
 * loop-and-wait: on each notification read GestureID / FingerNum / X / Y and
 * route the result to RTT Terminal 6 (DEBUG_RTT_CH_TOUCH).  A periodic
 * fallback poll covers panels whose IRQ wiring is not yet stuffed.
 *
 * @note  CST816T IRQ is an active-low pulse (default 1ms).  PB2 is currently
 *        configured GPIO_MODE_IT_RISING with internal pull-up in gpio.c, so
 *        the EXTI fires on the trailing edge of each pulse — the touch data
 *        registers are stable by then so single-shot reads are safe.
 *
 * @version V1.0 2026-04-25
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "main.h"
#include "i2c.h"
#include "stm32f4xx_hal.h"

#include "user_task_reso_config.h"
#include "osal_wrapper_adapter.h"
#include "bsp_cst816t_driver.h"
#include "bsp_cst816t_reg.h"
#include "Debug.h"
//******************************** Includes *********************************//

/**
 * Whole-file gate. Default OFF — the task is never registered. Flip
 * USER_TASK_CST816T_HAL_TEST to 1 in user_task_reso_config.h to compile
 * the body. Note: this file also installs an EXTI2 ISR hook through
 * user_isr_handlers, so when the gate is OFF that hook stays unbound.
 */
#if USER_TASK_CST816T_HAL_TEST

//******************************** Defines **********************************//
#define CST816T_HAL_BOOT_WAIT_MS         1500u
#define CST816T_HAL_POLL_INTERVAL_MS      200u
#define CST816T_HAL_IIC_TIMEOUT_MS       1000u
/* configTICK_RATE_HZ == 1 kHz in this project, so 1 ms == 1 tick. */
#define CST816T_HAL_NOTIFY_TIMEOUT_TICKS  CST816T_HAL_POLL_INTERVAL_MS
#define CST816T_HAL_NOTIFY_BIT           0x01u
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
extern void (*pf_cst816t_pin_interrupt_callback)(void *, void *);
extern void  *g_cst816t_pin_interrupt_arg;

static bsp_cst816t_driver_t              s_hal_driver;
static cst816t_iic_driver_interface_t    s_hal_iic;
static cst816t_timebase_interface_t      s_hal_timebase;
static cst816t_delay_interface_t         s_hal_delay;
static cst816t_os_delay_interface_t      s_hal_os;
static void (*s_int_cb)(void *, void *)  = NULL;

static osal_task_handle_t                s_hal_task_handle = NULL;
static volatile uint32_t                 s_irq_count       = 0u;

/* Last-printed sample, used to suppress identical reads. CST816T's gesture
 * register is latched (not auto-cleared) so spurious EXTI bursts read the
 * same stale (gesture, x, y) tuple over and over — without dedup the log
 * fills with copies of the previous swipe. */
static uint8_t                           s_last_finger     = 0xFFu;
static cst816t_gesture_id_t              s_last_gesture    = (cst816t_gesture_id_t)0xFFu;
static uint16_t                          s_last_x          = 0xFFFFu;
static uint16_t                          s_last_y          = 0xFFFFu;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/* ---- I2C bus (HAL passthrough) ------------------------------------------ */
/**
 * @brief      Bus init shim — MX_I2C1_Init already ran from main().
 *
 * @return     CST816T_OK always.
 * */
static cst816t_status_t hal_iic_init(void const * const hi2c)
{
    (void)hi2c;
    return CST816T_OK;
}

/**
 * @brief      Bus deinit shim — leave the controller running.
 *
 * @return     CST816T_OK always.
 * */
static cst816t_status_t hal_iic_deinit(void const * const hi2c)
{
    (void)hi2c;
    return CST816T_OK;
}

/**
 * @brief      Forward register write to HAL_I2C_Mem_Write on hi2c1.
 *
 * @param[in]  i2c       : Bus handle (must be &hi2c1).
 *
 * @param[in]  des_addr  : 8-bit device address pre-shifted by the driver.
 *
 * @param[in]  mem_addr  : Target register.
 *
 * @param[in]  mem_size  : Memory address byte width (driver passes 1).
 *
 * @param[in]  p_data    : Payload buffer.
 *
 * @param[in]  size      : Payload length.
 *
 * @param[in]  timeout   : Wait timeout in ms.
 *
 * @return     CST816T_OK on HAL_OK, CST816T_ERROR otherwise.
 * */
static cst816t_status_t hal_iic_mem_write(void    *i2c,
                                          uint16_t des_addr,
                                          uint16_t mem_addr,
                                          uint16_t mem_size,
                                          uint8_t *p_data,
                                          uint16_t size,
                                          uint32_t timeout)
{
    (void)i2c;
    HAL_StatusTypeDef hs = HAL_I2C_Mem_Write(&hi2c1, des_addr, mem_addr,
                                             mem_size, p_data, size, timeout);
    return (HAL_OK == hs) ? CST816T_OK : CST816T_ERROR;
}

/**
 * @brief      Forward register read to HAL_I2C_Mem_Read on hi2c1.
 *
 * @return     CST816T_OK on HAL_OK, CST816T_ERROR otherwise.
 * */
static cst816t_status_t hal_iic_mem_read(void    *i2c,
                                         uint16_t des_addr,
                                         uint16_t mem_addr,
                                         uint16_t mem_size,
                                         uint8_t *p_data,
                                         uint16_t size,
                                         uint32_t timeout)
{
    (void)i2c;
    HAL_StatusTypeDef hs = HAL_I2C_Mem_Read(&hi2c1, des_addr, mem_addr,
                                            mem_size, p_data, size, timeout);
    return (HAL_OK == hs) ? CST816T_OK : CST816T_ERROR;
}

/* ---- Timebase / delay / OS yield ---------------------------------------- */
static uint32_t hal_tb_get_tick_count(void)
{
    return HAL_GetTick();
}

static void hal_delay_init(void)
{
    /* HAL tick is already running by the time this fires. */
}

static void hal_delay_ms(uint32_t const ms)
{
    /**
     * Use osal_task_delay so the scheduler can run other ready tasks during
     * the CST816T post-power-on settle window; HAL_Delay would busy-loop.
     **/
    osal_task_delay(ms);
}

static void hal_delay_us(uint32_t const us)
{
    /* Tight busy-loop around HAL_GetTick is too coarse for us-level delays;
     * the CST816T driver currently never calls this in production paths so
     * a no-op is acceptable for bring-up. */
    (void)us;
}

static void hal_os_yield(uint32_t const ms)
{
    osal_task_delay(ms);
}

/* ---- ISR hook ----------------------------------------------------------- */
/**
 * @brief      EXTI2 dispatch entry installed via user_isr_handlers.c.
 *
 * @param[in]  p_arg  : Task handle to notify (set during binding).
 *
 * @param[in]  p_data : Unused (HAL EXTI callback passes NULL).
 *
 * @return     None.
 *
 * @note       Runs in ISR context — must not touch the I2C bus.  The
 *             notified task drains the touch registers in thread context
 *             where the bus mutex (if used by a future driver) is safe.
 * */
static void hal_isr_hook(void *p_arg, void *p_data)
{
    (void)p_data;
    s_irq_count++;
    osal_task_handle_t handle = (osal_task_handle_t)p_arg;
    if (NULL == handle)
    {
        return;
    }
    osal_base_type_t hpw = 0;
    (void)osal_notify_send_from_isr(handle, CST816T_HAL_NOTIFY_BIT,
                                    OSAL_NOTIFY_SET_BITS, &hpw);
    /**
     * Skip portYIELD_FROM_ISR(hpw) here to match the existing project
     * convention (see mpu6050_integration.c) — the woken task runs on the
     * next 1 ms tick, which is well within the touch-event budget.
     **/
    (void)hpw;
}

/* ---- Wire-up ------------------------------------------------------------ */
/**
 * @brief      Bind HAL passthrough vtables and instance the touch driver.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t hal_driver_bind(void)
{
    /**
     * The driver caches hi2c by value but never dereferences it; the HAL
     * passthroughs above always go through the global hi2c1 instead.  We
     * still set it to a non-NULL sentinel so bsp_cst816t_inst's parameter
     * validation passes.
     **/
    s_hal_iic.hi2c              = (void *)&hi2c1;
    s_hal_iic.pf_iic_init       = hal_iic_init;
    s_hal_iic.pf_iic_deinit     = hal_iic_deinit;
    s_hal_iic.pf_iic_mem_write  = hal_iic_mem_write;
    s_hal_iic.pf_iic_mem_read   = hal_iic_mem_read;

    s_hal_timebase.pf_get_tick_count = hal_tb_get_tick_count;

    s_hal_delay.pf_delay_init   = hal_delay_init;
    s_hal_delay.pf_delay_ms     = hal_delay_ms;
    s_hal_delay.pf_delay_us     = hal_delay_us;

    s_hal_os.pf_rtos_yield      = hal_os_yield;

    return bsp_cst816t_inst(&s_hal_driver, &s_hal_iic, &s_hal_timebase,
                            &s_hal_delay, &s_hal_os, &s_int_cb);
}

/**
 * @brief      Decode and log a single touch sample (gesture + xy + finger).
 *
 * @param[in]  trigger : Free-form label naming the trigger (IRQ / poll).
 *
 * @return     None.
 * */
static void hal_read_and_report(const char *trigger)
{
    uint8_t              finger_num  = 0u;
    cst816t_gesture_id_t gesture     = NOGESTURE;
    cst816t_xy_t         xy          = {0u, 0u};

    cst816t_status_t ret = s_hal_driver.pf_cst816t_get_finger_num(
        s_hal_driver, &finger_num);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "hal get_finger_num failed (%s) ret=%d", trigger, (int)ret);
        return;
    }

    ret = s_hal_driver.pf_cst816t_get_gesture_id(s_hal_driver, &gesture);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "hal get_gesture_id failed (%s) ret=%d", trigger, (int)ret);
        return;
    }

    /**
     * The CST816T reports CLICK / DOUBLECLICK / LONGPRESS only on finger
     * release, when finger_num has already returned to 0.  Filtering on
     * "finger_num == 0" alone would silently drop every tap event — bail
     * only when both the touch and the gesture register read idle.
     **/
    if ((0u == finger_num) && (NOGESTURE == gesture))
    {
        return;
    }

    ret = s_hal_driver.pf_cst816t_get_xy_axis(s_hal_driver, &xy);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "hal get_xy_axis failed (%s) ret=%d", trigger, (int)ret);
        return;
    }

    /**
     * Suppress consecutive identical samples.  CST816T's gesture / X / Y
     * registers are latched — they hold the last decoded values until the
     * chip overwrites them on the next event.  Any spurious EXTI fired by
     * noise (pre-DISAUTOSLEEP scan, ESD, floating IRQ trace) re-reads the
     * exact same tuple, which we collapse into a single log line.
     **/
    if ((finger_num == s_last_finger) && (gesture == s_last_gesture) &&
        (xy.x_pos   == s_last_x)      && (xy.y_pos == s_last_y))
    {
        return;
    }
    s_last_finger  = finger_num;
    s_last_gesture = gesture;
    s_last_x       = xy.x_pos;
    s_last_y       = xy.y_pos;

    DEBUG_OUT(i, CST816T_LOG_TAG,
              "[%s] fingers=%u gesture=0x%02X x=%u y=%u (irq#=%u)",
              trigger, (unsigned)finger_num, (unsigned)gesture,
              (unsigned)xy.x_pos, (unsigned)xy.y_pos,
              (unsigned)s_irq_count);
}

/* ---- Task entry --------------------------------------------------------- */
/**
 * @brief      Touch HAL test task entry; runs until power-off.
 *
 * @param[in]  argument : Unused.
 *
 * @return     None.
 * */
void cst816t_hal_test_task(void *argument)
{
    (void)argument;

    osal_task_delay(CST816T_HAL_BOOT_WAIT_MS);

    DEBUG_OUT(i, CST816T_LOG_TAG, "cst816t_hal_test_task start (I2C1 + PB2)");

    s_hal_task_handle = osal_task_get_current_handle();
    if (NULL == s_hal_task_handle)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG, "task handle is NULL — aborting");
        for (;;)
        {
            osal_task_delay(1000u);
        }
    }

    cst816t_status_t bind_ret = hal_driver_bind();
    if (CST816T_OK != bind_ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "bsp_cst816t_inst failed = %d (HAL bind)", (int)bind_ret);
        for (;;)
        {
            osal_task_delay(1000u);
        }
    }

    /**
     * Probe the chip id explicitly so a wrong wiring shows up in the log
     * before we sit waiting for an interrupt that never comes.
     **/
    uint8_t chip_id = 0u;
    cst816t_status_t id_ret =
        s_hal_driver.pf_cst816t_get_chip_id(s_hal_driver, &chip_id);
    if (CST816T_OK != id_ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "hal probe chip_id failed = %d", (int)id_ret);
    }
    else
    {
        DEBUG_OUT(i, CST816T_LOG_TAG, "hal probe chip_id = 0x%02X",
                  (unsigned)chip_id);
    }

    /**
     * The chip's RAM survives SoC reset, so a stale touch report from the
     * previous session can keep IRQ asserted until somebody reads it out.
     * Drain GestureID/FingerNum/XY once before arming the ISR so the very
     * first interrupt corresponds to a real new touch event.
     **/
    {
        uint8_t              flush_finger  = 0u;
        cst816t_gesture_id_t flush_gesture = NOGESTURE;
        cst816t_xy_t         flush_xy      = {0u, 0u};
        (void)s_hal_driver.pf_cst816t_get_finger_num(s_hal_driver,
                                                     &flush_finger);
        (void)s_hal_driver.pf_cst816t_get_gesture_id(s_hal_driver,
                                                     &flush_gesture);
        (void)s_hal_driver.pf_cst816t_get_xy_axis(s_hal_driver, &flush_xy);
    }

    /**
     * Install our ISR hook last so an early spurious EXTI cannot fire into
     * an uninitialised driver.  user_isr_handlers' EXTI dispatch reads both
     * globals on every shot, so any ordering between them is fine as long
     * as both land before the first interrupt the user can produce.
     **/
    g_cst816t_pin_interrupt_arg            = (void *)s_hal_task_handle;
    pf_cst816t_pin_interrupt_callback      = hal_isr_hook;

    /* Clear any latched EXTI line from boot before unmasking arrived. */
    __HAL_GPIO_EXTI_CLEAR_IT(TP_TINT_Pin);

    DEBUG_OUT(i, CST816T_LOG_TAG,
              "cst816t hal test ready — touch the panel to see events");

    for (;;)
    {
        uint32_t notif = 0u;
        int32_t  wr    = osal_notify_wait(0u, CST816T_HAL_NOTIFY_BIT, &notif,
                                          CST816T_HAL_NOTIFY_TIMEOUT_TICKS);
        if ((0 == wr) && (0u != (notif & CST816T_HAL_NOTIFY_BIT)))
        {
            hal_read_and_report("IRQ");
        }
        else
        {
            /**
             * Timeout path: poll once so a panel whose INT line is left
             * floating still reports activity, and so a bus failure shows
             * up in RTT instead of silent dead-task.
             **/
            hal_read_and_report("POLL");
        }
    }
}
#endif /* USER_TASK_CST816T_HAL_TEST */
//******************************* Functions *********************************//
