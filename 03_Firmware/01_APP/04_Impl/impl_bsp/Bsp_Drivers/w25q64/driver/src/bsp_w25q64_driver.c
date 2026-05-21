/******************************************************************************
 * @file    bsp_w25q64_driver.c
 *
 * @par dependencies
 * - bsp_w25q64_driver.h
 * - bsp_w25q64_reg.h
 * - Debug.h
 *
 * @author  Ethan-Hang
 *
 * @brief   W25Q64 SPI NOR flash driver implementation.
 *
 * Processing flow:
 * - Bind external interfaces via instantiation helper.
 * - Provide read/write/erase/sleep/wakeup operations via vtable.
 * - All hardware access goes through injected SPI vtable; no direct
 *   register manipulation.
 *
 * @version V1.0 2026-04-27
 *
 * @note    1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_w25q64_driver.h"
#include "bsp_w25q64_reg.h"
#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define W25Q64_IS_INITED                               true
#define W25Q64_NOT_INITED                             false

#define W25Q64_PIN_LOW                                   0U
#define W25Q64_PIN_HIGH                                  1U

/* Datasheet-mandated timings. */
#define W25Q64_DELAY_POWER_ON_MS                       50U
#define W25Q64_DELAY_WAKEUP_MS                          3U

/* Write-complete polling cadence.
 *
 * Chip Erase is the worst-case operation: 25 s typical, up to 100 s
 * max per the W25Q64 datasheet.  We yield between RDSR reads (instead
 * of spinning) so the SPI bus mutex is freed and other tasks can run.
 *
 *   timeout budget = INTERVAL_MS * MAX_RETRIES
 *   5 ms * 12000 = 60 s -- comfortably above typical chip erase, and
 *   above the test harness's 40 s timeout so the test still bounds the
 *   wait if hardware is genuinely stuck.  Sector erase / page program
 *   pay at most one extra 5 ms tick of latency, negligible vs their
 *   ~50 ms cycle time. */
#define W25Q64_WRITE_POLL_INTERVAL_MS                   5U
#define W25Q64_WRITE_POLL_MAX_RETRIES               12000U

/* W25Q64 Manufacturer / Device ID (cmd 0x90). */
#define W25Q64_MANUFACTURER_ID                        0xEFU
#define W25Q64_DEVICE_ID                              0x16U

/* Shortcuts to the driver-instance vtables. */
#define SPI_INSTANCE(driver)      ((driver)->p_spi_interface)
#define TIMEBASE_INSTANCE(driver) ((driver)->p_timebase_interface)
#define OS_INSTANCE(driver)       ((driver)->p_os_interface)
//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/**
 * @brief  Send a single command byte over SPI with CS framing.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] command         : Command byte to send.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t __w25q64_write_command(
    bsp_w25q64_driver_t *const driver_instance,
    uint8_t                     command)
{
    w25q64_status_t ret = W25Q64_OK;

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_LOW);

    ret = SPI_INSTANCE(driver_instance)->pf_spi_transmit(&command, 1U);

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_HIGH);

    return ret;
}

