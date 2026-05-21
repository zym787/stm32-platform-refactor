/******************************************************************************
 * @file w25q64_integration.c
 *
 * @par dependencies
 * - w25q64_integration.h
 * - bsp_w25q64_handler.h
 * - spi_port.h            (MCU-platform SPI bus abstraction, CORE_SPI_BUS_2)
 * - systick_port.h        (MCU-port ms timebase)
 * - osal_wrapper_adapter.h
 * - osal_error.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection bundle for the W25Q64 flash handler.
 *
 * Provides concrete implementations for every interface the W25Q64 handler
 * needs (hardware SPI / GPIO CS, ms timebase, OS queue, OS delay) and
 * wires them into w25q64_input_arg, consumed by flash_handler_thread()
 * at startup.
 *
 * All bus access goes through the MCU SPI port layer (CORE_SPI_BUS_2);
 * this file no longer touches HAL directly.  Bus mutex, CS toggling and
 * peripheral handle are owned by spi_port.c -- pin macros (PB13 CS,
 * PB10 SCK, PB14 MISO, PB15 MOSI) live in Core/Inc/main.h.
 *
 * @version V1.1 2026-05-05
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "w25q64_integration.h"

#include "spi_port.h"
#include "systick_port.h"
#include "osal_wrapper_adapter.h"
#include "osal_error.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define W25Q64_SPI_TIMEOUT_MS       200U
#define W25Q64_QUEUE_DEPTH          10U
//******************************** Defines **********************************//

//******************************* Functions *********************************//

/* ---- SPI bus (delegated to MCU SPI port, CORE_SPI_BUS_2) ---------------- */

/**
 * @brief  SPI init hook (no-op).
 *
 * @par
 *  SPI2 is brought up by CubeMX-generated MX_SPI2_Init() and the bus
 *  mutex is created by core_spi_port_init(CORE_SPI_BUS_2) during
 *  user_init.  Nothing flash-specific remains for this hook to do.
 *
 * @return W25Q64_OK
 */
static w25q64_status_t w25q64_spi_init(void)
{
    /**
     * Bus + mutex + CS GPIO are owned by the MCU SPI port layer; this
     * hook only exists so the driver vtable stays fully populated.
     **/
    return W25Q64_OK;
}

/**
 * @brief  SPI deinit hook (no-op).
 *
 * @return W25Q64_OK
 */
static w25q64_status_t w25q64_spi_deinit(void)
{
    return W25Q64_OK;
}

/**
 * @brief  Blocking hardware SPI transmit via the MCU SPI port layer.
 *
 * @param[in] p_data      : Source buffer.
 * @param[in] data_length : Number of bytes to send (<= UINT16_MAX).
 *
 * @return W25Q64_OK on success, W25Q64_ERRORPARAMETER on bad args,
 *         W25Q64_ERROR on bus failure or timeout.
 */
static w25q64_status_t w25q64_spi_transmit(uint8_t const *p_data,
                                            uint32_t       data_length)
{
    /**
     * Input validation: NULL guard plus 16-bit width guard, since the
     * underlying HAL transfer counter is uint16_t.
     **/
    if (NULL == p_data || data_length > UINT16_MAX)
    {
        return W25Q64_ERRORPARAMETER;
    }

    /**
     * Cast away const for the bus API -- HAL never modifies the TX
     * buffer in master mode, and the MCU port layer mirrors the HAL
     * prototype.
     **/
    core_spi_status_t ret = FLASH_HARDWARE_SPI_TRANSMIT(
        (uint8_t *)p_data,
        (uint16_t)data_length,
        W25Q64_SPI_TIMEOUT_MS);

    return (CORE_SPI_OK == ret) ? W25Q64_OK : W25Q64_ERROR;
}

/**
 * @brief  Blocking hardware SPI receive via the MCU SPI port layer.
 *
 * @param[out] p_buffer      : Destination buffer.
 * @param[in]  buffer_length : Number of bytes to read (<= UINT16_MAX).
 *
 * @return W25Q64_OK on success, W25Q64_ERRORPARAMETER on bad args,
 *         W25Q64_ERROR on bus failure or timeout.
 */
