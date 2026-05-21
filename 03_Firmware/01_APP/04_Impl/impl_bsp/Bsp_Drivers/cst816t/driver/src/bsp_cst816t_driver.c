/******************************************************************************
 * @file       bsp_cst816t_driver.c
 *
 * @par        dependencies
 * - bsp_cst816t_driver.h
 * - bsp_cst816t_reg.h
 * - Debug.h
 *
 * @author     Ethan-Hang
 *
 * @brief      CST816T capacitive touch controller driver implementation.
 *
 * Processing flow:
 * - Bind external interfaces via instantiation helper.
 * - Initialize bus, verify chip id, configure gesture/motion defaults.
 * - Provide register helpers for coordinate/gesture access and power ctrl.
 *
 * @version    V1.0 2026-04-25
 *
 * @note       1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "bsp_cst816t_driver.h"
#include "bsp_cst816t_reg.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* CST816T 7-bit I2C slave address; HAL mem APIs expect it left-shifted once. */
#define CST816T_ADDR                            0x15u
/* Known CST816T chip-id variants observed in the field.  Hynitron has
 * shipped at least 0xB4 / 0xB5 / 0xB6 for what is otherwise the same die
 * (different lot / packaging trim).  All three respond to the same
 * register map, so we treat any of them as a successful identification. */
#define CST816T_CHIP_ID_B4                      0xB4u
#define CST816T_CHIP_ID_B5                      0xB5u
#define CST816T_CHIP_ID_B6                      0xB6u
#define CST816T_IIC_TIMEOUT_MS                  1000u
#define CST816T_IIC_MEM_SIZE_8BIT               0x00000001u
#define CST816T_RESET_PULSE_MS                  10u
#define CST816T_BOOT_DELAY_MS                   50u
#define CST816T_SLEEP_CMD                       0x03u

/* Default device-side configuration applied during init.
 * EN_CHANGE intentionally NOT set: in low-power scan, capacitive noise /
 * ESD across LpScanTH triggers spurious "presence" pulses that flood EXTI
 * with stale-data IRQs.  EN_MOTION only fires on a gesture the chip's
 * decoder confirmed (CLICK/DCLICK/swipe/long-press), which filters that
 * noise.  Apps that need raw press/release should OR EN_CHANGE in via
 * pf_cst816t_set_irq_ctl after init AND keep auto-sleep disabled.
 * ONCE_WLP keeps long-press at one pulse instead of a periodic train. */
#define CST816T_DEFAULT_IRQ_CTL                 (EN_MOTION | ONCE_WLP)
#define CST816T_DEFAULT_MOTION_MASK             (MOTION_ALLENABLE)
#define CST816T_DEFAULT_IRQ_PULSE_WIDTH         10u    /* 0.1ms * 10 = 1ms   */
#define CST816T_DEFAULT_NOR_SCAN_PER            1u     /* 10ms * 1           */
#define CST816T_DEFAULT_LONG_PRESS_TICK         100u   /* ~1s                */
#define CST816T_DEFAULT_AUTO_SLEEP_TIME         2u     /* 2s no-touch sleep  */
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static bsp_cst816t_driver_t *gs_p_active_instance = NULL;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief      Yield current task (prefer OS yield, fall back to busy delay).
 *
 * @param[in]  p_os    : Optional OS yield interface (may be NULL).
 *
 * @param[in]  p_delay : Optional blocking delay interface (may be NULL).
 *
 * @param[in]  ms      : Wait duration in milliseconds.
 *
 * @param[out]         : None.
 *
 * @return     None.
 * */
static inline void cst816t_yield_ms(
    cst816t_os_delay_interface_t const * const p_os,
    cst816t_delay_interface_t    const * const p_delay,
    uint32_t                             const ms)
{
    /**
     * OS path is preferred when available so the thread does not monopolise
     * the CPU while the touch controller finishes its internal reset.
     **/
    if ((p_os != NULL) && (p_os->pf_rtos_yield != NULL))
    {
        p_os->pf_rtos_yield(ms);
        return;
    }
    if ((p_delay != NULL) && (p_delay->pf_delay_ms != NULL))
    {
        p_delay->pf_delay_ms(ms);
    }
}