/**
 * @brief  Send command + 3-byte address, then transmit data payload.
 *         Used for Page Program (0x02) and similar write sequences.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] command         : SPI command byte.
 * @param[in] address         : 24-bit flash address.
 * @param[in] p_data          : Data buffer to transmit.
 * @param[in] data_length     : Number of bytes to transmit.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t __w25q64_write_command_addr_data(
    bsp_w25q64_driver_t *const driver_instance,
    uint8_t                     command,
    uint32_t                    address,
    uint8_t             const  *p_data,
    uint32_t                    data_length)
{
    w25q64_status_t ret = W25Q64_OK;
    uint8_t cmd_addr[4];

    cmd_addr[0] = command;
    cmd_addr[1] = (uint8_t)((address >> 16) & 0xFFU);
    cmd_addr[2] = (uint8_t)((address >>  8) & 0xFFU);
    cmd_addr[3] = (uint8_t)((address      ) & 0xFFU);

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_LOW);

    ret = SPI_INSTANCE(driver_instance)->pf_spi_transmit(cmd_addr, 4U);
    if (W25Q64_OK != ret)
    {
        SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(
            W25Q64_PIN_HIGH);
        return ret;
    }

    ret = SPI_INSTANCE(driver_instance)->pf_spi_transmit(p_data,
                                                         data_length);

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_HIGH);

    return ret;
}

/**
 * @brief  Send command + 3-byte address, then receive data.
 *         Used for Read Data (0x03) and Read ID (0x90).
 *
 * @param[in]  driver_instance : Driver object.
 * @param[in]  command         : SPI command byte.
 * @param[in]  address         : 24-bit flash address.
 * @param[out] p_buffer        : Buffer to store received data.
 * @param[in]  buffer_length   : Number of bytes to receive.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t __w25q64_read_command_addr_data(
    bsp_w25q64_driver_t *const driver_instance,
    uint8_t                     command,
    uint32_t                    address,
    uint8_t                    *p_buffer,
    uint32_t                    buffer_length)
{
    w25q64_status_t ret = W25Q64_OK;
    uint8_t cmd_addr[4];

    cmd_addr[0] = command;
    cmd_addr[1] = (uint8_t)((address >> 16) & 0xFFU);
    cmd_addr[2] = (uint8_t)((address >>  8) & 0xFFU);
    cmd_addr[3] = (uint8_t)((address      ) & 0xFFU);

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_LOW);

    ret = SPI_INSTANCE(driver_instance)->pf_spi_transmit(cmd_addr, 4U);
    if (W25Q64_OK != ret)
    {
        SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(
            W25Q64_PIN_HIGH);
        return ret;
    }

    ret = SPI_INSTANCE(driver_instance)->pf_spi_read(p_buffer,
                                                     buffer_length);

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_HIGH);

    return ret;
}

/**
 * @brief  Read Status Register-1 and check BUSY bit.
 *
 * @param[in]  driver_instance : Driver object.
 * @param[out] p_status        : Pointer to store the register value.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t __w25q64_read_status_reg(
    bsp_w25q64_driver_t *const driver_instance,
    uint8_t                    *p_status)
{
    w25q64_status_t ret = W25Q64_OK;

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_LOW);

    ret = SPI_INSTANCE(driver_instance)->pf_spi_transmit(
        &(uint8_t){W25Q64_CMD_READ_REG}, 1U);
    if (W25Q64_OK != ret)
    {
        SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(
            W25Q64_PIN_HIGH);
        return ret;
    }

    ret = SPI_INSTANCE(driver_instance)->pf_spi_read(p_status, 1U);

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_HIGH);

    return ret;
}

/**
 * @brief  Poll Status Register-1 until BUSY bit clears.
 *
 * @param[in] driver_instance : Driver object.
 *
 * @return W25Q64_OK on success, W25Q64_ERRORTIMEOUT if still busy
 *         after W25Q64_WRITE_POLL_MAX_RETRIES iterations.
 * */
static w25q64_status_t __w25q64_wait_write_complete(
    bsp_w25q64_driver_t *const driver_instance)
{
    w25q64_status_t ret = W25Q64_OK;
    uint8_t status = 0;
    uint32_t retries = 0;

    while (retries < W25Q64_WRITE_POLL_MAX_RETRIES)
    {
        ret = __w25q64_read_status_reg(driver_instance, &status);
        if (W25Q64_OK != ret)
        {
            return ret;
        }

        if (0U == (status & W25Q64_STATUS_BUSY))
        {
            return W25Q64_OK;
        }

        /**
         * Yield between polls so the SPI bus mutex is released and
         * other tasks can run.  Without this the loop spins at full
         * SPI clock and the ceiling collapses to a few hundred ms --
         * far below chip-erase tCE (25 s typ, 100 s max).
         **/
        TIMEBASE_INSTANCE(driver_instance)->pf_delay_ms(
            W25Q64_WRITE_POLL_INTERVAL_MS);

        retries++;
    }

    return W25Q64_ERRORTIMEOUT;
}

