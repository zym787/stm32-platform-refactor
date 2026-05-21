/******************************************************************************
 * @file mpu6050_integration.c
 *
 * @par dependencies
 * - bsp_mpuxxxx_handler.h
 * - main.h
 * - osal_wrapper_adapter.h
 * - i2c_port.h     (MCU-port I2C abstraction)
 * - systick_port.h (MCU-port ms timebase abstraction)
 * - dwt_port.h     (MCU-port us busy-wait abstraction)
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection for the MPU6050 motion sensor handler.
 *
 * Provides concrete implementations for I2C (mem read/write/DMA), timebase,
 * delay, yield, OS (queues, mutexes, semaphores, task notifications) and
 * interrupt (EXTI) interfaces.  Wires them into the mpu6050_input_args struct
 * passed to mpuxxxx_handler_thread() at startup.
 *
 * @version V1.0 2026-04-10
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#include "bsp_mpuxxxx_handler.h"

#include "main.h"

#include "osal_wrapper_adapter.h"

#include "i2c_port.h"
#include "systick_port.h"
#include "dwt_port.h"

#define MPU6050_I2C_DMA_WAIT_TIMEOUT_MS   10U


/**
 * @brief  I2C bus init hook.  CubeMX MX_I2C3_Init() runs before the scheduler
 *         starts, so this is a no-op.
 *
 * @param[in] iic_bus : Unused (interface contract).
 *
 * @return MPUXXXX_OK
 */
mpuxxxx_status_t iic_driver_init(void const *const iic_bus)
{
    (void)iic_bus;
    return MPUXXXX_OK;
}

/**
 * @brief  I2C bus deinit hook (no-op for HAL passthrough).
 *
 * @param[in] iic_bus : Unused.
 *
 * @return MPUXXXX_OK
 */
mpuxxxx_status_t iic_driver_deinit(void const *const iic_bus)
{
    (void)iic_bus;
    return MPUXXXX_OK;
}

/**
 * @brief  Blocking I2C memory read through the MCU port abstraction.
 *         Routes to SENSOR_HARDWARE_I2C_MEM_READ (CORE_I2C_BUS_2).
 *
 * @param[in]  hi2c         : Bus handle (unused).
 * @param[in]  dst_address  : 7-bit slave address shifted left by 1.
 * @param[in]  mem_addr     : Internal register address.
 * @param[in]  mem_size     : 1 -> 8-bit reg addr, 2 -> 16-bit reg addr.
 * @param[out] p_data       : Destination buffer.
 * @param[in]  size         : Number of bytes to read.
 * @param[in]  timeout      : Timeout in ms.
 *
 * @return MPUXXXX_OK on success, MPUXXXX_ERROR on bus failure.
 */
mpuxxxx_status_t iic_mem_read(void *hi2c,
                              uint16_t dst_address,
                              uint16_t mem_addr,
                              uint16_t mem_size,
                              uint8_t *p_data,
                              uint16_t size,
                              uint32_t timeout)
{
    core_i2c_status_t ret = SENSOR_HARDWARE_I2C_MEM_READ(
        dst_address, mem_addr, mem_size, p_data, size, timeout);
    if (ret != CORE_I2C_OK)
    {
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "iic mem read failed, ret:%d dst:0x%04X mem:0x%04X size:%u",
                  ret, (unsigned int)dst_address, (unsigned int)mem_addr,
                  (unsigned int)size);
        return MPUXXXX_ERROR;
    }
    return MPUXXXX_OK;
}

/**
 * @brief  Blocking I2C memory write through the MCU port abstraction.
 *         Routes to SENSOR_HARDWARE_I2C_MEM_WRITE (CORE_I2C_BUS_2).
 *
 * @param[in] hi2c         : Bus handle (unused).
 * @param[in] dst_address  : 7-bit slave address shifted left by 1.
 * @param[in] mem_addr     : Internal register address.
 * @param[in] mem_size     : 1 -> 8-bit reg addr, 2 -> 16-bit reg addr.
 * @param[in] p_data       : Source buffer.
 * @param[in] size         : Number of bytes to write.
 * @param[in] timeout      : Timeout in ms.
 *
 * @return MPUXXXX_OK on success, MPUXXXX_ERROR on bus or timeout failure.
 */
