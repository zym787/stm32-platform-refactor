/******************************************************************************
 * @file w25q64_mock_test_task.c
 *
 * @par dependencies
 * - user_task_reso_config.h
 * - bsp_w25q64_driver.h
 * - bsp_w25q64_reg.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief W25Q64 driver logic validation without a real SPI NOR flash.
 *
 * Processing flow:
 * Mount fake SPI / timebase / OS-delay vtables that drive a CS-bracketed
 * transaction state machine modelling the W25Q64.  Every CS edge commits
 * the pending command (Write Enable, Page Program, Sector / Chip Erase,
 * Sleep, Wakeup) and Read commands serve fake JEDEC ID, status register
 * BUSY decay, or backing-store bytes.  Each test case self-asserts on
 * captured opcode / address / payload counters so a clean log becomes a
 * go/no-go signal instead of "eyeball against the datasheet".
 *
 * @version V1.0 2026-04-27
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "user_task_reso_config.h"
#include "bsp_w25q64_driver.h"
#include "bsp_w25q64_reg.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#ifndef W25Q64_MOCK_LOG_TAG
#define W25Q64_MOCK_LOG_TAG                  "W25Q64_MOCK"
#endif
#ifndef W25Q64_MOCK_ERR_LOG_TAG
#define W25Q64_MOCK_ERR_LOG_TAG              "W25Q64_MOCK_ERR"
#endif

#define W25Q64_MOCK_BOOT_WAIT_MS             2500u
#define W25Q64_MOCK_STEP_GAP_MS               200u

#define W25Q64_MOCK_PIN_LOW                     0u
#define W25Q64_MOCK_PIN_HIGH                    1u

/* Fake-flash backing store: just enough to cover the addresses the cases
 * below touch (we exercise sector boundary + a couple of payloads).      */
#define W25Q64_MOCK_FAKE_SIZE              (W25Q64_SECTOR_SIZE * 4u)

/* Fake-data-in capture buffer: must fit Page Program payload sizes used
 * by the cases below (2 sectors-worth covers spanning writes).           */
#define W25Q64_MOCK_DATA_IN_CAP             (W25Q64_PAGE_SIZE * 2u)

/* Test addresses / payload sizes. */
#define W25Q64_MOCK_ADDR_READ_SEED        (0x00001000u)
#define W25Q64_MOCK_ADDR_NOERASE          (0x00002000u)
#define W25Q64_MOCK_ADDR_ERASE            (0x00003000u)
#define W25Q64_MOCK_ADDR_E2E              (0x00000000u)
#define W25Q64_MOCK_PAYLOAD_LEN                 32u

/* W25Q64 datasheet identity (Manufacturer / Memory type / Capacity). */
#define W25Q64_MOCK_JEDEC_MFR                 0xEFu
#define W25Q64_MOCK_JEDEC_TYPE                0x40u
#define W25Q64_MOCK_JEDEC_CAP                 0x17u

/* Read ID (cmd 0x90) returns Manufacturer + Device ID. */
#define W25Q64_MOCK_DEV_ID                    0x16u
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static bsp_w25q64_driver_t          s_mock_driver;
static w25q64_spi_interface_t       s_mock_spi;
static w25q64_timebase_interface_t  s_mock_tb;
static w25q64_os_delay_t            s_mock_os;

/**
 * Per-case capture: every SPI op lands here so the case body can assert
 * on opcode, address, payload length and call counts.  The fake_flash
 * array is the synthetic device backing store; commit hooks (run on CS
 * rising edge) apply Write Enable / Page Program / Erase semantics.
 */
