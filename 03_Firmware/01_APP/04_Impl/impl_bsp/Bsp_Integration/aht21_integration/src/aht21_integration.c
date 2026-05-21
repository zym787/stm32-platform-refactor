/******************************************************************************
 * @file aht21_integration.c
 *
 * @par dependencies
 * - aht21_integration.h
 * - i2c_port.h     (MCU-port I2C abstraction)
 * - systick_port.h (MCU-port ms timebase abstraction)
 * - osal_wrapper_adapter.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection for the AHT21 temperature/humidity handler.
 *
 * Provides concrete implementations for every interface the AHT21 handler
 * needs (IIC, OS queue, timebase, yield) and wires them together into
 * aht21_input_arg, which is passed to temp_humi_handler_thread() at startup.
 *
 * Changing the target MCU or RTOS requires only editing this file.
 *
 * @version V1.0 2026-04-12
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "i2c_port.h"
#include "systick_port.h"
#include "aht21_integration.h"
#include "osal_wrapper_adapter.h"
#include "osal_error.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//

/* ---------- IIC interface ------------------------------------------------- */

#define AHT21_I2C_TIMEOUT_MS    100U

static aht21_status_t iic_init_myown(void *bus)
{
    (void)bus;
    // core_i2c_status_t ret = core_i2c_port_init(CORE_I2C_BUS_1);
    // return (ret == CORE_I2C_OK) ? AHT21_OK : AHT21_ERROR;
    return AHT21_OK;
}

static aht21_status_t iic_deinit_myown(void *bus)
{
    (void)bus;
    return AHT21_OK;
}

#if USE_HARDWARE_I2C

static aht21_status_t i2c_master_write_myown(void           *bus,
                                             uint16_t        dev_addr,
                                             uint8_t  *data,
                                             uint16_t        size)
{
    (void)bus;
    core_i2c_status_t ret = SENSOR_HARDWARE_I2C_SEND_BYTE(
        dev_addr, data, size, AHT21_I2C_TIMEOUT_MS);
    return (ret == CORE_I2C_OK) ? AHT21_OK : AHT21_ERROR;
}

static aht21_status_t i2c_master_read_myown(void     *bus,
                                            uint16_t  dev_addr,
                                            uint8_t  *data,
                                            uint16_t  size)
{
    (void)bus;
    core_i2c_status_t ret = SENSOR_HARDWARE_I2C_RECEIVE_BYTE(
        dev_addr, data, size, AHT21_I2C_TIMEOUT_MS);
    return (ret == CORE_I2C_OK) ? AHT21_OK : AHT21_ERROR;
}

static aht21_iic_driver_interface_t s_iic_driver_interface = {
    .pf_iic_init         = iic_init_myown,
    .pf_iic_deinit       = iic_deinit_myown,
    .pf_i2c_master_write = i2c_master_write_myown,
    .pf_i2c_master_read  = i2c_master_read_myown,
};

#else  /* SOFTWARE I2C */

static aht21_status_t iic_start_myown(void *bus)
{
    (void)bus;
    SENSOR_SOFTWARE_I2C_START();
    return AHT21_OK;
}

static aht21_status_t iic_stop_myown(void *bus)
{
    (void)bus;
    SENSOR_SOFTWARE_I2C_STOP();
    return AHT21_OK;
}

static aht21_status_t iic_wait_ack_myown(void *bus)
{
    (void)bus;
    core_i2c_status_t ret = SENSOR_SOFTWARE_I2C_WAIT_ACK();
    return (ret == CORE_I2C_OK) ? AHT21_OK : AHT21_ERROR;
}

static aht21_status_t iic_send_ack_myown(void *bus)
{
    (void)bus;
    SENSOR_SOFTWARE_I2C_SEND_ACK();
    return AHT21_OK;
}

static aht21_status_t iic_send_no_ack_myown(void *bus)
{
    (void)bus;
    SENSOR_SOFTWARE_I2C_SEND_NACK();
    return AHT21_OK;
}