static w25q64_status_t w25q64_spi_read(uint8_t  *p_buffer,
                                        uint32_t  buffer_length)
{
    /**
     * Input validation: same NULL + 16-bit width contract as the
     * transmit path.
     **/
    if (NULL == p_buffer || buffer_length > UINT16_MAX)
    {
        return W25Q64_ERRORPARAMETER;
    }

    core_spi_status_t ret = FLASH_HARDWARE_SPI_RECEIVE(
        p_buffer,
        (uint16_t)buffer_length,
        W25Q64_SPI_TIMEOUT_MS);

    return (CORE_SPI_OK == ret) ? W25Q64_OK : W25Q64_ERROR;
}

/**
 * @brief  DMA transmit stub -- not used for flash testing.
 *
 * @return W25Q64_ERRORUNSUPPORTED
 */
static w25q64_status_t w25q64_spi_transmit_dma(uint8_t const *p_data,
                                                uint32_t       data_length)
{
    (void)p_data;
    (void)data_length;
    return W25Q64_ERRORUNSUPPORTED;
}

/**
 * @brief  DMA wait stub -- not used for flash testing.
 *
 * @return W25Q64_ERRORUNSUPPORTED
 */
static w25q64_status_t w25q64_spi_wait_dma_complete(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return W25Q64_ERRORUNSUPPORTED;
}

/**
 * @brief  Drive the flash CS line via the MCU SPI port layer.
 *
 * @param[in] state : 0 -> CS low (active), non-zero -> CS high (idle).
 *
 * @return W25Q64_OK on success, W25Q64_ERROR if the port layer rejects
 *         the request (e.g. invalid bus index).
 */
static w25q64_status_t w25q64_spi_write_cs_pin(uint8_t state)
{
    /**
     * The CS GPIO (PB13) is registered on CORE_SPI_BUS_2; let the port
     * layer route the call so we don't hard-couple to HAL here.
     **/
    core_spi_status_t ret = (0U != state)
        ? FLASH_HARDWARE_SPI_CS_DESELECT()
        : FLASH_HARDWARE_SPI_CS_SELECT();

    return (CORE_SPI_OK == ret) ? W25Q64_OK : W25Q64_ERROR;
}

/**
 * @brief  DC pin stub -- W25Q64 has no data/command line.
 *
 * @return W25Q64_OK
 */
static w25q64_status_t w25q64_spi_write_dc_pin(uint8_t state)
{
    (void)state;
    return W25Q64_OK;
}

/* ---- Timebase / OS ------------------------------------------------------ */

/**
 * @brief  Monotonic ms tick provider via systick port.
 *
 * @return Current ms tick.
 */
static uint32_t w25q64_tb_get_tick_ms(void)
{
    return core_systick_get_ms();
}

/**
 * @brief  Blocking ms delay (datasheet timings like power-on 50 ms).
 *         Routes through OSAL so other tasks can run during the wait.
 *
 * @param[in] ms : Milliseconds to wait.
 */
static void w25q64_tb_delay_ms(uint32_t ms)
{
    osal_task_delay(ms);
}

/**
 * @brief  OS-aware delay for driver layer (returns void).
 *
 * @param[in] ms : Milliseconds to wait.
 */
static void w25q64_drv_delay_ms(uint32_t ms)
{
    osal_task_delay(ms);
}

/**
 * @brief  OS-aware delay for handler layer (returns status).
 *
 * @param[in] ms : Milliseconds to wait.
 *
 * @return FLASH_HANLDER_OK always.
 */
static flash_handler_status_t w25q64_os_delay_ms(uint32_t ms)
{
    osal_task_delay(ms);
    return FLASH_HANLDER_OK;
}

/* ---- OS queue (directly coupled with osal_queue) ------------------------ */

/**
 * @brief  Create a message queue via OSAL.
 *
 * @param[in]  item_num      : Queue depth.
 * @param[in]  item_size     : Size of each item in bytes.
 * @param[out] queue_handler : Receives the queue handle.
 *
 * @return FLASH_HANLDER_OK on success, FLASH_HANLDER_ERROR on failure.
 */
static flash_handler_status_t w25q64_os_queue_create(
    uint32_t const   item_num,
    uint32_t const   item_size,
    void    **const  queue_handler)
{
    int32_t ret = osal_queue_create(
        (osal_queue_handle_t *)queue_handler,
        (size_t)item_num,
        (size_t)item_size);
    return (OSAL_SUCCESS == ret) ? FLASH_HANLDER_OK : FLASH_HANLDER_ERROR;
}