typedef struct
{
    uint8_t  fake_flash[W25Q64_MOCK_FAKE_SIZE];

    /* CS-framed transaction state. */
    bool     cs_active;
    bool     cmd_latched;
    uint8_t  current_cmd;
    uint8_t  addr_bytes_collected;
    uint32_t addr_value;
    uint8_t  data_in[W25Q64_MOCK_DATA_IN_CAP];
    uint32_t data_in_len;

    /* Device flags. */
    bool     wel;
    bool     sleeping;
    uint32_t busy_polls_remaining;

    /* Fault injection. */
    bool     force_jedec_mismatch;
    bool     force_busy_forever;

    /* Counters. */
    uint32_t spi_init_count;
    uint32_t spi_deinit_count;
    uint32_t cs_low_count;
    uint32_t cs_high_count;
    uint32_t transmit_count;
    uint32_t read_count;
    uint32_t delay_ms_count;
    uint32_t os_delay_ms_count;
    uint32_t last_delay_ms;

    /* Per-command observability. */
    uint32_t we_count;
    uint32_t pp_count;
    uint32_t sector_erase_count;
    uint32_t chip_erase_count;
    uint32_t sleep_count;
    uint32_t wakeup_count;
} mock_state_t;

static mock_state_t s_st;
static uint32_t     s_fake_tick_ms;

static uint32_t     s_pass_count;
static uint32_t     s_fail_count;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
static void state_reset(void)
{
    memset(&s_st, 0, sizeof(s_st));
    memset(s_st.fake_flash, 0xFF, sizeof(s_st.fake_flash));
}

/* ---- Transaction commit (runs on CS rising edge) ------------------------ */
/**
 * @brief      Apply pending command effects when CS goes high.
 *
 * @return     None.
 * */
static void mock_commit(void)
{
    switch (s_st.current_cmd)
    {
    case W25Q64_CMD_WRITE_ENABLE:
        s_st.wel = true;
        s_st.we_count++;
        break;

    case W25Q64_CMD_ERASE_SECTOR:
        if ((true == s_st.wel) && (3u == s_st.addr_bytes_collected))
        {
            uint32_t base = s_st.addr_value & ~(W25Q64_SECTOR_SIZE - 1u);
            if ((base + W25Q64_SECTOR_SIZE) <= sizeof(s_st.fake_flash))
            {
                memset(&s_st.fake_flash[base], 0xFF, W25Q64_SECTOR_SIZE);
            }
            s_st.busy_polls_remaining = 2u;
            s_st.wel                  = false;
            s_st.sector_erase_count++;
        }
        break;

    case W25Q64_CMD_WRITE_DATA:
        if ((true == s_st.wel)                       &&
            (3u == s_st.addr_bytes_collected)        &&
            (s_st.data_in_len > 0u)                  &&
            ((s_st.addr_value + s_st.data_in_len) <=
                                            sizeof(s_st.fake_flash)))
        {
            /* Page Program is bitwise-AND (only 1 -> 0). */
            for (uint32_t i = 0u; i < s_st.data_in_len; i++)
            {
                s_st.fake_flash[s_st.addr_value + i] &= s_st.data_in[i];
            }
            s_st.busy_polls_remaining = 2u;
            s_st.wel                  = false;
            s_st.pp_count++;
        }
        break;

    case W25Q64_CMD_CHIP_ERASE:
        if (true == s_st.wel)
        {
            memset(s_st.fake_flash, 0xFF, sizeof(s_st.fake_flash));
            s_st.busy_polls_remaining = 2u;
            s_st.wel                  = false;
            s_st.chip_erase_count++;
        }
        break;

    case W25Q64_CMD_SLEEP:
        s_st.sleeping = true;
        s_st.sleep_count++;
        break;

    case W25Q64_CMD_WAKEUP:
        s_st.sleeping = false;
        s_st.wakeup_count++;
        break;

    default:
        break;
    }
}