/**
 * @brief      Write a single register via the mounted I2C interface.
 *
 * @param[in]  p_iic   : I2C interface bound to the driver instance.
 *
 * @param[in]  reg     : Target register address (8-bit).
 *
 * @param[in]  p_data  : Payload buffer.
 *
 * @param[in]  len     : Payload length in bytes.
 *
 * @param[out]         : None.
 *
 * @return     cst816t_status_t I2C write status code.
 * */
static inline cst816t_status_t cst816t_write_reg(
    cst816t_iic_driver_interface_t const * const p_iic,
    uint16_t                             const reg,
    uint8_t                                  * const p_data,
    uint16_t                             const len)
{
    /**
     * HAL mem APIs expect the 8-bit address; bit0 selects the R/W direction.
     **/
    return p_iic->pf_iic_mem_write(
        p_iic->hi2c,
        (CST816T_ADDR << 1) | 0,
        reg,
        CST816T_IIC_MEM_SIZE_8BIT,
        p_data,
        len,
        CST816T_IIC_TIMEOUT_MS);
}

/**
 * @brief      Read one or more consecutive registers via I2C.
 *
 * @param[in]  p_iic   : I2C interface bound to the driver instance.
 *
 * @param[in]  reg     : Starting register address (8-bit).
 *
 * @param[out] p_data  : Buffer receiving the read payload.
 *
 * @param[in]  len     : Number of bytes to read.
 *
 * @return     cst816t_status_t I2C read status code.
 * */
static inline cst816t_status_t cst816t_read_reg(
    cst816t_iic_driver_interface_t const * const p_iic,
    uint16_t                             const reg,
    uint8_t                                  * const p_data,
    uint16_t                             const len)
{
    return p_iic->pf_iic_mem_read(
        p_iic->hi2c,
        (CST816T_ADDR << 1) | 1,
        reg,
        CST816T_IIC_MEM_SIZE_8BIT,
        p_data,
        len,
        CST816T_IIC_TIMEOUT_MS);
}

/**
 * @brief      Write a single byte to a register (convenience wrapper).
 *
 * @param[in]  p_iic : I2C interface bound to the driver instance.
 *
 * @param[in]  reg   : Target register address.
 *
 * @param[in]  value : Byte payload.
 *
 * @return     cst816t_status_t I2C write status code.
 * */
static inline cst816t_status_t cst816t_write_byte(
    cst816t_iic_driver_interface_t const * const p_iic,
    uint16_t                             const reg,
    uint8_t                              const value)
{
    uint8_t data = value;
    return cst816t_write_reg(p_iic, reg, &data, 1);
}

/**
 * @brief      Read the current gesture code reported by the controller.
 *
 * @param[in]  this         : Driver instance (passed by value per header).
 *
 * @param[out] p_gesture_id : Decoded gesture id output.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_get_gesture_id(
    bsp_cst816t_driver_t                 const this,
    cst816t_gesture_id_t               * const p_gesture_id)
{
    /* 1. validate external inputs */
    if ((p_gesture_id == NULL) || (this.p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t get gesture id input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    /* 2. read register GestureID(0x01) */
    uint8_t raw = 0;
    cst816t_status_t ret = cst816t_read_reg(this.p_iic_driver_instance,
                                            GESTURE_ID, &raw, 1);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t read GESTURE_ID register failed!");
        return ret;
    }

    /* 3. cast to enum (only low byte is meaningful) */
    *p_gesture_id = (cst816t_gesture_id_t)raw;
    return ret;
}

/**
 * @brief      Read X/Y touch coordinate pair.
 *
 * @param[in]  this      : Driver instance (by value).
 *
 * @param[out] p_xy_axis : Output coordinate pair (12-bit per axis).
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_get_xy_axis(
    bsp_cst816t_driver_t                 const this,
    cst816t_xy_t                       * const p_xy_axis)
{
    /* 1. validate external inputs */
    if ((p_xy_axis == NULL) || (this.p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t get xy axis input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    /**
     * Burst-read 4 bytes: XposH, XposL, YposH, YposL starting at 0x03.
     * Only the low nibble of XposH/YposH carries coordinate bits [11:8].
     **/
    uint8_t buf[4] = {0};
    cst816t_status_t ret = cst816t_read_reg(this.p_iic_driver_instance,
                                            X_POSH, buf, 4);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t read XY registers failed!");
        return ret;
    }

    /* 2. decompose 12-bit coordinates */
    p_xy_axis->x_pos = (uint16_t)(((uint16_t)(buf[0] & 0x0Fu) << 8) | buf[1]);
    p_xy_axis->y_pos = (uint16_t)(((uint16_t)(buf[2] & 0x0Fu) << 8) | buf[3]);
    return ret;
}