/**
 * @brief  Read JEDEC ID (0x9F) for device identification.
 *
 * @param[in]  driver_instance : Driver object.
 * @param[out] p_id_buffer     : Buffer to store 3-byte JEDEC ID.
 * @param[in]  buffer_length   : Must be >= 3.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t __w25q64_read_jedec_id(
    bsp_w25q64_driver_t *const driver_instance,
    uint8_t                    *p_id_buffer,
    uint32_t                    buffer_length)
{
    w25q64_status_t ret = W25Q64_OK;

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_LOW);

    ret = SPI_INSTANCE(driver_instance)->pf_spi_transmit(
        &(uint8_t){W25Q64_CMD_JEDEC_ID}, 1U);
    if (W25Q64_OK != ret)
    {
        SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(
            W25Q64_PIN_HIGH);
        return ret;
    }

    ret = SPI_INSTANCE(driver_instance)->pf_spi_read(p_id_buffer,
                                                     buffer_length);

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_HIGH);

    return ret;
}

/**
 * @brief  Erase one 4KB sector at the given address.
 *
 * Sector address must be 4KB-aligned.  Caller is responsible for
 * ensuring the address falls within the device address space.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] address         : Start address of the sector to erase.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t __w25q64_erase_sector(
    bsp_w25q64_driver_t *const driver_instance,
    uint32_t                    address)
{
    w25q64_status_t ret = W25Q64_OK;
    uint8_t cmd_buf[4];

    ret = __w25q64_write_command(driver_instance,
                                W25Q64_CMD_WRITE_ENABLE);
    if (W25Q64_OK != ret)
    {
        return ret;
    }

    cmd_buf[0] = W25Q64_CMD_ERASE_SECTOR;
    cmd_buf[1] = (uint8_t)((address >> 16) & 0xFFU);
    cmd_buf[2] = (uint8_t)((address >>  8) & 0xFFU);
    cmd_buf[3] = (uint8_t)((address      ) & 0xFFU);

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_LOW);

    ret = SPI_INSTANCE(driver_instance)->pf_spi_transmit(cmd_buf, 4U);

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_HIGH);

    if (W25Q64_OK != ret)
    {
        return ret;
    }

    return __w25q64_wait_write_complete(driver_instance);
}

/**
 * @brief  Run power-on sequence and verify device ID.
 *
 * @param[in] driver_instance : Driver object already populated by
 *                              w25q64_driver_inst().
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t w25q64_init(
    bsp_w25q64_driver_t *const driver_instance)
{
    /**
     * Input parameter check.
     * Validate the driver object and its mounted interfaces before
     * touching the hardware.
     **/
    if ((NULL == driver_instance) ||
        (NULL == driver_instance->p_spi_interface) ||
        (NULL == driver_instance->p_timebase_interface))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_init input error parameter");
        return W25Q64_ERRORPARAMETER;
    }

    DEBUG_OUT(i, W25Q64_LOG_TAG, "w25q64_init start");

    /**
     * Phase 1 : SPI bus init.
     * Deassert CS to a known idle state before the first transaction.
     **/
    SPI_INSTANCE(driver_instance)->pf_spi_init();
    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_HIGH);

    /**
     * Phase 2 : Power-on settle.
     * Datasheet requires >= 5 ms after VDD reaches minimum before the
     * first command; use 50 ms for margin.
     **/
    TIMEBASE_INSTANCE(driver_instance)->pf_delay_ms(
        W25Q64_DELAY_POWER_ON_MS);

    /**
     * Phase 3 : Verify device identity via JEDEC ID (0x9F).
     * W25Q64 returns manufacturer 0xEF, memory type 0x40, capacity 0x17.
     * A mismatch here almost certainly means wrong device, broken wiring,
     * or uninitialized SPI peripheral.
     **/
    {
        uint8_t id_buf[3] = {0};
        w25q64_status_t ret = __w25q64_read_jedec_id(
            driver_instance, id_buf, 3U);
        if (W25Q64_OK != ret)
        {
            DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                      "w25q64_init read jedec id failed");
            return ret;
        }

        if ((W25Q64_MANUFACTURER_ID != id_buf[0]) ||
            (0x40U != id_buf[1])                   ||
            (0x17U != id_buf[2]))
        {
            DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                      "w25q64_init id mismatch: "
                      "0x%02X 0x%02X 0x%02X",
                      id_buf[0], id_buf[1], id_buf[2]);
            return W25Q64_ERRORRESOURCE;
        }

        DEBUG_OUT(d, W25Q64_LOG_TAG,
                  "w25q64_init id verified: "
                  "0x%02X 0x%02X 0x%02X",
                  id_buf[0], id_buf[1], id_buf[2]);
    }

    DEBUG_OUT(i, W25Q64_LOG_TAG, "w25q64_init complete");
    return W25Q64_OK;
}