/* ---- SPI mock ----------------------------------------------------------- */
static w25q64_status_t mock_spi_init(void)
{
    s_st.spi_init_count++;
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG, "mock spi_init");
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_deinit(void)
{
    s_st.spi_deinit_count++;
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG, "mock spi_deinit");
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_write_cs_pin(uint8_t state)
{
    if (W25Q64_MOCK_PIN_LOW == state)
    {
        s_st.cs_low_count++;
        s_st.cs_active             = true;
        s_st.cmd_latched           = false;
        s_st.current_cmd           = 0u;
        s_st.addr_bytes_collected  = 0u;
        s_st.addr_value            = 0u;
        s_st.data_in_len           = 0u;
    }
    else
    {
        s_st.cs_high_count++;
        if (true == s_st.cs_active)
        {
            mock_commit();
        }
        s_st.cs_active = false;
    }
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_transmit(uint8_t const *p_data,
                                         uint32_t       data_length)
{
    if ((NULL == p_data) || (0u == data_length) || (false == s_st.cs_active))
    {
        return W25Q64_ERROR;
    }

    s_st.transmit_count++;

    uint32_t i = 0u;
    if (false == s_st.cmd_latched)
    {
        s_st.current_cmd = p_data[0];
        s_st.cmd_latched = true;
        i                = 1u;
    }

    /* Commands consuming a 3-byte address. */
    bool needs_addr =
        (W25Q64_CMD_READ_ID      == s_st.current_cmd) ||
        (W25Q64_CMD_READ_DATA    == s_st.current_cmd) ||
        (W25Q64_CMD_ERASE_SECTOR == s_st.current_cmd) ||
        (W25Q64_CMD_WRITE_DATA   == s_st.current_cmd);

    while ((i < data_length)                  &&
           (true == needs_addr)               &&
           (s_st.addr_bytes_collected < 3u))
    {
        s_st.addr_value =
            (s_st.addr_value << 8) | (uint32_t)p_data[i];
        s_st.addr_bytes_collected++;
        i++;
    }

    while ((i < data_length) &&
           (s_st.data_in_len < sizeof(s_st.data_in)))
    {
        s_st.data_in[s_st.data_in_len++] = p_data[i++];
    }

    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "mock transmit cmd=0x%02X addr=0x%06X len=%u",
              (unsigned)s_st.current_cmd, (unsigned)s_st.addr_value,
              (unsigned)data_length);
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_read(uint8_t *p_buffer,
                                     uint32_t buffer_length)
{
    if ((NULL == p_buffer)            ||
        (0u == buffer_length)         ||
        (false == s_st.cs_active)     ||
        (false == s_st.cmd_latched))
    {
        return W25Q64_ERROR;
    }

    s_st.read_count++;

    switch (s_st.current_cmd)
    {
    case W25Q64_CMD_JEDEC_ID:
    {
        uint8_t id[3];
        if (true == s_st.force_jedec_mismatch)
        {
            id[0] = 0xAAu;  id[1] = 0xBBu;  id[2] = 0xCCu;
        }
        else
        {
            id[0] = W25Q64_MOCK_JEDEC_MFR;
            id[1] = W25Q64_MOCK_JEDEC_TYPE;
            id[2] = W25Q64_MOCK_JEDEC_CAP;
        }
        for (uint32_t i = 0u; i < buffer_length; i++)
        {
            p_buffer[i] = id[i % 3u];
        }
        break;
    }

    case W25Q64_CMD_READ_ID:
    {
        uint8_t id[2] = { W25Q64_MOCK_JEDEC_MFR, W25Q64_MOCK_DEV_ID };
        for (uint32_t i = 0u; i < buffer_length; i++)
        {
            p_buffer[i] = id[i % 2u];
        }
        break;
    }

    case W25Q64_CMD_READ_REG:
    {
        uint8_t status = 0u;
        if (true == s_st.force_busy_forever)
        {
            status = W25Q64_STATUS_BUSY;
        }
        else if (s_st.busy_polls_remaining > 0u)
        {
            status = W25Q64_STATUS_BUSY;
            s_st.busy_polls_remaining--;
        }
        else
        {
            status = 0u;
        }
        for (uint32_t i = 0u; i < buffer_length; i++)
        {
            p_buffer[i] = status;
        }
        break;
    }

    case W25Q64_CMD_READ_DATA:
    {
        uint32_t base = s_st.addr_value;
        for (uint32_t i = 0u; i < buffer_length; i++)
        {
            uint32_t a = base + i;
            p_buffer[i] = (a < sizeof(s_st.fake_flash))
                            ? s_st.fake_flash[a]
                            : 0xFFu;
        }
        break;
    }

    default:
        memset(p_buffer, 0, buffer_length);
        break;
    }

    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "mock read cmd=0x%02X len=%u", (unsigned)s_st.current_cmd,
              (unsigned)buffer_length);
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_transmit_dma(uint8_t const *p_data,
                                             uint32_t       data_length)
{
    (void)p_data;
    (void)data_length;
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_wait_dma_complete(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return W25Q64_OK;
}

static w25q64_status_t mock_spi_write_dc_pin(uint8_t state)
{
    (void)state;
    return W25Q64_OK;
}

/* ---- Timebase / OS mocks ------------------------------------------------ */
static uint32_t mock_get_tick_ms(void)
{
    return s_fake_tick_ms;
}

static void mock_delay_ms(uint32_t ms)
{
    s_st.delay_ms_count++;
    s_st.last_delay_ms = ms;
    s_fake_tick_ms    += ms;
}

static void mock_os_delay_ms(uint32_t ms)
{
    s_st.os_delay_ms_count++;
    s_fake_tick_ms += ms;
}

/* ---- Wire-up ------------------------------------------------------------ */
/**
 * @brief      Bind mock vtables and instantiate the driver object.
 *
 * @return     w25q64_status_t W25Q64_OK on success, error code otherwise.
 * */
static w25q64_status_t mock_driver_bind(void)
{
    s_mock_spi.pf_spi_init              = mock_spi_init;
    s_mock_spi.pf_spi_deinit            = mock_spi_deinit;
    s_mock_spi.pf_spi_transmit          = mock_spi_transmit;
    s_mock_spi.pf_spi_read              = mock_spi_read;
    s_mock_spi.pf_spi_transmit_dma      = mock_spi_transmit_dma;
    s_mock_spi.pf_spi_wait_dma_complete = mock_spi_wait_dma_complete;
    s_mock_spi.pf_spi_write_cs_pin      = mock_spi_write_cs_pin;
    s_mock_spi.pf_spi_write_dc_pin      = mock_spi_write_dc_pin;

    s_mock_tb.pf_get_tick_ms            = mock_get_tick_ms;
    s_mock_tb.pf_delay_ms               = mock_delay_ms;

    s_mock_os.pf_os_delay_ms            = mock_os_delay_ms;

    memset(&s_mock_driver, 0, sizeof(s_mock_driver));
    return w25q64_driver_inst(&s_mock_driver, &s_mock_spi, &s_mock_tb,
                              &s_mock_os);
}

/* ---- Pass / fail helpers ------------------------------------------------ */
static void case_report(const char *name, bool ok)
{
    if (true == ok)
    {
        s_pass_count++;
        DEBUG_OUT(d, W25Q64_MOCK_LOG_TAG, "[PASS] %s", name);
    }
    else
    {
        s_fail_count++;
        DEBUG_OUT(e, W25Q64_MOCK_ERR_LOG_TAG,
                  "[FAIL] %s  tx=%u rx=%u cs_lo=%u cs_hi=%u we=%u pp=%u "
                  "se=%u ce=%u",
                  name,
                  (unsigned)s_st.transmit_count, (unsigned)s_st.read_count,
                  (unsigned)s_st.cs_low_count,   (unsigned)s_st.cs_high_count,
                  (unsigned)s_st.we_count,       (unsigned)s_st.pp_count,
                  (unsigned)s_st.sector_erase_count,
                  (unsigned)s_st.chip_erase_count);
    }
}

/* ---- Cases -------------------------------------------------------------- */
static void test_case_inst_null_args(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 1: w25q64_driver_inst NULL args -> "
              "ERRORPARAMETER =====");
    state_reset();

    bsp_w25q64_driver_t drv;
    bool ok =
      (W25Q64_ERRORPARAMETER ==
            w25q64_driver_inst(NULL, &s_mock_spi, &s_mock_tb, &s_mock_os))    &&
      (W25Q64_ERRORPARAMETER ==
            w25q64_driver_inst(&drv, NULL,        &s_mock_tb, &s_mock_os))    &&
      (W25Q64_ERRORPARAMETER ==
            w25q64_driver_inst(&drv, &s_mock_spi, NULL,       &s_mock_os))    &&
      (W25Q64_ERRORPARAMETER ==
            w25q64_driver_inst(&drv, &s_mock_spi, &s_mock_tb, NULL));
    case_report("CASE 1 inst NULL args", ok);
}

static void test_case_inst_missing_cb(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 2: missing required SPI callback -> "
              "ERRORRESOURCE =====");
    state_reset();

    bsp_w25q64_driver_t          drv;
    w25q64_spi_interface_t       spi = s_mock_spi;
    spi.pf_spi_transmit              = NULL;

    bool ok = (W25Q64_ERRORRESOURCE ==
               w25q64_driver_inst(&drv, &spi, &s_mock_tb, &s_mock_os));
    case_report("CASE 2 inst missing cb", ok);
}