static aht21_status_t iic_send_byte_myown(void *bus, uint8_t const data)
{
    (void)bus;
    SENSOR_SOFTWARE_I2C_SEND_BYTE(data);
    return AHT21_OK;
}

static aht21_status_t iic_receive_byte_myown(void *bus, uint8_t *const data)
{
    (void)bus;
    SENSOR_SOFTWARE_I2C_RECEIVE_BYTE(data);
    return AHT21_OK;
}

static aht21_iic_driver_interface_t s_iic_driver_interface = {
    .pf_iic_init         = iic_init_myown,
    .pf_iic_deinit       = iic_deinit_myown,
    .pf_iic_start        = iic_start_myown,
    .pf_iic_stop         = iic_stop_myown,
    .pf_iic_wait_ack     = iic_wait_ack_myown,
    .pf_iic_send_ack     = iic_send_ack_myown,
    .pf_iic_send_no_ack  = iic_send_no_ack_myown,
    .pf_iic_send_byte    = iic_send_byte_myown,
    .pf_iic_receive_byte = iic_receive_byte_myown,
    .pf_critical_enter   = (aht21_status_t (*)(void))osal_critical_enter,
    .pf_critical_exit    = (aht21_status_t (*)(void))osal_critical_exit,
};

#endif // USE_HARDWARE_I2C

/* ---------- Timebase interface -------------------------------------------- */

static uint32_t get_tick_count_ms(void)
{
    return core_systick_get_ms();
}

static aht21_timebase_interface_t s_timebase_interface = {
    .pf_get_tick_count_ms = get_tick_count_ms,
};

/* ---------- Yield interface ----------------------------------------------- */

static void rtos_yield(uint32_t const xms)
{
    osal_task_delay(xms);
}

static aht21_yield_interface_t s_yield_interface = {
    .pf_rtos_yield = rtos_yield,
};

/* ---------- OS queue interface -------------------------------------------- */

static temp_humi_status_t os_queue_create_adapter(uint32_t const item_num,
                                                  uint32_t const item_size,
                                                  void **const   queue_handler)
{
    int32_t ret = osal_queue_create((osal_queue_handle_t *)queue_handler,
                                    (size_t)item_num, (size_t)item_size);
    return (ret == OSAL_SUCCESS) ? TEMP_HUMI_OK : TEMP_HUMI_ERROR;
}

static temp_humi_status_t os_queue_put_adapter(void *const queue_handler,
                                               void *const item,
                                               uint32_t    timeout)
{
    int32_t ret = osal_queue_send((osal_queue_handle_t)queue_handler, item,
                                  (osal_tick_type_t)timeout);
    return (ret == OSAL_SUCCESS) ? TEMP_HUMI_OK : TEMP_HUMI_ERROR;
}

static temp_humi_status_t os_queue_get_adapter(void *const queue_handler,
                                               void *const msg,
                                               uint32_t    timeout)
{
    int32_t ret = osal_queue_receive((osal_queue_handle_t)queue_handler, msg,
                                     (osal_tick_type_t)timeout);
    return (ret == OSAL_SUCCESS) ? TEMP_HUMI_OK : TEMP_HUMI_ERROR;
}

static temp_humi_status_t os_queue_delete_adapter(void *const queue_handler)
{
    osal_queue_delete((osal_queue_handle_t)queue_handler);
    return TEMP_HUMI_OK;
}

static handler_os_queue_t s_os_queue_interface = {
    .pf_os_queue_create = os_queue_create_adapter,
    .pf_os_queue_put    = os_queue_put_adapter,
    .pf_os_queue_get    = os_queue_get_adapter,
    .pf_os_queue_delete = os_queue_delete_adapter,
};

/* ---------- Assembled input arg exported to user_task_reso_config --------- */

temp_humi_handler_input_arg_t aht21_input_arg = {
    .p_iic_driver_interface = &s_iic_driver_interface,
    .p_os_queue_interface   = &s_os_queue_interface,
    .p_timebase_interface   = &s_timebase_interface,
    .p_yield_interface      = &s_yield_interface,
};

//******************************* Functions *********************************//