/**
 * @brief  Deassert CS and mark the instance uninitialized.
 *
 * @param[in] driver_instance : Driver object.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t w25q64_deinit(
    bsp_w25q64_driver_t *const driver_instance)
{
    if (NULL == driver_instance)
    {
        return W25Q64_ERRORPARAMETER;
    }

    SPI_INSTANCE(driver_instance)->pf_spi_write_cs_pin(W25Q64_PIN_HIGH);
    SPI_INSTANCE(driver_instance)->pf_spi_deinit();

    DEBUG_OUT(d, W25Q64_LOG_TAG, "w25q64_deinit success");
    return W25Q64_OK;
}

/**
 * @brief  Read device ID using command 0x90.
 *
 * Returns 2 bytes: manufacturer ID (byte 0) and device ID (byte 1).
 * The caller provides the buffer and its length; buffer_length must
 * be >= 2.
 *
 * @param[in]  driver_instance : Driver object.
 * @param[out] p_id_buffer     : Buffer to store the ID bytes.
 * @param[in]  buffer_length   : Size of p_id_buffer (>= 2).
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t w25q64_read_id(
    bsp_w25q64_driver_t *const driver_instance,
    uint8_t                    *p_id_buffer,
    uint32_t                    buffer_length)
{
    if ((NULL == driver_instance) ||
        (NULL == p_id_buffer)    ||
        (buffer_length < 2U))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_read_id invalid parameter");
        return W25Q64_ERRORPARAMETER;
    }

    return __w25q64_read_command_addr_data(
        driver_instance,
        W25Q64_CMD_READ_ID,
        0x000000U,
        p_id_buffer,
        buffer_length);
}

/**
 * @brief  Read data from flash at the specified address.
 *
 * @param[in]  driver_instance : Driver object.
 * @param[in]  address         : Start address in flash (0-based).
 * @param[out] p_data_buffer   : Buffer to store read data.
 * @param[in]  buffer_length   : Number of bytes to read.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t w25q64_read_data(
    bsp_w25q64_driver_t *const driver_instance,
    uint32_t                     address,
    uint8_t                     *p_data_buffer,
    uint32_t                     buffer_length)
{
    if ((NULL == driver_instance) ||
        (NULL == p_data_buffer)   ||
        (0U == buffer_length))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_read_data invalid parameter");
        return W25Q64_ERRORPARAMETER;
    }

    if ((address >= W25Q64_MAX_SIZE) ||
        (address + buffer_length > W25Q64_MAX_SIZE))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_read_data address out of range");
        return W25Q64_ERRORPARAMETER;
    }

    return __w25q64_read_command_addr_data(
        driver_instance,
        W25Q64_CMD_READ_DATA,
        address,
        p_data_buffer,
        buffer_length);
}

/**
 * @brief  Program up to 256 bytes at the given address without
 *         erasing first.
 *
 * The caller must ensure the target region has been erased
 * (0xFF) beforehand; bits can only be cleared (1->0) by a
 * Page Program command.
 *
 * W25Q64 Page Program allows up to 256 bytes per transaction.
 * If data_length > 256, the device wraps within the page.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] address         : Start address (should be page-aligned).
 * @param[in] p_data_buffer   : Data to program.
 * @param[in] data_length     : Number of bytes (1..256).
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t w25q64_write_data_noerase(
    bsp_w25q64_driver_t *const driver_instance,
    uint32_t                     address,
    uint8_t             const   *p_data_buffer,
    uint32_t                     data_length)
{
    w25q64_status_t ret = W25Q64_OK;

    if ((NULL == driver_instance) ||
        (NULL == p_data_buffer)   ||
        (0U == data_length))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_write_data_noerase invalid parameter");
        return W25Q64_ERRORPARAMETER;
    }

    if ((address >= W25Q64_MAX_SIZE) ||
        (address + data_length > W25Q64_MAX_SIZE))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_write_data_noerase address out of range");
        return W25Q64_ERRORPARAMETER;
    }

    /**
    * W25Q64 Page Program wraps addresses within a 256-byte page on
    * overflow — must split into page-bounded chunks. See companion
    * comment in w25q64_write_data_erase for full datasheet citation.
    * Write Enable must be re-issued before each Page Program; the WEL
    * bit auto-clears after each program completes.
    **/
    uint32_t bytes_written = 0U;
    while (bytes_written < data_length)
    {
        uint32_t cur_addr       = address + bytes_written;
        uint32_t offset_in_page = cur_addr & (W25Q64_PAGE_SIZE - 1U);
        uint32_t space_in_page  = W25Q64_PAGE_SIZE - offset_in_page;
        uint32_t remaining      = data_length - bytes_written;
        uint32_t chunk_size = (remaining < space_in_page) ? remaining
                                                          : space_in_page;

        ret = __w25q64_write_command(driver_instance,
                                     W25Q64_CMD_WRITE_ENABLE);
        if (W25Q64_OK != ret)
        {
            return ret;
        }

        ret = __w25q64_write_command_addr_data(
            driver_instance,
            W25Q64_CMD_WRITE_DATA,
            cur_addr,
            &p_data_buffer[bytes_written],
            chunk_size);
        if (W25Q64_OK != ret)
        {
            return ret;
        }

        ret = __w25q64_wait_write_complete(driver_instance);
        if (W25Q64_OK != ret)
        {
            return ret;
        }

        bytes_written += chunk_size;
    }

    return W25Q64_OK;
}