static void test_case_init_success(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 3: pf_w25q64_init -- spi_init + 50ms delay + "
              "JEDEC verify =====");
    state_reset();

    w25q64_status_t ret = s_mock_driver.pf_w25q64_init(&s_mock_driver);

    bool ok = (W25Q64_OK == ret)                       &&
              (1u == s_st.spi_init_count)              &&
              (1u == s_st.delay_ms_count)              &&
              (50u == s_st.last_delay_ms)              &&
              (s_st.transmit_count >= 1u)              &&
              (s_st.read_count >= 1u);
    case_report("CASE 3 init success", ok);
}

static void test_case_init_jedec_mismatch(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 4: init with bad JEDEC ID -> ERRORRESOURCE =====");
    state_reset();
    s_st.force_jedec_mismatch = true;

    w25q64_status_t ret = s_mock_driver.pf_w25q64_init(&s_mock_driver);

    bool ok = (W25Q64_ERRORRESOURCE == ret);
    case_report("CASE 4 init jedec mismatch", ok);
}

static void test_case_init_null_arg(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 5: pf_w25q64_init(NULL) -> ERRORPARAMETER =====");
    state_reset();

    w25q64_status_t ret = s_mock_driver.pf_w25q64_init(NULL);

    bool ok = (W25Q64_ERRORPARAMETER == ret) &&
              (0u == s_st.transmit_count)    &&
              (0u == s_st.read_count);
    case_report("CASE 5 init NULL arg", ok);
}