/**
 * @brief      Read the chip id register for device identification.
 *
 * @param[in]  this      : Driver instance (by value).
 *
 * @param[out] p_chip_id : Output chip id byte.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_get_chip_id(
    bsp_cst816t_driver_t                 const this,
    uint8_t                            * const p_chip_id)
{
    if ((p_chip_id == NULL) || (this.p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t get chip id input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_read_reg(this.p_iic_driver_instance,
                                            CHIPID, p_chip_id, 1);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t read CHIPID register failed!");
    }
    return ret;
}

/**
 * @brief      Read the current finger count (0 = idle, 1 = single touch).
 *
 * @param[in]  this         : Driver instance (by value).
 *
 * @param[out] p_finger_num : Output finger count.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_get_finger_num(
    bsp_cst816t_driver_t                 const this,
    uint8_t                            * const p_finger_num)
{
    if ((p_finger_num == NULL) || (this.p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t get finger num input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_read_reg(this.p_iic_driver_instance,
                                            FINGER_NUM, p_finger_num, 1);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t read FINGER_NUM register failed!");
    }
    return ret;
}

/**
 * @brief      Put the controller into deep sleep (no touch wakeup).
 *
 * @param[in]  this : Driver instance (by value).
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_sleep(bsp_cst816t_driver_t const this)
{
    if (this.p_iic_driver_instance == NULL)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t sleep input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(this.p_iic_driver_instance,
                                              SLEEPMODE, CST816T_SLEEP_CMD);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write SLEEPMODE register failed!");
    }
    return ret;
}

/**
 * @brief      Wake the controller by poking a non-sleep register.
 *
 * @param[in]  this : Driver instance (by value).
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_wakeup(bsp_cst816t_driver_t const this)
{
    if (this.p_iic_driver_instance == NULL)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t wakeup input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    /**
     * CST816T has no dedicated wakeup command. Writing a benign value to
     * the DisAutoSleep register both clears the sleep state and disables
     * auto-sleep until the application re-enables it.
     **/
    cst816t_status_t ret = cst816t_write_byte(this.p_iic_driver_instance,
                                              DISAUTOALEEP, 0x01u);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write DISAUTOALEEP register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the error-recovery reset modes (0xEA).
 *
 * @param[in]  this          : Driver instance (by value).
 *
 * @param[in]  err_reset_ctl : Reset mode (digital vs double-finger).
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_err_reset_ctl(
    bsp_cst816t_driver_t                 const this,
    cst816_err_reset_ctl_t                     err_reset_ctl)
{
    if (this.p_iic_driver_instance == NULL)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set err reset ctl input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(this.p_iic_driver_instance,
                                              ERRRESETCTL,
                                              (uint8_t)err_reset_ctl);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write ERRRESETCTL register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the long-press detection threshold (0xEB).
 *
 * @param[in]  this            : Driver instance (by value).
 *
 * @param[in]  long_press_tick : Ticks (default 100 ≈ 1s).
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_long_press_tick(
    bsp_cst816t_driver_t                 const this,
    uint8_t                                    long_press_tick)
{
    if (this.p_iic_driver_instance == NULL)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set long press tick input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(this.p_iic_driver_instance,
                                              LONGPRESSTICK, long_press_tick);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write LONGPRESSTICK register failed!");
    }
    return ret;
}

