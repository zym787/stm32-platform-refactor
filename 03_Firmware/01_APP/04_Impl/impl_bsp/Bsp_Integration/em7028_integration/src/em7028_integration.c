/******************************************************************************
 * @file em7028_integration.c
 *
 * @par dependencies
 * - em7028_integration.h
 * - i2c_port.h        (MCU-port I2C abstraction, CORE_I2C_BUS_1 sensor bus)
 * - systick_port.h    (MCU-port ms timebase)
 * - dwt_port.h        (MCU-port us busy-wait)
 * - osal_wrapper_adapter.h
 * - osal_error.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection for the EM7028 PPG sensor handler.
 *
 * Provides concrete implementations for the five interfaces the handler
 * (and the driver underneath it) consume:
 *   - em7028_iic_driver_interface_t  : HAL-style mem read/write on the
 *     shared sensor I2C bus (CORE_I2C_BUS_1, hardware I2C3).
 *   - em7028_timebase_interface_t           : monotonic ms tick from systick_port.
 *   - em7028_delay_interface_t       : busy-wait us/ms via DWT for the
 *     5 ms boot delay the driver issues before the soft-reset probe.
 *   - em7028_os_delay_interface_t    : OS-yield ms wait used everywhere
 *     else inside the driver and handler so other tasks can run.
 *   - em7028_handler_os_queue_t      : OSAL queue services for the cmd
 *     and frame queues created during handler_inst.
 *
 * Changing the target MCU or RTOS requires only editing this file.
 *
 * @version V1.0 2026-05-05
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "em7028_integration.h"

#include "i2c_port.h"
#include "systick_port.h"
#include "dwt_port.h"

#include "osal_wrapper_adapter.h"
#include "osal_error.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* Non-NULL token mounted into em7028_iic_driver_interface_t::hi2c so the
 * driver's NULL-guard accepts the interface. The actual HAL handle is
 * resolved inside the MCU I2C port layer via CORE_I2C_BUS_1, so this
 * pointer value is never dereferenced here.                                */
#define EM7028_INT_I2C_BUS_TOKEN            ((void *)&hi2c3)
//******************************** Defines **********************************//

//******************************* Functions *********************************//

/* ---- I2C interface (hardware, common sensor bus / CORE_I2C_BUS_1) ------- */

/**
 * @brief  I2C bus init hook.
 *
 *         The shared sensor I2C bus is brought up by CubeMX-generated
 *         MX_I2C3_Init() before the scheduler starts and the bus mutex
 *         is created by core_i2c_port_init(CORE_I2C_BUS_1) in user_init,
 *         so there is nothing EM7028-specific to do here.
 *
 * @param[in] hi2c : Mounted hi2c token (unused -- routed by macro).
 *
 * @return EM7028_OK
 */
static em7028_status_t em7028_int_iic_init(void *hi2c)
{
    (void)hi2c;
    return EM7028_OK;
}

/**
 * @brief  I2C bus deinit hook (no-op for HAL passthrough).
 *
 * @param[in] hi2c : Unused.
 *
 * @return EM7028_OK
 */
static em7028_status_t em7028_int_iic_deinit(void *hi2c)
{
    (void)hi2c;
    return EM7028_OK;
}

/**
 * @brief  Blocking I2C memory write through the MCU sensor-bus port layer.
 *
 * @param[in] hi2c     : Bus token (unused -- routed by SENSOR_HARDWARE_I2C
 *                       macros to CORE_I2C_BUS_1).
 * @param[in] des_addr : 7-bit slave address shifted left by 1 (W frame).
 * @param[in] mem_addr : Internal register address.
 * @param[in] mem_size : Register address byte count (always 1 for EM7028).
 * @param[in] p_data   : Source buffer.
 * @param[in] size     : Number of bytes to write.
 * @param[in] timeout  : Timeout in ms.
 *
 * @return EM7028_OK on success, EM7028_ERROR on bus or timeout failure.
 */