static void test_case_read_id_invalid_param(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 6: pf_w25q64_read_id invalid param =====");
    state_reset();

    uint8_t buf[2];
    bool ok =
        (W25Q64_ERRORPARAMETER ==
            s_mock_driver.pf_w25q64_read_id(NULL, buf, 2u))           &&
        (W25Q64_ERRORPARAMETER ==
            s_mock_driver.pf_w25q64_read_id(&s_mock_driver, NULL, 2u)) &&
        (W25Q64_ERRORPARAMETER ==
            s_mock_driver.pf_w25q64_read_id(&s_mock_driver, buf, 1u));
    case_report("CASE 6 read_id invalid", ok);
}

static void test_case_read_id_returns_id(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 7: pf_w25q64_read_id returns 0xEF / 0x16 =====");
    state_reset();

    uint8_t buf[2] = { 0u, 0u };
    w25q64_status_t ret =
        s_mock_driver.pf_w25q64_read_id(&s_mock_driver, buf, 2u);

    bool ok = (W25Q64_OK == ret)                  &&
              (W25Q64_CMD_READ_ID == s_st.current_cmd) &&
              (W25Q64_MOCK_JEDEC_MFR == buf[0])   &&
              (W25Q64_MOCK_DEV_ID    == buf[1]);
    case_report("CASE 7 read_id returns id", ok);
}

static void test_case_read_data_invalid_param(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 8: pf_w25q64_read_data invalid param =====");
    state_reset();

    uint8_t buf[16];
    bool ok =
      (W25Q64_ERRORPARAMETER ==
        s_mock_driver.pf_w25q64_read_data(NULL, 0u, buf, 16u))             &&
      (W25Q64_ERRORPARAMETER ==
        s_mock_driver.pf_w25q64_read_data(&s_mock_driver, 0u, NULL, 16u))  &&
      (W25Q64_ERRORPARAMETER ==
        s_mock_driver.pf_w25q64_read_data(&s_mock_driver, 0u, buf, 0u))    &&
      (W25Q64_ERRORPARAMETER ==
        s_mock_driver.pf_w25q64_read_data(&s_mock_driver,
                                          W25Q64_MAX_SIZE, buf, 1u));
    case_report("CASE 8 read_data invalid", ok);
}