/**
 * @brief  Erase the containing 4KB sector, then program data.
 *
 * Convenience wrapper: erases the sector(s) covering [address,
 * address + data_length) and then programs the data.  The caller
 * does not need to pre-erase.
 *
 * For data spanning multiple sectors, each 4KB sector boundary
 * triggers a separate erase-program cycle.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] address         : Start address in flash.
 * @param[in] p_data_buffer   : Data to program.
 * @param[in] data_length     : Number of bytes.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t w25q64_write_data_erase(
    bsp_w25q64_driver_t *const driver_instance,
    uint32_t                     address,
    uint8_t             const   *p_data_buffer,
    uint32_t                     data_length)
{
    w25q64_status_t ret = W25Q64_OK;
    uint32_t bytes_written = 0;

    if ((NULL == driver_instance) ||
        (NULL == p_data_buffer)   ||
        (0U == data_length))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_write_data_erase invalid parameter");
        return W25Q64_ERRORPARAMETER;
    }

    if ((address >= W25Q64_MAX_SIZE) ||
        (address + data_length > W25Q64_MAX_SIZE))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_write_data_erase address out of range");
        return W25Q64_ERRORPARAMETER;
    }

    /**
    * W25Q64 Page Program (0x02) writes UP TO 256 bytes within one page
    * (256-byte aligned). If the host sends past the page boundary, the
    * chip wraps the address back to the page start and overwrites bytes
    * already programmed in that page (see W25Q64FW datasheet, "Page
    * Program" section). So we must:
    *   1. Erase the 4 KB sector once per sector
    *   2. Break writes into page-aligned chunks of at most PAGE_SIZE
    *
    * Bug hit on 2026-05-17: a single 4096 B Page Program landed the
    * trailing 256 bytes of the buffer at addresses 0..255, the rest
    * overwritten in place — bootloader's OTA decrypt saw .mxxx[3840..]
    * at offset 0 and rejected the image.
    **/
    while (bytes_written < data_length)
    {
        uint32_t cur_addr = address + bytes_written;

        /* Erase the 4 KB sector only on entering a new sector. */
        if (((cur_addr) & (W25Q64_SECTOR_SIZE - 1U)) == 0U)
        {
            uint32_t sector_base = cur_addr & ~(W25Q64_SECTOR_SIZE - 1U);
            ret = __w25q64_erase_sector(driver_instance, sector_base);
            if (W25Q64_OK != ret)
            {
                return ret;
            }
        }
        else if (bytes_written == 0U)
        {
            /* First chunk lands in the middle of a sector — erase the
               containing sector before any write touches it. */
            uint32_t sector_base = cur_addr & ~(W25Q64_SECTOR_SIZE - 1U);
            ret = __w25q64_erase_sector(driver_instance, sector_base);
            if (W25Q64_OK != ret)
            {
                return ret;
            }
        }

        /* Per-page-program chunk: limited by both the remaining bytes,
           the rest of the current page, and the rest of the current
           sector (we re-erase per sector). */
        uint32_t offset_in_page = cur_addr & (W25Q64_PAGE_SIZE - 1U);
        uint32_t space_in_page  = W25Q64_PAGE_SIZE - offset_in_page;
        uint32_t offset_in_sector = cur_addr & (W25Q64_SECTOR_SIZE - 1U);
        uint32_t space_in_sector  = W25Q64_SECTOR_SIZE - offset_in_sector;
        uint32_t remaining = data_length - bytes_written;
        uint32_t chunk_size = remaining;
        if (chunk_size > space_in_page)   { chunk_size = space_in_page; }
        if (chunk_size > space_in_sector) { chunk_size = space_in_sector; }

        /* Write Enable + Page Program + poll BUSY. */
        ret = __w25q64_write_command(
            driver_instance, W25Q64_CMD_WRITE_ENABLE);
        if (W25Q64_OK != ret)
        {
            return ret;
        }

        ret = __w25q64_write_command_addr_data(
            driver_instance,
            W25Q64_CMD_WRITE_DATA,
            cur_addr,
            &p_data_buffer[bytes_written],
            chunk_size);
        if (W25Q64_OK != ret)
        {
            return ret;
        }

        ret = __w25q64_wait_write_complete(driver_instance);
        if (W25Q64_OK != ret)
        {
            return ret;
        }

        bytes_written += chunk_size;
    }

    return W25Q64_OK;
}