/**
 * @brief  Put a message into the queue via OSAL.
 *
 * @param[in] queue_handler : Queue handle.
 * @param[in] item          : Pointer to the item data.
 * @param[in] timeout       : Wait timeout in ticks.
 *
 * @return FLASH_HANLDER_OK on success, FLASH_HANLDER_ERROR on failure.
 */
static flash_handler_status_t w25q64_os_queue_put(
    void     *const  queue_handler,
    void     *const  item,
    uint32_t         timeout)
{
    int32_t ret = osal_queue_send(
        (osal_queue_handle_t)queue_handler,
        item,
        (osal_tick_type_t)timeout);
    return (OSAL_SUCCESS == ret) ? FLASH_HANLDER_OK : FLASH_HANLDER_ERROR;
}

/**
 * @brief  Get a message from the queue via OSAL.
 *
 * @param[in]  queue_handler : Queue handle.
 * @param[out] msg           : Pointer to receive the item data.
 * @param[in]  timeout       : Wait timeout in ticks.
 *
 * @return FLASH_HANLDER_OK on success, FLASH_HANLDER_ERROR on failure.
 */
static flash_handler_status_t w25q64_os_queue_get(
    void     *const  queue_handler,
    void     *const  msg,
    uint32_t         timeout)
{
    int32_t ret = osal_queue_receive(
        (osal_queue_handle_t)queue_handler,
        msg,
        (osal_tick_type_t)timeout);
    return (OSAL_SUCCESS == ret) ? FLASH_HANLDER_OK : FLASH_HANLDER_ERROR;
}

/**
 * @brief  Delete a queue via OSAL.
 *
 * @param[in] queue_handler : Queue handle.
 *
 * @return FLASH_HANLDER_OK always.
 */
static flash_handler_status_t w25q64_os_queue_delete(
    void *const queue_handler)
{
    osal_queue_delete((osal_queue_handle_t)queue_handler);
    return FLASH_HANLDER_OK;
}

/* ---- Assembled interface vtables ----------------------------------------- */

static w25q64_spi_interface_t s_spi_interface = {
    .pf_spi_init              = w25q64_spi_init,
    .pf_spi_deinit            = w25q64_spi_deinit,
    .pf_spi_transmit          = w25q64_spi_transmit,
    .pf_spi_read              = w25q64_spi_read,
    .pf_spi_transmit_dma      = w25q64_spi_transmit_dma,
    .pf_spi_wait_dma_complete = w25q64_spi_wait_dma_complete,
    .pf_spi_write_cs_pin      = w25q64_spi_write_cs_pin,
    .pf_spi_write_dc_pin      = w25q64_spi_write_dc_pin,
};

static w25q64_timebase_interface_t s_timebase_interface = {
    .pf_get_tick_ms = w25q64_tb_get_tick_ms,
    .pf_delay_ms    = w25q64_tb_delay_ms,
};

static w25q64_os_delay_t s_w25q64_os_delay = {
    .pf_os_delay_ms = w25q64_drv_delay_ms,
};

static flash_handler_os_queue_t s_os_queue_interface = {
    .pf_os_queue_create = w25q64_os_queue_create,
    .pf_os_queue_put    = w25q64_os_queue_put,
    .pf_os_queue_get    = w25q64_os_queue_get,
    .pf_os_queue_delete = w25q64_os_queue_delete,
};

static flash_handler_os_delay_t s_os_delay_interface = {
    .pf_os_delay_ms = w25q64_os_delay_ms,
};

static flash_os_interface_t s_os_interface = {
    .p_os_queue_interface = &s_os_queue_interface,
    .p_os_delay_interface = &s_os_delay_interface,
};

/* ---- Driver input arg ---------------------------------------------------- */

flash_input_args_t w25q64_input_arg = {
    .p_os_interface       = &s_os_interface,
    .p_spi_interface      = &s_spi_interface,
    .p_timebase_interface = &s_timebase_interface,
    .p_w25q64_os_delay    = &s_w25q64_os_delay,
};

//******************************* Functions *********************************//