static void test_case_read_data_returns_flash(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 9: pf_w25q64_read_data returns backing-store "
              "bytes =====");
    state_reset();

    for (uint32_t i = 0u; i < W25Q64_MOCK_PAYLOAD_LEN; i++)
    {
        s_st.fake_flash[W25Q64_MOCK_ADDR_READ_SEED + i] = (uint8_t)(0xA0u + i);
    }

    uint8_t buf[W25Q64_MOCK_PAYLOAD_LEN] = { 0u };
    w25q64_status_t ret = s_mock_driver.pf_w25q64_read_data(
        &s_mock_driver, W25Q64_MOCK_ADDR_READ_SEED, buf,
        W25Q64_MOCK_PAYLOAD_LEN);

    bool ok = (W25Q64_OK == ret) &&
              (W25Q64_CMD_READ_DATA == s_st.current_cmd) &&
              (W25Q64_MOCK_ADDR_READ_SEED == s_st.addr_value);
    if (true == ok)
    {
        for (uint32_t i = 0u; i < W25Q64_MOCK_PAYLOAD_LEN; i++)
        {
            if ((uint8_t)(0xA0u + i) != buf[i])
            {
                ok = false;
                break;
            }
        }
    }
    case_report("CASE 9 read_data returns flash", ok);
}

static void test_case_write_noerase_invalid(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 10: pf_w25q64_write_data_noerase invalid "
              "param =====");
    state_reset();

    uint8_t data[4] = { 1u, 2u, 3u, 4u };
    bool ok =
      (W25Q64_ERRORPARAMETER ==
        s_mock_driver.pf_w25q64_write_data_noerase(NULL, 0u, data, 4u))    &&
      (W25Q64_ERRORPARAMETER ==
        s_mock_driver.pf_w25q64_write_data_noerase(&s_mock_driver, 0u,
                                                   NULL, 4u))              &&
      (W25Q64_ERRORPARAMETER ==
        s_mock_driver.pf_w25q64_write_data_noerase(&s_mock_driver, 0u,
                                                   data, 0u))              &&
      (W25Q64_ERRORPARAMETER ==
        s_mock_driver.pf_w25q64_write_data_noerase(&s_mock_driver,
                                                   W25Q64_MAX_SIZE,
                                                   data, 1u));
    case_report("CASE 10 write_noerase invalid", ok);
}

static void test_case_write_noerase_persists(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 11: write_noerase persists payload to "
              "fake flash =====");
    state_reset();

    uint8_t pattern[16];
    for (uint32_t i = 0u; i < 16u; i++)
    {
        pattern[i] = (uint8_t)(0x10u + i);
    }

    w25q64_status_t ret = s_mock_driver.pf_w25q64_write_data_noerase(
        &s_mock_driver, W25Q64_MOCK_ADDR_NOERASE, pattern, 16u);

    bool ok = (W25Q64_OK == ret)         &&
              (1u == s_st.we_count)      &&
              (1u == s_st.pp_count)      &&
              (0 == memcmp(pattern,
                           &s_st.fake_flash[W25Q64_MOCK_ADDR_NOERASE], 16u));
    case_report("CASE 11 write_noerase persists", ok);
}

static void test_case_write_erase_overwrites(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 12: write_erase overwrites pre-dirty sector =====");
    state_reset();

    /* Dirty target so a Page Program alone could not flip 0 -> 1 bits. */
    memset(&s_st.fake_flash[W25Q64_MOCK_ADDR_ERASE], 0x00u, 32u);

    uint8_t pattern[16];
    for (uint32_t i = 0u; i < 16u; i++)
    {
        pattern[i] = (uint8_t)(0xC0u | i);
    }

    w25q64_status_t ret = s_mock_driver.pf_w25q64_write_data_erase(
        &s_mock_driver, W25Q64_MOCK_ADDR_ERASE, pattern, 16u);

    bool ok = (W25Q64_OK == ret)              &&
              (s_st.sector_erase_count >= 1u) &&
              (s_st.pp_count >= 1u)           &&
              (0 == memcmp(pattern,
                           &s_st.fake_flash[W25Q64_MOCK_ADDR_ERASE], 16u));
    if (true == ok)
    {
        for (uint32_t i = 16u; i < 32u; i++)
        {
            if (0xFFu != s_st.fake_flash[W25Q64_MOCK_ADDR_ERASE + i])
            {
                ok = false;
                break;
            }
        }
    }
    case_report("CASE 12 write_erase overwrites", ok);
}