/**
 * @brief  Erase the entire flash chip.
 *
 * Chip Erase takes 25-50 s typical (up to 100 s max) for W25Q64.
 * This is a blocking call that polls until the BUSY bit clears.
 *
 * @param[in] driver_instance : Driver object.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t w25q64_erase_chip(
    bsp_w25q64_driver_t *const driver_instance)
{
    w25q64_status_t ret = W25Q64_OK;

    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_erase_chip invalid parameter");
        return W25Q64_ERRORPARAMETER;
    }

    ret = __w25q64_write_command(driver_instance,
                                W25Q64_CMD_WRITE_ENABLE);
    if (W25Q64_OK != ret)
    {
        return ret;
    }

    ret = __w25q64_write_command(driver_instance,
                                W25Q64_CMD_CHIP_ERASE);
    if (W25Q64_OK != ret)
    {
        return ret;
    }

    DEBUG_OUT(i, W25Q64_LOG_TAG,
              "w25q64_erase_chip started, polling BUSY...");

    return __w25q64_wait_write_complete(driver_instance);
}

/**
 * @brief  Public wrapper for sector erase: validate, align, dispatch.
 *
 * Erases the 4KB sector containing @p address.  The address is
 * automatically aligned down to the sector boundary, so callers may
 * pass any address inside the target sector.
 *
 * @param[in] driver_instance : Driver object.
 * @param[in] address         : Any address inside the target sector.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t w25q64_erase_sector(
    bsp_w25q64_driver_t *const driver_instance,
    uint32_t                    address)
{
    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_erase_sector invalid parameter");
        return W25Q64_ERRORPARAMETER;
    }

    if (address >= W25Q64_MAX_SIZE)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_erase_sector address out of range");
        return W25Q64_ERRORPARAMETER;
    }

    address = address & ~(W25Q64_SECTOR_SIZE - 1U);

    return __w25q64_erase_sector(driver_instance, address);
}

/**
 * @brief  Enter deep power-down mode to minimize standby current.
 *
 * Standby current drops from ~25 uA to ~1 uA in deep power-down.
 * No read/write/erase operations are available until Release
 * Power-Down (0xAB) is issued.
 *
 * @param[in] driver_instance : Driver object.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t w25q64_sleep(
    bsp_w25q64_driver_t *const driver_instance)
{
    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_sleep invalid parameter");
        return W25Q64_ERRORPARAMETER;
    }

    return __w25q64_write_command(driver_instance,
                                 W25Q64_CMD_SLEEP);
}

/**
 * @brief  Release from deep power-down mode.
 *
 * After the Release Power-Down command (0xAB), the device needs
 * ~3 us before it is ready for normal operation.
 *
 * @param[in] driver_instance : Driver object.
 *
 * @return W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t w25q64_wakeup(
    bsp_w25q64_driver_t *const driver_instance)
{
    w25q64_status_t ret = W25Q64_OK;

    if (NULL == driver_instance)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_wakeup invalid parameter");
        return W25Q64_ERRORPARAMETER;
    }

    ret = __w25q64_write_command(driver_instance,
                                W25Q64_CMD_WAKEUP);
    if (W25Q64_OK != ret)
    {
        return ret;
    }

    /* Datasheet: tRES1 = 3 us typical.  Busy-wait since this may
     * run before the RTOS scheduler starts. */
    TIMEBASE_INSTANCE(driver_instance)->pf_delay_ms(
        W25Q64_DELAY_WAKEUP_MS);

    return W25Q64_OK;
}