static em7028_status_t em7028_int_iic_mem_write(void    *hi2c,
                                                uint16_t des_addr,
                                                uint16_t mem_addr,
                                                uint16_t mem_size,
                                                uint8_t *p_data,
                                                uint16_t size,
                                                uint32_t timeout)
{
    (void)hi2c;

    /**
     * Route to the shared sensor bus port layer; the bus mutex is owned by
     * the port and arbitrates access against AHT21 / MPU6050 traffic.
     **/
    core_i2c_status_t ret = SENSOR_HARDWARE_I2C_MEM_WRITE(
        des_addr, mem_addr, mem_size, p_data, size, timeout);
    if (CORE_I2C_OK != ret)
    {
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "em7028 mem write failed, ret:%d dst:0x%04X mem:0x%04X "
                  "size:%u",
                  ret, (unsigned)des_addr, (unsigned)mem_addr, (unsigned)size);
        return EM7028_ERROR;
    }
    return EM7028_OK;
}

/**
 * @brief  Blocking I2C memory read through the MCU sensor-bus port layer.
 *
 * @param[in]  hi2c     : Bus token (unused).
 * @param[in]  des_addr : 7-bit slave address shifted left by 1 (R frame).
 * @param[in]  mem_addr : Starting register address.
 * @param[in]  mem_size : Register address byte count (always 1 for EM7028).
 * @param[out] p_data   : Destination buffer.
 * @param[in]  size     : Number of bytes to read.
 * @param[in]  timeout  : Timeout in ms.
 *
 * @return EM7028_OK on success, EM7028_ERROR on bus or timeout failure.
 */
static em7028_status_t em7028_int_iic_mem_read(void    *hi2c,
                                               uint16_t des_addr,
                                               uint16_t mem_addr,
                                               uint16_t mem_size,
                                               uint8_t *p_data,
                                               uint16_t size,
                                               uint32_t timeout)
{
    (void)hi2c;

    core_i2c_status_t ret = SENSOR_HARDWARE_I2C_MEM_READ(
        des_addr, mem_addr, mem_size, p_data, size, timeout);
    if (CORE_I2C_OK != ret)
    {
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "em7028 mem read failed, ret:%d dst:0x%04X mem:0x%04X "
                  "size:%u",
                  ret, (unsigned)des_addr, (unsigned)mem_addr, (unsigned)size);
        return EM7028_ERROR;
    }
    return EM7028_OK;
}

/* ---- Timebase interface -------------------------------------------------- */

/**
 * @brief  Monotonic ms tick provider for frame timestamps and timeouts.
 *
 * @return Current ms tick from the systick port.
 */
static uint32_t em7028_int_get_tick_ms(void)
{
    return core_systick_get_ms();
}

/* ---- Hardware (DWT) delay interface -------------------------------------- */

/**
 * @brief  ms-resolution busy-wait wrapper.
 *
 *         The driver's vtable exposes a uint32_t-ms entry; DWT provides us
 *         resolution, so trampoline through *1000U. Used only on the
 *         5 ms boot path before the scheduler-aware OS delay kicks in.
 *
 * @param[in] ms : Wait duration in milliseconds.
 */
static void em7028_int_dwt_delay_ms(uint32_t const ms)
{
    core_dwt_delay_us(ms * 1000U);
}

/* ---- OS yield delay (driver layer) -------------------------------------- */

/**
 * @brief  OS-yield ms delay used by the driver and handler whenever they
 *         can afford to sleep instead of busy-waiting.
 *
 * @param[in] ms : Wait duration in milliseconds.
 */
static void em7028_int_rtos_delay(uint32_t const ms)
{
    osal_task_delay((osal_tick_type_t)ms);
}

/* ---- OS queue interface (handler-owned cmd / frame queues) -------------- */

/**
 * @brief  Create a message queue via OSAL.
 *
 * @param[in]  item_num     : Queue depth.
 * @param[in]  item_size    : Size of each item in bytes.
 * @param[out] queue_handler: Receives the queue handle.
 *
 * @return EM7028_HANDLER_OK on success, EM7028_HANDLER_ERROR on failure.
 */
static em7028_handler_status_t em7028_int_os_queue_create(
    uint32_t const   item_num,
    uint32_t const   item_size,
    void    **const  queue_handler)
{
    int32_t ret = osal_queue_create((osal_queue_handle_t *)queue_handler,
                                    (size_t)item_num, (size_t)item_size);
    return (OSAL_SUCCESS == ret) ? EM7028_HANDLER_OK : EM7028_HANDLER_ERROR;
}