static void test_case_write_erase_spans_two_sectors(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 13: write_erase spans two sectors -> 2 erases / "
              "2 PPs =====");
    state_reset();

    uint32_t addr = W25Q64_SECTOR_SIZE - 8u;     /* end of sector 0 */
    uint8_t  pattern[16];
    for (uint32_t i = 0u; i < 16u; i++)
    {
        pattern[i] = (uint8_t)(0x80u + i);
    }

    w25q64_status_t ret = s_mock_driver.pf_w25q64_write_data_erase(
        &s_mock_driver, addr, pattern, 16u);

    bool ok = (W25Q64_OK == ret)            &&
              (2u == s_st.sector_erase_count) &&
              (2u == s_st.pp_count)         &&
              (0 == memcmp(pattern,
                           &s_st.fake_flash[addr], 16u));
    case_report("CASE 13 write_erase spans 2 sectors", ok);
}

static void test_case_erase_chip(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 14: pf_w25q64_erase_chip wipes backing store "
              "to 0xFF =====");
    state_reset();

    s_st.fake_flash[0]                                  = 0x00u;
    s_st.fake_flash[sizeof(s_st.fake_flash) / 2u]       = 0x55u;
    s_st.fake_flash[sizeof(s_st.fake_flash) - 1u]       = 0xAAu;

    w25q64_status_t ret = s_mock_driver.pf_w25q64_erase_chip(&s_mock_driver);

    bool ok = (W25Q64_OK == ret)            &&
              (1u == s_st.chip_erase_count) &&
              (0xFFu == s_st.fake_flash[0]) &&
              (0xFFu == s_st.fake_flash[sizeof(s_st.fake_flash) / 2u]) &&
              (0xFFu == s_st.fake_flash[sizeof(s_st.fake_flash) - 1u]);
    case_report("CASE 14 erase_chip wipes flash", ok);
}

static void test_case_erase_chip_busy_forever(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 15: BUSY-forever -> ERRORTIMEOUT =====");
    state_reset();
    s_st.force_busy_forever = true;

    w25q64_status_t ret = s_mock_driver.pf_w25q64_erase_chip(&s_mock_driver);

    bool ok = (W25Q64_ERRORTIMEOUT == ret);
    case_report("CASE 15 busy-forever timeout", ok);
}

static void test_case_sleep_wakeup(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 16: sleep -> wakeup round-trip + 3ms post-wake "
              "delay =====");
    state_reset();

    bool ok =
        (W25Q64_OK == s_mock_driver.pf_w25q64_sleep(&s_mock_driver)) &&
        (true == s_st.sleeping)                                      &&
        (1u == s_st.sleep_count);

    uint32_t delays_before = s_st.delay_ms_count;
    ok = ok &&
         (W25Q64_OK == s_mock_driver.pf_w25q64_wakeup(&s_mock_driver)) &&
         (false == s_st.sleeping)                                      &&
         (1u == s_st.wakeup_count)                                     &&
         (s_st.delay_ms_count > delays_before)                         &&
         (3u == s_st.last_delay_ms);
    case_report("CASE 16 sleep / wakeup round-trip", ok);
}

static void test_case_sleep_wakeup_null_arg(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 17: sleep(NULL) / wakeup(NULL) / erase_chip(NULL) "
              "-> ERRORPARAMETER =====");
    state_reset();

    bool ok =
        (W25Q64_ERRORPARAMETER == s_mock_driver.pf_w25q64_sleep(NULL))      &&
        (W25Q64_ERRORPARAMETER == s_mock_driver.pf_w25q64_wakeup(NULL))     &&
        (W25Q64_ERRORPARAMETER == s_mock_driver.pf_w25q64_erase_chip(NULL)) &&
        (0u == s_st.transmit_count);
    case_report("CASE 17 NULL arg rejected", ok);
}