/**
 * @brief  Instantiate a W25Q64 driver object: validate caller-supplied
 *         interfaces, bind them into the driver instance, and mount
 *         the public API vtable.  Does NOT touch the hardware - the
 *         caller must invoke pf_w25q64_init() afterwards to run the
 *         power-on reset and device-ID verification sequence.
 *
 * @param[out] p_w25q64_inst        Driver object to populate.
 * @param[in]  p_spi_interface      Raw SPI / CS vtable.
 * @param[in]  p_timebase_interface ms-tick / busy-wait delay vtable.
 * @param[in]  p_os_interface       OS-aware delay vtable.
 *
 * @return W25Q64_OK                 - Success.
 *         W25Q64_ERRORPARAMETER     - NULL pointer.
 *         W25Q64_ERRORRESOURCE      - Instance already initialized, or a
 *                                     required vtable slot is NULL.
 * */
w25q64_status_t w25q64_driver_inst(
    bsp_w25q64_driver_t         * const        p_w25q64_inst,
    w25q64_spi_interface_t      * const      p_spi_interface,
    w25q64_timebase_interface_t * const p_timebase_interface,
    w25q64_os_delay_t           * const       p_os_interface)
{
    w25q64_status_t ret = W25Q64_OK;

    DEBUG_OUT(i, W25Q64_LOG_TAG, "w25q64_driver_inst start");

    /************ 1.Checking input parameters ************/
    if ((NULL == p_w25q64_inst)     ||
        (NULL == p_spi_interface)   ||
        (NULL == p_timebase_interface) ||
        (NULL == p_os_interface))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_driver_inst input error parameter");
        return W25Q64_ERRORPARAMETER;
    }

    /************* 2.Checking the Resources **************/
    /**
     * Verify every vtable slot the driver will actually call.
     * Do this on the caller-supplied pointers directly, BEFORE
     * mounting them onto the driver instance, so a partial bind
     * never leaks into the instance on failure.
     **/
    if ((NULL == p_spi_interface->pf_spi_init)      ||
        (NULL == p_spi_interface->pf_spi_deinit)    ||
        (NULL == p_spi_interface->pf_spi_transmit)  ||
        (NULL == p_spi_interface->pf_spi_read)      ||
        (NULL == p_spi_interface->pf_spi_write_cs_pin))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_driver_inst spi_interface has NULL callback");
        return W25Q64_ERRORRESOURCE;
    }

    if ((NULL == p_timebase_interface->pf_get_tick_ms) ||
        (NULL == p_timebase_interface->pf_delay_ms))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_driver_inst timebase has NULL callback");
        return W25Q64_ERRORRESOURCE;
    }

    if (NULL == p_os_interface->pf_os_delay_ms)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "w25q64_driver_inst os_interface has NULL callback");
        return W25Q64_ERRORRESOURCE;
    }

    /************** 3.Mount the Interfaces ***************/
    /* 3.1 Mount external interfaces. */
    p_w25q64_inst->p_spi_interface       =       p_spi_interface;
    p_w25q64_inst->p_timebase_interface  =  p_timebase_interface;
    p_w25q64_inst->p_os_interface        =        p_os_interface;

    /**
     * 3.2 Mount internal API vtable.
     * Each slot points to a file-local function; call-sites dispatch
     * via driver_instance->pf_xxx(driver_instance, ...).
     **/
    p_w25q64_inst->pf_w25q64_init              =              w25q64_init;
    p_w25q64_inst->pf_w25q64_deinit            =            w25q64_deinit;
    p_w25q64_inst->pf_w25q64_read_id           =           w25q64_read_id;
    p_w25q64_inst->pf_w25q64_read_data         =         w25q64_read_data;
    p_w25q64_inst->pf_w25q64_write_data_noerase =
                                               w25q64_write_data_noerase;
    p_w25q64_inst->pf_w25q64_write_data_erase  =  w25q64_write_data_erase;
    p_w25q64_inst->pf_w25q64_erase_chip        =        w25q64_erase_chip;
    p_w25q64_inst->pf_w25q64_erase_sector      =      w25q64_erase_sector;
    p_w25q64_inst->pf_w25q64_sleep             =             w25q64_sleep;
    p_w25q64_inst->pf_w25q64_wakeup            =            w25q64_wakeup;

    DEBUG_OUT(d, W25Q64_LOG_TAG, "w25q64_driver_inst success");
    return ret;
}

//******************************* Functions *********************************/