/**
 * @brief  Push an item into the queue via OSAL.
 *
 * @param[in] queue_handler : Queue handle.
 * @param[in] item          : Pointer to the item to enqueue.
 * @param[in] timeout       : Wait timeout in OSAL ticks (== ms here).
 *
 * @return EM7028_HANDLER_OK on success, EM7028_HANDLER_ERROR on failure.
 */
static em7028_handler_status_t em7028_int_os_queue_put(
    void    *const   queue_handler,
    void    *const   item,
    uint32_t         timeout)
{
    int32_t ret = osal_queue_send((osal_queue_handle_t)queue_handler, item,
                                  (osal_tick_type_t)timeout);
    return (OSAL_SUCCESS == ret) ? EM7028_HANDLER_OK : EM7028_HANDLER_ERROR;
}

/**
 * @brief  Pop an item from the queue via OSAL.
 *
 * @param[in]  queue_handler : Queue handle.
 * @param[out] msg           : Receives the dequeued item.
 * @param[in]  timeout       : Wait timeout in OSAL ticks.
 *
 * @return EM7028_HANDLER_OK on success, EM7028_HANDLER_ERRORTIMEOUT on
 *         empty / timeout, EM7028_HANDLER_ERROR otherwise.
 */
static em7028_handler_status_t em7028_int_os_queue_get(
    void    *const   queue_handler,
    void    *const   msg,
    uint32_t         timeout)
{
    int32_t ret = osal_queue_receive((osal_queue_handle_t)queue_handler, msg,
                                     (osal_tick_type_t)timeout);
    if (OSAL_SUCCESS == ret)
    {
        return EM7028_HANDLER_OK;
    }
    /**
     * Distinguish empty-queue timeouts from hard errors so the handler
     * dispatch loop can use queue_get as its sample-period pacing source.
     **/
    return EM7028_HANDLER_ERRORTIMEOUT;
}

/**
 * @brief  Delete a queue via OSAL.
 *
 * @param[in] queue_handler : Queue handle.
 *
 * @return EM7028_HANDLER_OK
 */
static em7028_handler_status_t em7028_int_os_queue_delete(
    void *const queue_handler)
{
    osal_queue_delete((osal_queue_handle_t)queue_handler);
    return EM7028_HANDLER_OK;
}

/* ---- Assembled vtable instances ----------------------------------------- */

static em7028_iic_driver_interface_t s_iic_interface = {
    .hi2c             = EM7028_INT_I2C_BUS_TOKEN,
    .pf_iic_init      = em7028_int_iic_init,
    .pf_iic_deinit    = em7028_int_iic_deinit,
    .pf_iic_mem_write = em7028_int_iic_mem_write,
    .pf_iic_mem_read  = em7028_int_iic_mem_read,
};

static em7028_timebase_interface_t s_timebase_interface = {
    .pf_get_tick_count = em7028_int_get_tick_ms,
};

static em7028_delay_interface_t s_delay_interface = {
    .pf_delay_init = core_dwt_init,
    .pf_delay_us   = core_dwt_delay_us,
    .pf_delay_ms   = em7028_int_dwt_delay_ms,
};

static em7028_os_delay_interface_t s_os_delay_interface = {
    .pf_rtos_delay = em7028_int_rtos_delay,
};

static em7028_handler_os_queue_t s_os_queue_interface = {
    .pf_os_queue_create = em7028_int_os_queue_create,
    .pf_os_queue_put    = em7028_int_os_queue_put,
    .pf_os_queue_get    = em7028_int_os_queue_get,
    .pf_os_queue_delete = em7028_int_os_queue_delete,
};

static em7028_handler_os_interface_t s_os_interface = {
    .p_os_queue_interface = &s_os_queue_interface,
};

/* ---- Assembled handler input arg exported to user_task_reso_config ------ */

em7028_handler_input_args_t em7028_input_arg = {
    .p_os_interface       = &s_os_interface,
    .p_iic_interface      = &s_iic_interface,
    .p_timebase_interface = &s_timebase_interface,
    .p_delay_interface    = &s_delay_interface,
    .p_driver_os_delay    = &s_os_delay_interface,
};

//******************************* Functions *********************************//