static void test_case_deinit(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 18: pf_w25q64_deinit -- spi_deinit + CS high "
              "=====");
    state_reset();

    w25q64_status_t ret = s_mock_driver.pf_w25q64_deinit(&s_mock_driver);

    bool ok = (W25Q64_OK == ret)               &&
              (1u == s_st.spi_deinit_count)    &&
              (1u == s_st.cs_high_count)       &&
              (W25Q64_ERRORPARAMETER ==
                  s_mock_driver.pf_w25q64_deinit(NULL));
    case_report("CASE 18 deinit", ok);
}

static void test_case_end_to_end(void)
{
    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== CASE 19: end-to-end init -> write_erase -> read_data "
              "round-trip =====");
    state_reset();

    bool ok =
        (W25Q64_OK == s_mock_driver.pf_w25q64_init(&s_mock_driver));

    uint8_t tx[64];
    for (uint32_t i = 0u; i < 64u; i++)
    {
        tx[i] = (uint8_t)(i ^ 0x5Au);
    }

    ok = ok &&
        (W25Q64_OK == s_mock_driver.pf_w25q64_write_data_erase(
                          &s_mock_driver, W25Q64_MOCK_ADDR_E2E, tx, 64u));

    uint8_t rx[64] = { 0u };
    ok = ok &&
        (W25Q64_OK == s_mock_driver.pf_w25q64_read_data(
                          &s_mock_driver, W25Q64_MOCK_ADDR_E2E, rx, 64u)) &&
        (0 == memcmp(tx, rx, 64u));
    case_report("CASE 19 end-to-end round-trip", ok);
}
//******************************* Functions *********************************//

//******************************* Functions *********************************//
/**
 * @brief      Entry point for the W25Q64 mock test task.
 *
 * @param[in]  argument : Unused task argument.
 *
 * @return     None.
 * */
void w25q64_mock_test_task(void *argument)
{
    (void)argument;

    /**
     * Stagger against EasyLogger warm-up so our banner lines are not
     * shredded by concurrent boot logs from the rest of User_Init.
     **/
    osal_task_delay(W25Q64_MOCK_BOOT_WAIT_MS);

    DEBUG_OUT(d, W25Q64_MOCK_LOG_TAG, "w25q64_mock_test_task start");

    w25q64_status_t bind_ret = mock_driver_bind();
    if (W25Q64_OK != bind_ret)
    {
        DEBUG_OUT(e, W25Q64_MOCK_ERR_LOG_TAG,
                  "w25q64_driver_inst failed = %d (mock bind)",
                  (int)bind_ret);
        for (;;)
        {
            osal_task_delay(1000u);
        }
    }

    test_case_inst_null_args();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_inst_missing_cb();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    /* Re-bind because CASE 2 invalidated s_mock_driver via partial inst. */
    (void)mock_driver_bind();

    test_case_init_success();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_init_jedec_mismatch();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_init_null_arg();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_read_id_invalid_param();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_read_id_returns_id();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_read_data_invalid_param();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_read_data_returns_flash();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_write_noerase_invalid();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_write_noerase_persists();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_write_erase_overwrites();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_write_erase_spans_two_sectors();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_erase_chip();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_erase_chip_busy_forever();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_sleep_wakeup();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_sleep_wakeup_null_arg();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_deinit();
    osal_task_delay(W25Q64_MOCK_STEP_GAP_MS);

    test_case_end_to_end();

    DEBUG_OUT(i, W25Q64_MOCK_LOG_TAG,
              "===== SUMMARY: %u PASS / %u FAIL =====",
              (unsigned)s_pass_count, (unsigned)s_fail_count);
    if (0u == s_fail_count)
    {
        DEBUG_OUT(d, W25Q64_MOCK_LOG_TAG, "w25q64_mock_test_task ALL PASS");
    }
    else
    {
        DEBUG_OUT(e, W25Q64_MOCK_ERR_LOG_TAG,
                  "w25q64_mock_test_task %u FAIL -- review log above",
                  (unsigned)s_fail_count);
    }

    for (;;)
    {
        osal_task_delay(5000u);
    }
}
//******************************* Functions *********************************//