/**
 * @brief      Enable continuous swipe/double-click gesture detection (0xEC).
 *
 * @param[in]  this        : Driver instance (by value).
 *
 * @param[in]  motion_mask : Motion feature bitmask.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_motion_mask(
    bsp_cst816t_driver_t                 const this,
    cst816_motion_mask_t                       motion_mask)
{
    if (this.p_iic_driver_instance == NULL)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set motion mask input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(this.p_iic_driver_instance,
                                              MOTIONMASK, (uint8_t)motion_mask);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write MOTIONMASK register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the IRQ low-pulse output width (0xED).
 *
 * @param[in]  this            : Driver instance (by value).
 *
 * @param[in]  irq_pulse_width : Width in 0.1ms units, range 1..200.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_irq_pulse_width(
    bsp_cst816t_driver_t                 const this,
    uint8_t                                    irq_pulse_width)
{
    if (this.p_iic_driver_instance == NULL)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set irq pulse width input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(this.p_iic_driver_instance,
                                              IRQPLUSEWIDTH, irq_pulse_width);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write IRQPLUSEWIDTH register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the normal fast-scan period (0xEE).
 *
 * @param[in]  p_this      : Driver instance pointer.
 *
 * @param[in]  scan_period : Period in 10ms units, range 1..30.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_nor_scan_per(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    scan_period)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set nor scan per input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              NORSCANPER, scan_period);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write NORSCANPER register failed!");
    }
    return ret;
}

/**
 * @brief      Set the motion slope angle threshold (0xEF).
 *
 * @param[in]  p_this             : Driver instance pointer.
 *
 * @param[in]  x_right_y_up_angle : Slope angle value.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_motion_slope_angle(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    x_right_y_up_angle)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set motion slope angle input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              MOTIONSLANGLE,
                                              x_right_y_up_angle);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write MOTIONSLANGLE register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the low-power auto re-calibration period (0xF4).
 *
 * @param[in]  p_this : Driver instance pointer.
 *
 * @param[in]  time   : Calibration period value.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_low_power_auto_wake_time(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    time)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set lp auto wake time input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              LPAUTOWAKETIME, time);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write LPAUTOWAKETIME register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the low-power wakeup threshold (0xF5).
 *
 * @param[in]  p_this    : Driver instance pointer.
 *
 * @param[in]  threshold : Wakeup threshold.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_lp_scan_th(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    threshold)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set lp scan th input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              LPSCANTH, threshold);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write LPSCANTH register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the low-power scan range/window (0xF6).
 *
 * @param[in]  p_this : Driver instance pointer.
 *
 * @param[in]  window : Scan window value (0..3, 3 is default).
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_lp_scan_win(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    window)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set lp scan win input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              LPSCANWIN, window);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write LPSCANWIN register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the low-power scan frequency (0xF7).
 *
 * @param[in]  p_this    : Driver instance pointer.
 *
 * @param[in]  frequency : Scan frequency (1..255, default 7).
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_lp_scan_freq(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    frequency)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set lp scan freq input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              LPSCANFREQ, frequency);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write LPSCANFREQ register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the low-power scan IDAC current (0xF8).
 *
 * @param[in]  p_this : Driver instance pointer.
 *
 * @param[in]  idac   : Scan current value.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_lp_scan_idac(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    idac)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set lp scan idac input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              LPSCANIDAC, idac);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write LPSCANIDAC register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the idle time before auto-sleep (0xF9).
 *
 * @param[in]  p_this : Driver instance pointer.
 *
 * @param[in]  time   : Idle time threshold in seconds.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_auto_sleep_time(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    time)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set auto sleep time input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              AUTOSLEEPTIME, time);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write AUTOSLEEPTIME register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the IRQ trigger source mask (0xFA).
 *
 * @param[in]  p_this : Driver instance pointer.
 *
 * @param[in]  mode   : IRQ enable bitmask.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_irq_ctl(
    bsp_cst816t_driver_t               * const p_this,
    cst816_irq_ctl_t                           mode)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set irq ctl input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              IRQCTL, (uint8_t)mode);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write IRQCTL register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the debounce-based auto-reset timer (0xFB).
 *
 * @param[in]  p_this : Driver instance pointer.
 *
 * @param[in]  time   : Debounce seconds before auto-reset (0 disables).
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_auto_reset(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    time)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set auto reset input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              AUTORESET, time);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write AUTORESET register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the long-press auto-reset timer (0xFC).
 *
 * @param[in]  p_this : Driver instance pointer.
 *
 * @param[in]  time   : Long-press auto-reset seconds.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_long_press_time(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    time)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set long press time input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              LONGPRESSTIME, time);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write LONGPRESSTIME register failed!");
    }
    return ret;
}

/**
 * @brief      Configure the IO-level and soft-reset flags (0xFD).
 *
 * @param[in]  p_this : Driver instance pointer.
 *
 * @param[in]  mode   : IO control bitmask.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_set_io_ctl(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    mode)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t set io ctl input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              IOCTL, mode);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write IOCTL register failed!");
    }
    return ret;
}

/**
 * @brief      Disable automatic entry into low-power mode (0xFE).
 *
 * @param[in]  p_this  : Driver instance pointer.
 *
 * @param[in]  disable : Non-zero disables auto-sleep, 0 enables default.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_disable_auto_sleep(
    bsp_cst816t_driver_t               * const p_this,
    uint8_t                                    disable)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t disable auto sleep input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = cst816t_write_byte(p_this->p_iic_driver_instance,
                                              DISAUTOALEEP, disable);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write DISAUTOALEEP register failed!");
    }
    return ret;
}

/**
 * @brief      Deinitialize the driver and release the underlying bus.
 *
 * @param[in]  p_this : Driver instance pointer.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_deinit(bsp_cst816t_driver_t * const p_this)
{
    if ((p_this == NULL) || (p_this->p_iic_driver_instance == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t deinit input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    cst816t_status_t ret = CST816T_OK;
    if (p_this->p_iic_driver_instance->pf_iic_deinit != NULL)
    {
        ret = p_this->p_iic_driver_instance->pf_iic_deinit(
                  p_this->p_iic_driver_instance->hi2c);
    }
    if (gs_p_active_instance == p_this)
    {
        gs_p_active_instance = NULL;
    }
    return ret;
}

/**
 * @brief      Initialize the controller (boot delay, chip id, default regs).
 *
 * @param[in]  this : Driver instance (by value, as declared in header).
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t cst816t_init(bsp_cst816t_driver_t const this)
{
    /* 1. validate external inputs */
    if ((this.p_iic_driver_instance == NULL)                         ||
        (this.p_iic_driver_instance->pf_iic_init      == NULL)       ||
        (this.p_iic_driver_instance->pf_iic_mem_read  == NULL)       ||
        (this.p_iic_driver_instance->pf_iic_mem_write == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t init input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    /* 2. initialize I2C bus */
    cst816t_status_t ret = this.p_iic_driver_instance->pf_iic_init(
                               this.p_iic_driver_instance->hi2c);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG, "cst816t iic init failed!");
        return ret;
    }

    /**
     * CST816T needs a short post-power-on settle window before the first
     * I2C transaction responds. Empirically >=50ms is enough.
     **/
    cst816t_yield_ms(this.p_os_instance, this.p_delay_instance,
                     CST816T_BOOT_DELAY_MS);

    /* 3. probe chip id — the only reliable presence check on CST816T */
    uint8_t chip_id = 0;
    ret = cst816t_read_reg(this.p_iic_driver_instance,
                           CHIPID, &chip_id, 1);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t read CHIPID register failed!");
        return ret;
    }
    if ((CST816T_CHIP_ID_B4 == chip_id) ||
        (CST816T_CHIP_ID_B5 == chip_id) ||
        (CST816T_CHIP_ID_B6 == chip_id))
    {
        DEBUG_OUT(i, CST816T_LOG_TAG,
                  "cst816t chip id ok: 0x%02X", chip_id);
    }
    else
    {
        /**
         * Continue regardless — the register map is identical across
         * shipping variants and a future trim may pick a new id; bailing
         * here would brick boards that are otherwise fully functional.
         * Just surface the unknown id loudly so it can be added to the
         * accepted list above if it becomes common.
         **/
        DEBUG_OUT(w, CST816T_LOG_TAG,
                  "cst816t unknown chip id 0x%02X (continuing)", chip_id);
    }

    /* 4. apply default device configuration */
    ret = cst816t_write_byte(this.p_iic_driver_instance,
                             IRQPLUSEWIDTH, CST816T_DEFAULT_IRQ_PULSE_WIDTH);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write IRQPLUSEWIDTH register failed!");
        return ret;
    }
    ret = cst816t_write_byte(this.p_iic_driver_instance,
                             NORSCANPER, CST816T_DEFAULT_NOR_SCAN_PER);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write NORSCANPER register failed!");
        return ret;
    }
    ret = cst816t_write_byte(this.p_iic_driver_instance,
                             LONGPRESSTICK, CST816T_DEFAULT_LONG_PRESS_TICK);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write LONGPRESSTICK register failed!");
        return ret;
    }
    ret = cst816t_write_byte(this.p_iic_driver_instance,
                             MOTIONMASK, CST816T_DEFAULT_MOTION_MASK);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write MOTIONMASK register failed!");
        return ret;
    }
    ret = cst816t_write_byte(this.p_iic_driver_instance,
                             AUTOSLEEPTIME, CST816T_DEFAULT_AUTO_SLEEP_TIME);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write AUTOSLEEPTIME register failed!");
        return ret;
    }
    ret = cst816t_write_byte(this.p_iic_driver_instance,
                             IRQCTL, CST816T_DEFAULT_IRQ_CTL);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write IRQCTL register failed!");
        return ret;
    }

    /**
     * Disable auto-sleep so the chip stays in active scan.  In low-power
     * scan the LpScanTH-based wake path produces false-positive presence
     * pulses on capacitive noise, which the host sees as a constant IRQ
     * stream of stale-data reads.  Apps that need power saving should
     * re-enable auto-sleep with pf_cst816t_disable_auto_sleep(0) when the
     * UI goes idle, paired with a higher LpScanTH.
     **/
    ret = cst816t_write_byte(this.p_iic_driver_instance, DISAUTOALEEP, 0x01u);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t write DISAUTOALEEP register failed!");
        return ret;
    }

    return ret;
}