mpuxxxx_status_t iic_mem_write(void *hi2c,
                               uint16_t dst_address,
                               uint16_t mem_addr,
                               uint16_t mem_size,
                               uint8_t *p_data,
                               uint16_t size,
                               uint32_t timeout)
{
    core_i2c_status_t ret = SENSOR_HARDWARE_I2C_MEM_WRITE(
        dst_address, mem_addr, mem_size, p_data, size, timeout);
    if (ret != CORE_I2C_OK)
    {
        if (CORE_I2C_TIMEOUT == ret)
        {
            DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                      "iic mem write timeout, dst:0x%04X mem:0x%04X size:%u",
                      (unsigned int)dst_address, (unsigned int)mem_addr,
                      (unsigned int)size);
        }
        DEBUG_OUT(e, HAL_IIC_ERR_LOG_TAG,
                  "iic mem write failed, ret:%d dst:0x%04X mem:0x%04X size:%u",
                  ret, (unsigned int)dst_address, (unsigned int)mem_addr,
                  (unsigned int)size);
        return MPUXXXX_ERROR;
    }
    return MPUXXXX_OK;
}

/**
 * @brief  Non-blocking DMA I2C memory read through the MCU port abstraction.
 *         Routes to SENSOR_HARDWARE_I2C_MEM_READ_DMA (CORE_I2C_BUS_2).
 *         Blocks internally on a semaphore inside the I2C abstraction until
 *         the DMA transfer completes or times out.
 *
 * @param[in]  hi2c         : Bus handle (unused).
 * @param[in]  dst_address  : 7-bit slave address shifted left by 1.
 * @param[in]  mem_addr     : Internal register address.
 * @param[in]  mem_size     : 1 -> 8-bit reg addr, 2 -> 16-bit reg addr.
 * @param[out] p_data       : Destination buffer (valid after return).
 * @param[in]  size         : Number of bytes to read.
 *
 * @return MPUXXXX_OK on completion, MPUXXXX_ERROR on timeout or bus failure.
 */
mpuxxxx_status_t iic_mem_read_dma(void *hi2c,
                                  uint16_t dst_address,
                                  uint16_t mem_addr,
                                  uint16_t mem_size,
                                  uint8_t *p_data,
                                  uint16_t size)
{
    core_i2c_status_t ret = SENSOR_HARDWARE_I2C_MEM_READ_DMA(
        dst_address, mem_addr, mem_size, p_data, size,
        MPU6050_I2C_DMA_WAIT_TIMEOUT_MS);
    if (ret != CORE_I2C_OK)
    {
        DEBUG_OUT(
            e, HAL_IIC_ERR_LOG_TAG,
            "iic mem read dma failed, ret:%d dst:0x%04X mem:0x%04X size:%u",
            ret, (unsigned int)dst_address, (unsigned int)mem_addr,
            (unsigned int)size);
        return MPUXXXX_ERROR;
    }
    return MPUXXXX_OK;
}


iic_driver_interface_t mpuxxxx_iic_driver_instance = {
    .hi2c                = &hi2c3,
    .pf_iic_deinit       = iic_driver_deinit,
    .pf_iic_init         = iic_driver_init,
    .pf_iic_mem_read     = iic_mem_read,
    .pf_iic_mem_write    = iic_mem_write,
    .pf_iic_mem_read_dma = iic_mem_read_dma,
};

timebase_interface_t timebase_interface = {
    .pf_get_tick_count = core_systick_get_ms,
};

/*****************************************************************************/
#if OS_SUPPORTING
static void osal_yield_wrapper(uint32_t const ms)
{
    osal_task_delay((osal_tick_type_t)ms);
}

