/******************************************************************************
 * @file cst816t_integration.c
 *
 * @par dependencies
 * - cst816t_integration.h
 * - bsp_cst816t_driver.h
 * - i2c_port.h     (MCU-port I2C abstraction)
 * - systick_port.h (MCU-port ms timebase abstraction)
 * - osal_wrapper_adapter.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection bundle for the CST816T capacitive touch driver.
 *
 * Provides concrete implementations for every interface the CST816T driver
 * needs (I2C mem read/write through the MCU port abstraction, ms timebase,
 * blocking delay, OS yield) and wires them into cst816t_input_arg, consumed
 * by the touch adapter port at startup.
 *
 * @version V1.0 2026-04-26
 * @version V2.0 2026-04-26
 * @version V3.0 2026-04-26
 * @upgrade 2.0: I2C path now goes through TOUCH_HARDWARE_I2C_* macros
 *               (CORE_I2C_BUS_2 / hi2c1) instead of HAL_I2C_Mem_* directly.
 * @upgrade 3.0: Tick path now goes through core_systick_get_ms — no more
 *               direct HAL_GetTick in this layer.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "cst816t_integration.h"

#include "i2c_port.h"
#include "systick_port.h"

#include "osal_wrapper_adapter.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define CST816T_HAL_IIC_TIMEOUT_MS    1000U
//******************************** Defines **********************************//

//******************************* Functions *********************************//

/* ---- I2C bus (HAL passthrough) ------------------------------------------ */
/**
 * @brief  I2C port init hook.  CubeMX MX_I2C1_Init() runs before the
 *         scheduler starts, so this is a no-op.
 *
 * @param[in] hi2c : Unused (interface contract).
 *
 * @return CST816T_OK
 */
static cst816t_status_t cst816t_iic_init(void const *const hi2c)
{
    (void)hi2c;
    return CST816T_OK;
}

/**
 * @brief  I2C port deinit hook (no-op for HAL passthrough).
 *
 * @param[in] hi2c : Unused.
 *
 * @return CST816T_OK
 */
static cst816t_status_t cst816t_iic_deinit(void const *const hi2c)
{
    (void)hi2c;
    return CST816T_OK;
}

/**
 * @brief  Blocking I2C memory write through the MCU port abstraction.
 *
 * @param[in] i2c       : Bus handle (unused; the MCU port owns the bus
 *                        binding via TOUCH_HARDWARE_I2C_*).
 * @param[in] des_addr  : 7-bit slave address shifted left by 1 (HAL convention).
 * @param[in] mem_addr  : Internal register address.
 * @param[in] mem_size  : 1 -> 8-bit reg addr, 2 -> 16-bit reg addr.
 * @param[in] p_data    : Source buffer.
 * @param[in] size      : Number of bytes to write.
 * @param[in] timeout   : Timeout in ms.
 *
 * @return CST816T_OK on success, CST816T_ERROR on bus failure.
 */
static cst816t_status_t cst816t_iic_mem_write(void    *i2c,
                                              uint16_t des_addr,
                                              uint16_t mem_addr,
                                              uint16_t mem_size,
                                              uint8_t *p_data,
                                              uint16_t size,
                                              uint32_t timeout)
{
    (void)i2c;
    core_i2c_status_t st = TOUCH_HARDWARE_I2C_MEM_WRITE(
        des_addr, mem_addr, mem_size, p_data, size, timeout);
    return (CORE_I2C_OK == st) ? CST816T_OK : CST816T_ERROR;
}

/**
 * @brief  Blocking I2C memory read through the MCU port abstraction.
 *         Same parameter contract as cst816t_iic_mem_write.
 *
 * @return CST816T_OK on success, CST816T_ERROR on bus failure.
 */
static cst816t_status_t cst816t_iic_mem_read(void    *i2c,
                                             uint16_t des_addr,
                                             uint16_t mem_addr,
                                             uint16_t mem_size,
                                             uint8_t *p_data,
                                             uint16_t size,
                                             uint32_t timeout)
{
    (void)i2c;
    core_i2c_status_t st = TOUCH_HARDWARE_I2C_MEM_READ(
        des_addr, mem_addr, mem_size, p_data, size, timeout);
    return (CORE_I2C_OK == st) ? CST816T_OK : CST816T_ERROR;
}

/* ---- Timebase / Delay / OS ---------------------------------------------- */
/**
 * @brief  Monotonic ms tick provider — routed through MCU systick port.
 *
 * @return Current ms tick.
 */
static uint32_t cst816t_tb_get_tick(void)
{
    return core_systick_get_ms();
}

/**
 * @brief  Blocking delay init hook (no busy-loop calibration needed on
 *         a Cortex-M4F using a SysTick-driven timebase).
 */
static void cst816t_delay_init(void)
{
    /**
     * Nothing to calibrate; core_systick is already running.
     **/
}

/**
 * @brief  Blocking ms delay used by the driver for datasheet-mandated
 *         reset / boot timings.  Routes through OSAL so other tasks
 *         can run during the wait.
 *
 * @param[in] ms : Milliseconds to wait.
 */
static void cst816t_delay_ms(uint32_t const ms)
{
    osal_task_delay(ms);
}

/**
 * @brief  Microsecond busy-wait — currently unused by the CST816T driver
 *         on this MCU (datasheet timings are all ms-scale), so this
 *         intentionally drops the request rather than block the scheduler.
 *
 * @param[in] us : Microseconds (ignored).
 */
static void cst816t_delay_us(uint32_t const us)
{
    (void)us;
}

/**
 * @brief  Cooperative yield used by the driver between polled register
 *         reads.  Routed through osal_task_delay so the scheduler can
 *         run other tasks during the wait.
 *
 * @param[in] ms : Milliseconds to yield.
 */
static void cst816t_os_yield(uint32_t const ms)
{
    osal_task_delay(ms);
}

/* ---- Assembled interface vtables ----------------------------------------- */

static cst816t_iic_driver_interface_t s_iic_interface = {
    .hi2c              = (void *)&hi2c1,
    .pf_iic_init       = cst816t_iic_init,
    .pf_iic_deinit     = cst816t_iic_deinit,
    .pf_iic_mem_write  = cst816t_iic_mem_write,
    .pf_iic_mem_read   = cst816t_iic_mem_read,
};

static cst816t_timebase_interface_t s_timebase_interface = {
    .pf_get_tick_count = cst816t_tb_get_tick,
};

static cst816t_delay_interface_t s_delay_interface = {
    .pf_delay_init = cst816t_delay_init,
    .pf_delay_ms   = cst816t_delay_ms,
    .pf_delay_us   = cst816t_delay_us,
};

static cst816t_os_delay_interface_t s_os_interface = {
    .pf_rtos_yield = cst816t_os_yield,
};

/* ---- Driver input arg ---------------------------------------------------- */

cst816t_driver_input_arg_t cst816t_input_arg = {
    .p_iic_interface      = &s_iic_interface,
    .p_timebase_interface = &s_timebase_interface,
    .p_delay_interface    = &s_delay_interface,
    .p_os_interface       = &s_os_interface,
};
//******************************* Functions *********************************//