/**
 * @brief      External interrupt hook exposed to the caller via pp_int_callback.
 *
 * @param[in]  p_this  : Driver instance (opaque pointer).
 *
 * @param[out] p_data  : Optional context buffer (unused in bare-metal path).
 *
 * @return     None.
 * */
static void cst816t_int_interrupt_callback(void *p_this, void *p_data)
{
    (void)p_data;

    /**
     * The CST816T IRQ is level-active during a touch event. Reading X/Y
     * registers in ISR is unsafe when the bus requires a mutex (see the
     * project's ISR rule); the caller should forward this notification to
     * a handler thread. We keep the hook minimal on purpose.
     **/
    if (p_this == NULL)
    {
        if (gs_p_active_instance != NULL)
        {
            DEBUG_OUT(w, CST816T_LOG_TAG,
                      "cst816t int callback invoked without instance arg");
        }
        return;
    }
    (void)p_this;
}

/**
 * @brief      Instantiate the driver object and bind external interfaces.
 *
 * @param[in]  p_instance            : Driver object to populate.
 *
 * @param[in]  p_iic_driver_instance : I2C interface implementation.
 *
 * @param[in]  p_timebase_instance   : Monotonic tick provider.
 *
 * @param[in]  p_delay_instance      : Blocking delay provider (ms/us).
 *
 * @param[in]  p_os_instance         : Optional OS yield provider.
 *
 * @param[out] pp_int_callback       : Receives the interrupt hook address.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
cst816t_status_t bsp_cst816t_inst(
    bsp_cst816t_driver_t                      * const               p_instance,
    cst816t_iic_driver_interface_t      const * const    p_iic_driver_instance,
    cst816t_timebase_interface_t        const * const      p_timebase_instance,
    cst816t_delay_interface_t           const * const         p_delay_instance,
    cst816t_os_delay_interface_t        const * const            p_os_instance,
    void (**pp_int_callback)(void *, void*)
)
{
    /* 1. validate external inputs */
    if ((p_instance            == NULL)                              ||
        (p_iic_driver_instance == NULL)                              ||
        (p_timebase_instance   == NULL)                              ||
        (p_delay_instance      == NULL)                              ||
        (pp_int_callback       == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t inst input error parameter!");
        return CST816T_ERRORPARAMETER;
    }

    /* 2. validate mandatory I2C interface members */
    if ((p_iic_driver_instance->hi2c             == NULL)            ||
        (p_iic_driver_instance->pf_iic_init      == NULL)            ||
        (p_iic_driver_instance->pf_iic_mem_read  == NULL)            ||
        (p_iic_driver_instance->pf_iic_mem_write == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t iic driver interface missing ops!");
        return CST816T_ERRORPARAMETER;
    }

    /* 3. validate timebase/delay providers */
    if (p_timebase_instance->pf_get_tick_count == NULL)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t timebase interface missing ops!");
        return CST816T_ERRORPARAMETER;
    }
    if ((p_delay_instance->pf_delay_ms == NULL) ||
        (p_delay_instance->pf_delay_us == NULL))
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t delay interface missing ops!");
        return CST816T_ERRORPARAMETER;
    }

    /* 4. mount external interfaces */
    p_instance->p_iic_driver_instance              =     p_iic_driver_instance;
    p_instance->p_timebase_instance                =       p_timebase_instance;
    p_instance->p_delay_instance                   =          p_delay_instance;
    p_instance->p_os_instance                      =             p_os_instance;

    /* 5. mount internal operations */
    p_instance->pf_cst816t_init                    =              cst816t_init;
    p_instance->pf_cst816t_deinit                  =            cst816t_deinit;
    p_instance->pf_cst816t_get_gesture_id          =    cst816t_get_gesture_id;
    p_instance->pf_cst816t_get_xy_axis             =       cst816t_get_xy_axis;
    p_instance->pf_cst816t_get_chip_id             =       cst816t_get_chip_id;
    p_instance->pf_cst816t_get_finger_num          =    cst816t_get_finger_num;
    p_instance->pf_cst816t_sleep                   =             cst816t_sleep;
    p_instance->pf_cst816t_wakeup                  =            cst816t_wakeup;
    p_instance->pf_cst816t_set_err_reset_ctl       = cst816t_set_err_reset_ctl;
    p_instance->pf_cst816t_set_long_press_tick     =                          \
                                                cst816t_set_long_press_tick;
    p_instance->pf_cst816t_set_motion_mask         =    cst816t_set_motion_mask;
    p_instance->pf_cst816t_set_irq_pulse_width     =                          \
                                                cst816t_set_irq_pulse_width;
    p_instance->pf_cst816t_set_nor_scan_per        =                          \
                                                cst816t_set_nor_scan_per;
    p_instance->pf_cst816t_set_motion_slope_angle  =                          \
                                                cst816t_set_motion_slope_angle;
    p_instance->pf_cst816t_set_low_power_auto_wake_time =                     \
                                         cst816t_set_low_power_auto_wake_time;
    p_instance->pf_cst816t_set_lp_scan_th          =    cst816t_set_lp_scan_th;
    p_instance->pf_cst816t_set_lp_scan_win         =   cst816t_set_lp_scan_win;
    p_instance->pf_cst816t_set_lp_scan_freq        =                          \
                                                cst816t_set_lp_scan_freq;
    p_instance->pf_cst816t_set_lp_scan_idac        =                          \
                                                cst816t_set_lp_scan_idac;
    p_instance->pf_cst816t_set_auto_sleep_time     =                          \
                                                cst816t_set_auto_sleep_time;
    p_instance->pf_cst816t_set_irq_ctl             =       cst816t_set_irq_ctl;
    p_instance->pf_cst816t_set_auto_reset          =    cst816t_set_auto_reset;
    p_instance->pf_cst816t_set_long_press_time     =                          \
                                                cst816t_set_long_press_time;
    p_instance->pf_cst816t_set_io_ctl              =        cst816t_set_io_ctl;
    p_instance->pf_cst816t_disable_auto_sleep      =                          \
                                                cst816t_disable_auto_sleep;

    /* 6. record the active instance so the ISR hook can locate context */
    gs_p_active_instance = p_instance;

    /* 7. initialize the controller via the freshly-mounted vtable */
    cst816t_status_t ret = cst816t_init(*p_instance);
    if (CST816T_OK != ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG, "cst816t inst init failed!");
        return ret;
    }

    /* 8. hand the interrupt entry back to the caller */
    *pp_int_callback = cst816t_int_interrupt_callback;

    DEBUG_OUT(i, CST816T_LOG_TAG, "cst816t driver inst success");
    return ret;
}
//******************************* Functions *********************************//