yield_interface_t yield_interface = {
    .pf_rtos_yield = osal_yield_wrapper,
};
#endif
/*****************************************************************************/
/**
 * @brief  ms busy-wait wrapper — driver vtable expects (uint32_t ms),
 *         core_dwt_delay_us takes us, so trampoline through *1000.
 *         Kept busy-wait (not osal_task_delay) because the MPU init path
 *         may run before the scheduler starts.
 */
static void mpu_dwt_delay_ms(uint32_t ms)
{
    core_dwt_delay_us(ms * 1000U);
}

delay_interface_t delay_interface = {
    .pf_delay_init = core_dwt_init,
    .pf_delay_us   = core_dwt_delay_us,
    .pf_delay_ms   = mpu_dwt_delay_ms,
};
/*****************************************************************************/
// os interface
#if OS_SUPPORTING
static mpuxxxx_status_t os_queue_create(uint32_t const queue_size,
                                        uint32_t const item_size,
                                        void         **queue_handle)
{
    int32_t ret = osal_queue_create((osal_queue_handle_t *)queue_handle,
                                   (size_t)queue_size, (size_t)item_size);
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static mpuxxxx_status_t os_queue_delete(void *const queue_handle)
{
    osal_queue_delete((osal_queue_handle_t)queue_handle);
    return MPUXXXX_OK;
}

static mpuxxxx_status_t os_queue_put(void *const queue_handle,
                                     void *const item,
                                     uint32_t const timeout)
{
    int32_t ret = osal_queue_send((osal_queue_handle_t)queue_handle, item,
                                 (osal_tick_type_t)timeout);
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static mpuxxxx_status_t os_queue_put_isr(void *const queue_handle,
                                         void *const item,
                                         long *const HigherPriorityTaskWoken)
{
    int32_t ret = osal_queue_send_from_isr(
        (osal_queue_handle_t)queue_handle, item,
        (osal_base_type_t *)HigherPriorityTaskWoken);
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static mpuxxxx_status_t os_queue_get(void *const queue_handle,
                                     void *const item,
                                     uint32_t const timeout)
{
    int32_t ret = osal_queue_receive((osal_queue_handle_t)queue_handle, item,
                                    (osal_tick_type_t)timeout);
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static mpuxxxx_status_t os_semaphore_create_mutex(void **mutex_handle)
{
    int32_t ret = osal_mutex_init((osal_mutex_handle_t *)mutex_handle);
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static mpuxxxx_status_t os_semaphore_delete_mutex(void *const mutex_handle)
{
    osal_mutex_delete((osal_mutex_handle_t)mutex_handle);
    return MPUXXXX_OK;
}

static mpuxxxx_status_t os_semaphore_lock_mutex(void *const    mutex_handle,
                                                 uint32_t const timeout)
{
    int32_t ret = osal_mutex_take((osal_mutex_handle_t)mutex_handle,
                                 (osal_tick_type_t)timeout);
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static mpuxxxx_status_t os_semaphore_unlock_mutex(void *const mutex_handle)
{
    int32_t ret = osal_mutex_give((osal_mutex_handle_t)mutex_handle);
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static mpuxxxx_status_t os_semaphore_create_binary(void **binary_handle)
{
    int32_t ret = osal_sema_init((osal_sema_handle_t *)binary_handle, 0);
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static mpuxxxx_status_t os_semaphore_delete_binary(void *const binary_handle)
{
    osal_sema_delete((osal_sema_handle_t)binary_handle);
    return MPUXXXX_OK;
}

static mpuxxxx_status_t os_semaphore_signal_binary(void *const binary_handle)
{
    int32_t ret = osal_sema_give((osal_sema_handle_t)binary_handle);
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static mpuxxxx_status_t os_semaphore_wait_binary(void *const binary_handle)
{
    int32_t ret = osal_sema_take((osal_sema_handle_t)binary_handle,
                                OSAL_MAX_DELAY);
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static mpuxxxx_status_t
os_semaphore_signal_notify_isr(void *const notify_handle, uint32_t ulValue,
                               uint32_t    eAction,
                               long *const HigherPriorityTaskWoken)
{
    int32_t ret = osal_notify_send_from_isr(
        (osal_task_handle_t)notify_handle,
        ulValue,
        (osal_notify_action_t)eAction,
        (osal_base_type_t *)HigherPriorityTaskWoken);
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static mpuxxxx_status_t os_semaphore_wait_notify(uint32_t  ulBitsToClearOnEntry,
                                                  uint32_t  ulBitsToClearOnExit,
                                                  uint32_t *pulNotificationValue,
                                                  uint32_t  timeout)
{
    int32_t ret = osal_notify_wait(ulBitsToClearOnEntry, ulBitsToClearOnExit,
                                   pulNotificationValue,
                                   (osal_tick_type_t)timeout);
    /* DMA complete: release bus mutex in task context (mutex cannot be given
       from ISR; the ISR only sends the task notification above). */
    return (ret == 0) ? MPUXXXX_OK : MPUXXXX_ERROR;
}

static void *os_get_task_handle(void)
{
    return (void *)osal_task_get_current_handle();
}

os_interface_t os_interface = {
    .pf_os_queue_create          = os_queue_create,
    .pf_os_queue_delete          = os_queue_delete,
    .pf_os_queue_receive         = os_queue_get,
    .pf_os_queue_send            = os_queue_put,
    .pf_os_queue_send_isr        = os_queue_put_isr,
    .pf_os_mutex_create          = os_semaphore_create_mutex,
    .pf_os_mutex_delete          = os_semaphore_delete_mutex,
    .pf_os_mutex_lock            = os_semaphore_lock_mutex,
    .pf_os_mutex_unlock          = os_semaphore_unlock_mutex,
    .pf_os_semaphore_create      = os_semaphore_create_binary,
    .pf_os_semaphore_delete      = os_semaphore_delete_binary,
    .pf_os_semaphore_take        = os_semaphore_wait_binary,
    .pf_os_semaphore_give        = os_semaphore_signal_binary,
    .pf_os_semaephore_notify_isr = os_semaphore_signal_notify_isr,
    .pf_os_semaphore_wait_notify = os_semaphore_wait_notify,
    .pf_os_get_task_handle       = os_get_task_handle
};

#endif


/* MPU6050 INT → PB5 → EXTI9_5_IRQn (rising edge, see gpio.c).
 * These two wrappers replace the former iic_mem_write(INT_EN) calls that were
 * illegally called from ISR context (osal_mutex_take cannot block in ISR). */
/**
 * @brief  Disable the MPU6050 EXTI interrupt (PB5 → EXTI9_5_IRQn).
 *         Called before starting a DMA read to prevent re-triggering.
 */
static mpuxxxx_status_t irq_disable(void)
{
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
    return MPUXXXX_OK;
}

/**
 * @brief  Re-enable the MPU6050 EXTI interrupt after the DMA read completes.
 */
static mpuxxxx_status_t irq_enable(void)
{
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    return MPUXXXX_OK;
}

hardware_interrupt_interface_t interrupt_interface = {
    .pf_irq_clear_pending =
        (mpuxxxx_status_t (*)(void))HAL_NVIC_ClearPendingIRQ,
    .pf_irq_deinit        = NULL,
    .pf_irq_disable       = irq_disable,
    .pf_irq_disable_clock = NULL,
    .pf_irq_enable        = irq_enable,
    .pf_irq_enable_clock  = NULL,
    .pf_irq_init          = NULL};

mpuxxxx_handler_input_args_t mpu6050_input_args = {
    .p_iic_driver         = &mpuxxxx_iic_driver_instance,
    .p_timebase           = &timebase_interface,
    .p_delay_interface    = &delay_interface,
    .p_yield_interface    = &yield_interface,
    .p_os_interface       = &os_interface,
    .p_interrupt_interface = &interrupt_interface
};
