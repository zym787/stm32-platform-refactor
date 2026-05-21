/******************************************************************************
 * @file em7028_mock_test_task.c
 *
 * @par dependencies
 * - user_task_reso_config.h
 * - bsp_em7028_driver.h
 * - bsp_em7028_reg.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief EM7028 driver logic validation without a real PPG sensor.
 *
 * Processing flow:
 * Mount fake I2C / timebase / DWT-delay / OS-delay vtables that capture
 * every mem read / write / delay call, then exercise the EM7028 driver
 * entry points and self-assert against the expected register byte for
 * every step (HRS_CFG / LED_CRT / HRS2_GAIN_CTRL / HRS2_CTRL / HRS1_CTRL
 * default-cfg encoding plus pf_start / pf_stop / pf_read_frame / raw
 * register access / pf_deinit). Output is routed to RTT Terminal 8
 * (DEBUG_RTT_CH_PPG) so the case banners line up next to the real PPG
 * traffic.
 *
 * @version V1.0 2026-04-29
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
#include "bsp_em7028_driver.h"
#include "bsp_em7028_reg.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define EM7028_MOCK_SENTINEL_PTR            ((void *)0xE7028E70u)

#define EM7028_MOCK_BOOT_WAIT_MS                2000u
#define EM7028_MOCK_STEP_GAP_MS                  200u

#define EM7028_MOCK_I2C_WRITE_ADDR            (uint16_t)((EM7028_ADDR << 1) | 0)
#define EM7028_MOCK_I2C_READ_ADDR             (uint16_t)((EM7028_ADDR << 1) | 1)

/* Expected stats from auto-init inside bsp_em7028_driver_inst.
 * Sequence: iic_init -> yield(boot 5ms) -> write SOFT_RESET -> yield(7ms)
 *           -> read ID_REG -> 5x apply_config_inner writes.            */
#define EM7028_MOCK_EXPECTED_BOOT_DELAY_MS         5u
#define EM7028_MOCK_EXPECTED_RESET_DELAY_MS        7u
#define EM7028_MOCK_EXPECTED_INIT_WRITES           6u   /* SOFT_RESET + 5 */
#define EM7028_MOCK_EXPECTED_INIT_READS            1u   /* ID_REG         */
#define EM7028_MOCK_EXPECTED_INIT_YIELDS           2u   /* boot + reset   */

/* Expected register byte values produced by the default cached_cfg in
 * bsp_em7028_driver_inst (40 Hz HRS1 pulse, 14-bit ADC, gain x5, range
 * x8, HRS2 disabled, LED_CRT mid).                                     */
#define EM7028_MOCK_DEFAULT_HRS_CFG_OFF        0x00u
#define EM7028_MOCK_DEFAULT_LED_CRT            0x80u
/* HRS2_GAIN_CTRL: gain_high=false (bit7=0) | pos_mask=0x0F             */
#define EM7028_MOCK_DEFAULT_HRS2_GAIN_CTRL     0x0Fu
/* HRS2_CTRL: mode=PULSE bit7=1 | wait=0x05 << 4 = 0x50 |
 *            LED_WIDTH default (1<<2) | LED_CNT default (1<<0) = 0xD5  */
#define EM7028_MOCK_DEFAULT_HRS2_CTRL          0xD5u
/* HRS1_CTRL: gain=1 (0x80) | range=1 (0x40) | freq=0x04 << 3 (0x20) |
 *            res=0x02 << 1 (0x04) | ir_mode=1 (0x01) = 0xE5            */
#define EM7028_MOCK_DEFAULT_HRS1_CTRL          0xE5u

/* Expected HRS_CFG byte after pf_start with default cached_cfg --
 * enable_hrs1=true, enable_hrs2=false  ->  bit3 set only.              */
#define EM7028_MOCK_HRS_CFG_HRS1_EN_ONLY       (1u << 3)

/* Test pattern for HRS data registers (LSB first per byte order).      */
#define EM7028_MOCK_FRAME_PIXEL0               0x1234u
#define EM7028_MOCK_FRAME_PIXEL1               0x5678u
#define EM7028_MOCK_FRAME_PIXEL2               0xABCDu
#define EM7028_MOCK_FRAME_PIXEL3               0xEF01u
#define EM7028_MOCK_FRAME_SUM                                                 \
    ((uint32_t)EM7028_MOCK_FRAME_PIXEL0 + (uint32_t)EM7028_MOCK_FRAME_PIXEL1 +\
     (uint32_t)EM7028_MOCK_FRAME_PIXEL2 + (uint32_t)EM7028_MOCK_FRAME_PIXEL3)
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static bsp_em7028_driver_t              s_mock_driver;
static em7028_iic_driver_interface_t    s_mock_iic;
static em7028_timebase_interface_t             s_mock_timebase;
static em7028_delay_interface_t         s_mock_delay;
static em7028_os_delay_interface_t      s_mock_os_delay;

/**
 * Per-case capture: every I2C op lands here so the case body can assert
 * on register address, payload, length and call count. The "next_read"
 * buffer lets a test seed a synthetic chip response before calling the
 * driver. Up to 16 bytes -- enough for one HRS data block (8 bytes) or
 * a couple of single-byte pre-arm sequences.
 */
typedef struct
{
    uint32_t iic_init_count;
    uint32_t iic_deinit_count;
    uint32_t write_count;
    uint32_t read_count;
    uint32_t delay_ms_count;
    uint32_t yield_ms_count;
    uint32_t last_delay_ms;
    uint32_t last_yield_ms;
    uint16_t last_write_reg;
    uint16_t last_read_reg;
    uint16_t last_write_len;
    uint16_t last_read_len;
    uint8_t  last_write_byte;
    void    *last_i2c_handle;
    uint16_t last_dev_addr;
    /* Capture every register byte the driver pushes during an init or
     * apply_config sequence, so the test can verify the full series.   */
    uint8_t  write_log_reg[16];
    uint8_t  write_log_byte[16];
    uint8_t  write_log_len;
} mock_stats_t;

static mock_stats_t s_stats;
static uint8_t      s_next_read[16];
static uint16_t     s_next_read_len;
static uint32_t     s_fake_tick_ms;

static uint32_t     s_pass_count;
static uint32_t     s_fail_count;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief      Reset all per-case capture state to zero.
 *
 * @return     None.
 * */
static void stats_reset(void)
{
    /**
     * Clear capture struct in full so leaks between cases are impossible.
     * s_next_read is left alone -- callers re-arm it explicitly per case.
     **/
    memset(&s_stats, 0, sizeof(s_stats));
}

/**
 * @brief      Seed the synthetic response served on the next mem-read call.
 *
 * @param[in]  p_data : Bytes to return when the driver reads.
 *
 * @param[in]  len    : Number of bytes to arm (clamped to buffer size).
 *
 * @return     None.
 * */
static void mock_arm_read(uint8_t const *p_data, uint16_t len)
{
    if ((p_data == NULL) || (len == 0U))
    {
        s_next_read_len = 0U;
        return;
    }
    if (len > sizeof(s_next_read))
    {
        len = (uint16_t)sizeof(s_next_read);
    }
    memcpy(s_next_read, p_data, len);
    s_next_read_len = len;
}

/* ---- I2C mock ------------------------------------------------------------ */
/**
 * @brief      Capture iic init invocation count.
 *
 * @param[in]  hi2c : Mock sentinel pointer (validated by the assertion).
 *
 * @return     EM7028_OK always (mock).
 * */
static em7028_status_t mock_iic_init(void *hi2c)
{
    (void)hi2c;
    s_stats.iic_init_count++;
    DEBUG_OUT(i, EM7028_LOG_TAG, "mock iic_init");
    return EM7028_OK;
}

/**
 * @brief      Capture iic deinit invocation count.
 *
 * @param[in]  hi2c : Mock sentinel pointer.
 *
 * @return     EM7028_OK always.
 * */
static em7028_status_t mock_iic_deinit(void *hi2c)
{
    (void)hi2c;
    s_stats.iic_deinit_count++;
    DEBUG_OUT(i, EM7028_LOG_TAG, "mock iic_deinit");
    return EM7028_OK;
}

/**
 * @brief      Capture a mem-write transaction and append it to the log.
 *
 * @param[in]  i2c       : Mock sentinel pointer.
 *
 * @param[in]  des_addr  : Device address (8-bit, with R/W bit cleared).
 *
 * @param[in]  mem_addr  : Target register address.
 *
 * @param[in]  mem_size  : Memory address byte size (always 1 for EM7028).
 *
 * @param[in]  p_data    : Payload buffer.
 *
 * @param[in]  size      : Payload length.
 *
 * @param[in]  timeout   : Driver-supplied timeout (ignored by mock).
 *
 * @return     EM7028_OK always.
 * */
static em7028_status_t mock_iic_mem_write(void    *i2c,
                                          uint16_t des_addr,
                                          uint16_t mem_addr,
                                          uint16_t mem_size,
                                          uint8_t *p_data,
                                          uint16_t size,
                                          uint32_t timeout)
{
    (void)mem_size;
    (void)timeout;
    s_stats.write_count++;
    s_stats.last_i2c_handle = i2c;
    s_stats.last_dev_addr   = des_addr;
    s_stats.last_write_reg  = mem_addr;
    s_stats.last_write_len  = size;
    s_stats.last_write_byte = (size > 0U) ? p_data[0] : 0U;

    /* Append to write log so init/apply_config sequences can be replayed
     * by the case bodies for full-sequence assertion.                  */
    if (s_stats.write_log_len < (uint8_t)(sizeof(s_stats.write_log_reg)))
    {
        s_stats.write_log_reg [s_stats.write_log_len] = (uint8_t)mem_addr;
        s_stats.write_log_byte[s_stats.write_log_len] =
                                                (size > 0U) ? p_data[0] : 0U;
        s_stats.write_log_len++;
    }

    DEBUG_OUT(i, EM7028_LOG_TAG,
              "mock mem_write addr=0x%02X reg=0x%02X len=%u data0=0x%02X",
              (unsigned)des_addr, (unsigned)mem_addr, (unsigned)size,
              (unsigned)s_stats.last_write_byte);
    return EM7028_OK;
}

/**
 * @brief      Capture a mem-read transaction and serve the armed bytes.
 *
 * @param[in]  i2c      : Mock sentinel pointer.
 *
 * @param[in]  des_addr : Device address (8-bit, with R/W bit set).
 *
 * @param[in]  mem_addr : Starting register address.
 *
 * @param[in]  mem_size : Memory address byte size (always 1 for EM7028).
 *
 * @param[out] p_data   : Output buffer.
 *
 * @param[in]  size     : Number of bytes requested.
 *
 * @param[in]  timeout  : Driver-supplied timeout (ignored by mock).
 *
 * @return     EM7028_OK always.
 * */
static em7028_status_t mock_iic_mem_read(void    *i2c,
                                         uint16_t des_addr,
                                         uint16_t mem_addr,
                                         uint16_t mem_size,
                                         uint8_t *p_data,
                                         uint16_t size,
                                         uint32_t timeout)
{
    (void)mem_size;
    (void)timeout;
    s_stats.read_count++;
    s_stats.last_i2c_handle = i2c;
    s_stats.last_dev_addr   = des_addr;
    s_stats.last_read_reg   = mem_addr;
    s_stats.last_read_len   = size;

    /**
     * Serve the synthetic response armed by the test; zero-fill any bytes
     * the test forgot to provide so old values cannot leak between cases.
     **/
    uint16_t copy = (size < s_next_read_len) ? size : s_next_read_len;
    if ((p_data != NULL) && (copy > 0U))
    {
        memcpy(p_data, s_next_read, copy);
    }
    if ((p_data != NULL) && (size > copy))
    {
        memset(&p_data[copy], 0, (size_t)(size - copy));
    }
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "mock mem_read addr=0x%02X reg=0x%02X len=%u served=%u",
              (unsigned)des_addr, (unsigned)mem_addr, (unsigned)size,
              (unsigned)copy);
    return EM7028_OK;
}

/* ---- Timebase / delay / OS mocks ---------------------------------------- */
/**
 * @brief      Synthetic monotonic ms tick used for frame timestamps and
 *             advanced by the delay/yield mocks.
 *
 * @return     Current synthetic tick in milliseconds.
 * */
static uint32_t mock_tb_get_tick_count(void)
{
    return s_fake_tick_ms;
}

/**
 * @brief      No-op DWT delay init (captured purely for log readability).
 *
 * @return     None.
 * */
static void mock_delay_init(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG, "mock delay_init");
}

/**
 * @brief      Capture a busy-wait ms delay and advance the synthetic tick.
 *
 * @param[in]  ms : Wait duration in milliseconds.
 *
 * @return     None.
 * */
static void mock_delay_ms(uint32_t const ms)
{
    s_stats.delay_ms_count++;
    s_stats.last_delay_ms = ms;
    s_fake_tick_ms       += ms;
    DEBUG_OUT(i, EM7028_LOG_TAG, "mock delay_ms %u", (unsigned)ms);
}

/**
 * @brief      Stub for the us-precision delay (driver does not use it
 *             in any current code path; logged for safety).
 *
 * @param[in]  us : Wait duration in microseconds.
 *
 * @return     None.
 * */
static void mock_delay_us(uint32_t const us)
{
    (void)us;
}

/**
 * @brief      Capture an OS-yield ms delay and advance the synthetic tick.
 *
 * @param[in]  ms : Wait duration in milliseconds.
 *
 * @return     None.
 * */
static void mock_os_delay(uint32_t const ms)
{
    s_stats.yield_ms_count++;
    s_stats.last_yield_ms = ms;
    s_fake_tick_ms       += ms;
    DEBUG_OUT(i, EM7028_LOG_TAG, "mock os_delay %u", (unsigned)ms);
}

/* ---- Wire-up ------------------------------------------------------------- */
/**
 * @brief      Bind mock vtables and run bsp_em7028_driver_inst, which
 *             auto-invokes pf_init via the freshly-mounted vtable.
 *             The chip-id response is pre-armed so init can probe it.
 *
 * @return     em7028_status_t EM7028_OK on success, error code otherwise.
 * */
static em7028_status_t mock_driver_bind(void)
{
    s_mock_iic.hi2c              = EM7028_MOCK_SENTINEL_PTR;
    s_mock_iic.pf_iic_init       = mock_iic_init;
    s_mock_iic.pf_iic_deinit     = mock_iic_deinit;
    s_mock_iic.pf_iic_mem_write  = mock_iic_mem_write;
    s_mock_iic.pf_iic_mem_read   = mock_iic_mem_read;

    s_mock_timebase.pf_get_tick_count = mock_tb_get_tick_count;

    s_mock_delay.pf_delay_init   = mock_delay_init;
    s_mock_delay.pf_delay_ms     = mock_delay_ms;
    s_mock_delay.pf_delay_us     = mock_delay_us;

    s_mock_os_delay.pf_rtos_delay = mock_os_delay;

    /**
     * Arm the ID_REG response BEFORE inst, because bsp_em7028_driver_inst
     * auto-invokes pf_init which reads ID_REG synchronously.
     **/
    uint8_t chip_id = EM7028_ID;
    mock_arm_read(&chip_id, 1U);

    return bsp_em7028_driver_inst(&s_mock_driver,
                                  &s_mock_iic,
                                  &s_mock_timebase,
                                  &s_mock_delay,
                                  &s_mock_os_delay);
}

/* ---- Pass/fail helpers --------------------------------------------------- */
/**
 * @brief      Record case outcome and emit a one-line PASS/FAIL banner.
 *
 * @param[in]  name : Human-readable case label.
 *
 * @param[in]  ok   : true if the case passed.
 *
 * @return     None.
 * */
static void case_report(const char *name, bool ok)
{
    if (ok)
    {
        s_pass_count++;
        DEBUG_OUT(d, EM7028_LOG_TAG, "[PASS] %s", name);
    }
    else
    {
        s_fail_count++;
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "[FAIL] %s  stats w=%u r=%u reg_w=0x%02X reg_r=0x%02X"
                  " byte=0x%02X yield=%u",
                  name,
                  (unsigned)s_stats.write_count,
                  (unsigned)s_stats.read_count,
                  (unsigned)s_stats.last_write_reg,
                  (unsigned)s_stats.last_read_reg,
                  (unsigned)s_stats.last_write_byte,
                  (unsigned)s_stats.yield_ms_count);
    }
}

/**
 * @brief      Locate the most recent write to `reg` in the captured log
 *             and return its payload byte.
 *
 * @param[in]  reg     : Register address to look up.
 *
 * @param[out] p_byte  : Output byte (only set when found).
 *
 * @return     true if `reg` was written at least once during this case.
 * */
static bool find_logged_write(uint8_t reg, uint8_t *p_byte)
{
    bool found = false;
    for (uint8_t i = 0U; i < s_stats.write_log_len; i++)
    {
        if (s_stats.write_log_reg[i] == reg)
        {
            *p_byte = s_stats.write_log_byte[i];
            found   = true;
        }
    }
    return found;
}

/* ---- Cases --------------------------------------------------------------- */
/**
 * @brief      CASE 1 -- inst auto-init writes the full default config.
 *             Verifies iic_init=1, two yields (boot + reset), one
 *             ID_REG read, six writes (SOFT_RESET + 5 cfg registers),
 *             and that each cfg register received the byte computed from
 *             the default cached_cfg (40 Hz HRS1 pulse, 14-bit ADC).
 *
 * @return     None (uses s_pass_count / s_fail_count).
 * */
static void test_case_inst_auto_init(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 1: inst auto-init -- boot delay + soft reset + "
              "id probe + 5x apply_config writes =====");

    /**
     * mock_driver_bind has already been called by the task entry; the
     * captured stats are still fresh because no case has run yet.
     **/
    bool seq_ok = true;
    uint8_t got = 0U;

    if (!find_logged_write(SOFT_RESET, &got) || (got != 0x4Du))
    {
        seq_ok = false;
    }
    if (!find_logged_write(HRS_CFG, &got) ||
        (got != EM7028_MOCK_DEFAULT_HRS_CFG_OFF))
    {
        seq_ok = false;
    }
    if (!find_logged_write(LED_CRT, &got) ||
        (got != EM7028_MOCK_DEFAULT_LED_CRT))
    {
        seq_ok = false;
    }
    if (!find_logged_write(HRS2_GAIN_CTRL, &got) ||
        (got != EM7028_MOCK_DEFAULT_HRS2_GAIN_CTRL))
    {
        seq_ok = false;
    }
    if (!find_logged_write(HRS2_CTRL, &got) ||
        (got != EM7028_MOCK_DEFAULT_HRS2_CTRL))
    {
        seq_ok = false;
    }
    if (!find_logged_write(HRS1_CTRL, &got) ||
        (got != EM7028_MOCK_DEFAULT_HRS1_CTRL))
    {
        seq_ok = false;
    }

    const bool ok =
        (1U == s_stats.iic_init_count)                                     &&
        (EM7028_MOCK_EXPECTED_INIT_YIELDS  == s_stats.yield_ms_count)      &&
        (EM7028_MOCK_EXPECTED_INIT_READS   == s_stats.read_count)          &&
        (EM7028_MOCK_EXPECTED_INIT_WRITES  == s_stats.write_count)         &&
        (ID_REG                            == s_stats.last_read_reg)       &&
        (HRS1_CTRL                         == s_stats.last_write_reg)      &&
        (EM7028_MOCK_DEFAULT_HRS1_CTRL     == s_stats.last_write_byte)     &&
        (EM7028_MOCK_I2C_WRITE_ADDR        == s_stats.last_dev_addr ||
         EM7028_MOCK_I2C_READ_ADDR         == s_stats.last_dev_addr)       &&
        (EM7028_MOCK_SENTINEL_PTR          == s_stats.last_i2c_handle)     &&
        seq_ok;
    case_report("CASE 1 inst auto-init", ok);
}

/**
 * @brief      CASE 2 -- pf_read_id reads ID_REG and returns the armed
 *             chip-id byte.
 *
 * @return     None.
 * */
static void test_case_read_id(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 2: pf_read_id -- read ID_REG (0x00) =====");
    stats_reset();
    uint8_t armed = EM7028_ID;
    mock_arm_read(&armed, 1U);

    uint8_t id = 0U;
    em7028_status_t ret = s_mock_driver.pf_read_id(&s_mock_driver, &id);

    const bool ok = (EM7028_OK == ret)                  &&
                    (1U        == s_stats.read_count)   &&
                    (0U        == s_stats.write_count)  &&
                    (ID_REG    == s_stats.last_read_reg)&&
                    (1U        == s_stats.last_read_len)&&
                    (EM7028_ID == id);
    case_report("CASE 2 read_id", ok);
}

/**
 * @brief      CASE 3 -- pf_soft_reset writes SOFT_RESET (0x0F) and
 *             yields for ~7 ms (single combined wait, not two).
 *
 * @return     None.
 * */
static void test_case_soft_reset(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 3: pf_soft_reset -- write SOFT_RESET, yield 7ms "
              "=====");
    stats_reset();

    em7028_status_t ret = s_mock_driver.pf_soft_reset(&s_mock_driver);

    const bool ok = (EM7028_OK    == ret)                                  &&
                    (1U           == s_stats.write_count)                  &&
                    (0U           == s_stats.read_count)                   &&
                    (SOFT_RESET   == s_stats.last_write_reg)               &&
                    (0x4Du        == s_stats.last_write_byte)              &&
                    (1U           == s_stats.yield_ms_count)               &&
                    (EM7028_MOCK_EXPECTED_RESET_DELAY_MS ==
                                                       s_stats.last_yield_ms);
    case_report("CASE 3 soft_reset", ok);
}

/**
 * @brief      CASE 4 -- pf_apply_config with a non-default cfg writes
 *             the 5-register sequence with bytes derived from cfg.
 *
 * @return     None.
 * */
static void test_case_apply_config(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 4: pf_apply_config -- custom cfg, verify 5 "
              "register writes =====");
    stats_reset();

    /**
     * Custom cfg: HRS1 gain x1 / range x1 / 80 Hz / 12-bit, HRS2
     * continuous mode / 80 Hz wait / gain x10 / pixels {0,2}, LED_CRT 0x40.
     *   HRS_CFG step1 = 0x00
     *   LED_CRT       = 0x40
     *   HRS2_GAIN     = 0x80 (gain) | 0x05 (pos) = 0x85
     *   HRS2_CTRL     = 0x00 (cont) | 0x40 (wait<<4) | 0x04 | 0x01 = 0x45
     *   HRS1_CTRL     = 0x00 (gain) | 0x00 (range) | 0x18 (freq<<3) |
     *                   0x02 (res<<1) | 0x01 (ir_mode) = 0x1B
     **/
    em7028_config_t cfg = {
        .enable_hrs1     = true,
        .enable_hrs2     = true,
        .hrs1_gain_high  = false,
        .hrs1_range_high = false,
        .hrs1_freq       = EM7028_HRS1_FREQ_327K_12_5MS,
        .hrs1_res        = EM7028_HRS_RES_12BIT,
        .hrs2_mode       = EM7028_HRS2_MODE_CONTINUOUS,
        .hrs2_wait       = EM7028_HRS2_WAIT_12_5MS,
        .hrs2_gain_high  = true,
        .hrs2_pos_mask   = 0x05u,
        .led_current     = 0x40u,
    };

    em7028_status_t ret =
        s_mock_driver.pf_apply_config(&s_mock_driver, &cfg);

    uint8_t got = 0U;
    bool seq_ok = true;
    if (!find_logged_write(HRS_CFG, &got)        || (got != 0x00u))
    {
        seq_ok = false;
    }
    if (!find_logged_write(LED_CRT, &got)        || (got != 0x40u))
    {
        seq_ok = false;
    }
    if (!find_logged_write(HRS2_GAIN_CTRL, &got) || (got != 0x85u))
    {
        seq_ok = false;
    }
    if (!find_logged_write(HRS2_CTRL, &got)      || (got != 0x45u))
    {
        seq_ok = false;
    }
    if (!find_logged_write(HRS1_CTRL, &got)      || (got != 0x1Bu))
    {
        seq_ok = false;
    }

    const bool ok = (EM7028_OK == ret)              &&
                    (5U == s_stats.write_count)     &&
                    (0U == s_stats.read_count)      &&
                    (HRS1_CTRL == s_stats.last_write_reg) &&
                    (0x1Bu == s_stats.last_write_byte) &&
                    seq_ok;
    case_report("CASE 4 apply_config", ok);
}

/**
 * @brief      CASE 5 -- pf_start reads HRS_CFG, ORs in HRS1_EN bit
 *             (cached_cfg has enable_hrs1=true / enable_hrs2=true after
 *             CASE 4), and writes the merged value back.
 *
 * @return     None.
 * */
static void test_case_start(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 5: pf_start -- read HRS_CFG, set enable bits, "
              "write back =====");
    stats_reset();
    /* Arm HRS_CFG read response = 0 (post-apply state)                */
    uint8_t armed_cfg = 0x00u;
    mock_arm_read(&armed_cfg, 1U);

    em7028_status_t ret = s_mock_driver.pf_start(&s_mock_driver);

    /* CASE 4 cfg enabled both channels: bit7 (HRS2_EN) | bit3 (HRS1_EN)
     * = 0x88                                                          */
    const uint8_t expected = (uint8_t)((1u << 7) | (1u << 3));
    const bool ok = (EM7028_OK == ret)                  &&
                    (1U == s_stats.read_count)          &&
                    (1U == s_stats.write_count)         &&
                    (HRS_CFG == s_stats.last_read_reg)  &&
                    (HRS_CFG == s_stats.last_write_reg) &&
                    (expected == s_stats.last_write_byte);
    case_report("CASE 5 start", ok);
}

/**
 * @brief      CASE 6 -- pf_stop reads HRS_CFG, clears HRS1_EN/HRS2_EN
 *             bits, writes back. Cached cfg is preserved.
 *
 * @return     None.
 * */
static void test_case_stop(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 6: pf_stop -- read HRS_CFG, clear enable bits, "
              "write back =====");
    stats_reset();
    /* Arm HRS_CFG read response = both enable bits set                */
    uint8_t armed_cfg = (uint8_t)((1u << 7) | (1u << 3));
    mock_arm_read(&armed_cfg, 1U);

    em7028_status_t ret = s_mock_driver.pf_stop(&s_mock_driver);

    const bool ok = (EM7028_OK == ret)                  &&
                    (1U == s_stats.read_count)          &&
                    (1U == s_stats.write_count)         &&
                    (HRS_CFG == s_stats.last_read_reg)  &&
                    (HRS_CFG == s_stats.last_write_reg) &&
                    (0x00u   == s_stats.last_write_byte);
    case_report("CASE 6 stop", ok);
}

/**
 * @brief      CASE 7 -- pf_read_frame with default cfg (HRS1 only).
 *             First we re-apply the default to flip enable_hrs2 back
 *             to false, then we burst-read the HRS1 block and check
 *             that HRS1 pixels decode correctly while HRS2 fields are
 *             zeroed (no read issued for the disabled channel).
 *
 * @return     None.
 * */
static void test_case_read_frame_hrs1_only(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 7: pf_read_frame -- HRS1-only burst, decode 4 "
              "pixels + sum + timestamp =====");

    /* First: re-apply default cfg (HRS1 only) -- pf_apply_config does
     * 5 register writes; not asserting them here, just resetting the
     * cached cfg so read_frame skips the HRS2 read.                   */
    em7028_config_t cfg = {
        .enable_hrs1     = true,
        .enable_hrs2     = false,
        .hrs1_gain_high  = true,
        .hrs1_range_high = true,
        .hrs1_freq       = EM7028_HRS1_FREQ_163K_25MS,
        .hrs1_res        = EM7028_HRS_RES_14BIT,
        .hrs2_mode       = EM7028_HRS2_MODE_PULSE,
        .hrs2_wait       = EM7028_HRS2_WAIT_25MS,
        .hrs2_gain_high  = false,
        .hrs2_pos_mask   = 0x0Fu,
        .led_current     = 0x80u,
    };
    (void)s_mock_driver.pf_apply_config(&s_mock_driver, &cfg);

    /* Now arm 8 bytes that decode (LSB-first) to the four test pixels */
    static const uint8_t ppg_bytes[8] = {
        0x34u, 0x12u, /* pixel 0 = 0x1234 */
        0x78u, 0x56u, /* pixel 1 = 0x5678 */
        0xCDu, 0xABu, /* pixel 2 = 0xABCD */
        0x01u, 0xEFu, /* pixel 3 = 0xEF01 */
    };
    stats_reset();
    mock_arm_read(ppg_bytes, sizeof(ppg_bytes));

    em7028_ppg_frame_t frame = {0};
    em7028_status_t ret =
        s_mock_driver.pf_read_frame(&s_mock_driver, &frame);

    const bool ok =
        (EM7028_OK == ret)                                                  &&
        (1U                          == s_stats.read_count)                 &&
        (0U                          == s_stats.write_count)                &&
        (HRS1_DATA0_L                == s_stats.last_read_reg)              &&
        (8U                          == s_stats.last_read_len)              &&
        (EM7028_MOCK_FRAME_PIXEL0    == frame.hrs1_pixel[0])                &&
        (EM7028_MOCK_FRAME_PIXEL1    == frame.hrs1_pixel[1])                &&
        (EM7028_MOCK_FRAME_PIXEL2    == frame.hrs1_pixel[2])                &&
        (EM7028_MOCK_FRAME_PIXEL3    == frame.hrs1_pixel[3])                &&
        (EM7028_MOCK_FRAME_SUM       == frame.hrs1_sum)                     &&
        (0U                          == frame.hrs2_pixel[0])                &&
        (0U                          == frame.hrs2_sum)                     &&
        (frame.timestamp_ms          == s_fake_tick_ms);
    case_report("CASE 7 read_frame HRS1 only", ok);
}

/**
 * @brief      CASE 8 -- pf_read_frame with both HRS1 and HRS2 enabled.
 *             Two burst reads must fire (HRS2 first, then HRS1) and both
 *             sums populated. Same armed buffer serves both reads, so
 *             the per-channel sums are equal -- we assert structure.
 *
 * @return     None.
 * */
static void test_case_read_frame_both(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 8: pf_read_frame -- both channels, expect 2 "
              "burst reads at 0x20 then 0x28 =====");

    em7028_config_t cfg = {
        .enable_hrs1     = true,
        .enable_hrs2     = true,
        .hrs1_gain_high  = true,
        .hrs1_range_high = true,
        .hrs1_freq       = EM7028_HRS1_FREQ_163K_25MS,
        .hrs1_res        = EM7028_HRS_RES_14BIT,
        .hrs2_mode       = EM7028_HRS2_MODE_PULSE,
        .hrs2_wait       = EM7028_HRS2_WAIT_25MS,
        .hrs2_gain_high  = false,
        .hrs2_pos_mask   = 0x0Fu,
        .led_current     = 0x80u,
    };
    (void)s_mock_driver.pf_apply_config(&s_mock_driver, &cfg);

    static const uint8_t ppg_bytes[8] = {
        0x34u, 0x12u, 0x78u, 0x56u, 0xCDu, 0xABu, 0x01u, 0xEFu,
    };
    stats_reset();
    mock_arm_read(ppg_bytes, sizeof(ppg_bytes));

    em7028_ppg_frame_t frame = {0};
    em7028_status_t ret =
        s_mock_driver.pf_read_frame(&s_mock_driver, &frame);

    const bool ok =
        (EM7028_OK == ret)                                                  &&
        (2U                          == s_stats.read_count)                 &&
        (HRS1_DATA0_L                == s_stats.last_read_reg)              &&
        (EM7028_MOCK_FRAME_SUM       == frame.hrs1_sum)                     &&
        (EM7028_MOCK_FRAME_SUM       == frame.hrs2_sum)                     &&
        (frame.timestamp_ms          == s_fake_tick_ms);
    case_report("CASE 8 read_frame HRS1+HRS2", ok);
}

/**
 * @brief      CASE 9 -- pf_read_reg returns the armed byte from any
 *             address.
 *
 * @return     None.
 * */
static void test_case_read_reg_raw(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 9: pf_read_reg -- raw access reads INT_CTRL "
              "(0x0E) =====");
    stats_reset();
    uint8_t armed = 0xA5u;
    mock_arm_read(&armed, 1U);

    uint8_t val = 0U;
    em7028_status_t ret =
        s_mock_driver.pf_read_reg(&s_mock_driver, INT_CTRL, &val);

    const bool ok = (EM7028_OK == ret)                  &&
                    (1U == s_stats.read_count)          &&
                    (0U == s_stats.write_count)         &&
                    (INT_CTRL == s_stats.last_read_reg) &&
                    (1U       == s_stats.last_read_len) &&
                    (0xA5u    == val);
    case_report("CASE 9 read_reg raw", ok);
}

/**
 * @brief      CASE 10 -- pf_write_reg pushes a single byte at any
 *             address.
 *
 * @return     None.
 * */
static void test_case_write_reg_raw(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 10: pf_write_reg -- raw write to HRS_LT_L "
              "(0x03) =====");
    stats_reset();

    em7028_status_t ret =
        s_mock_driver.pf_write_reg(&s_mock_driver, HRS_LT_L, 0x12u);

    const bool ok = (EM7028_OK == ret)                  &&
                    (1U == s_stats.write_count)         &&
                    (0U == s_stats.read_count)          &&
                    (HRS_LT_L == s_stats.last_write_reg)&&
                    (1U       == s_stats.last_write_len)&&
                    (0x12u    == s_stats.last_write_byte);
    case_report("CASE 10 write_reg raw", ok);
}

/**
 * @brief      CASE 11 -- NULL output pointer must be rejected without
 *             any I2C traffic.
 *
 * @return     None.
 * */
static void test_case_null_param_rejected(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 11: NULL output pointer must be rejected =====");
    stats_reset();

    em7028_status_t ret =
        s_mock_driver.pf_read_id(&s_mock_driver, NULL);

    const bool ok = (EM7028_ERRORPARAMETER == ret) &&
                    (0U == s_stats.read_count)    &&
                    (0U == s_stats.write_count);
    case_report("CASE 11 null-arg reject", ok);
}

/**
 * @brief      CASE 12 -- pf_deinit clears HRS_CFG (best effort), calls
 *             iic_deinit, drops is_init. A subsequent pf_read_id must
 *             then return EM7028_ERRORPARAMETER because is_init is gone.
 *
 * @return     None.
 * */
static void test_case_deinit_clears_is_init(void)
{
    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== CASE 12: pf_deinit -- clears HRS_CFG, runs iic_deinit, "
              "subsequent API calls rejected =====");
    stats_reset();

    em7028_status_t ret = s_mock_driver.pf_deinit(&s_mock_driver);

    /* deinit writes HRS_CFG=0 best-effort + calls iic_deinit            */
    const bool deinit_ok = (EM7028_OK == ret)                  &&
                           (1U == s_stats.write_count)         &&
                           (HRS_CFG == s_stats.last_write_reg) &&
                           (0x00u == s_stats.last_write_byte)  &&
                           (1U == s_stats.iic_deinit_count);

    /* Confirm is_init was cleared: subsequent typed call should bounce */
    uint8_t id = 0U;
    em7028_status_t after = s_mock_driver.pf_read_id(&s_mock_driver, &id);
    const bool guard_ok = (EM7028_ERRORPARAMETER == after);

    case_report("CASE 12 deinit + is_init cleared",
                deinit_ok && guard_ok);
}
//******************************* Functions *********************************//

//******************************* Functions *********************************//
/**
 * @brief      Entry point for the EM7028 mock test task.
 *
 * @param[in]  argument : Unused task argument.
 *
 * @return     None.
 * */
void em7028_mock_test_task(void *argument)
{
    (void)argument;

    /**
     * Stagger against EasyLogger warm-up so our banner lines are not
     * shredded by concurrent boot logs from the rest of User_Init.
     **/
    osal_task_delay(EM7028_MOCK_BOOT_WAIT_MS);

    DEBUG_OUT(d, EM7028_LOG_TAG, "em7028_mock_test_task start");

    em7028_status_t bind_ret = mock_driver_bind();
    if (EM7028_OK != bind_ret)
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "bsp_em7028_driver_inst failed = %d (mock bind)",
                  (int)bind_ret);
        for (;;)
        {
            osal_task_delay(1000U);
        }
    }

    /* CASE 1 must run before any stats_reset since it inspects the
     * captured init sequence produced by the inst auto-init above. */
    test_case_inst_auto_init();
    osal_task_delay(EM7028_MOCK_STEP_GAP_MS);

    test_case_read_id();
    osal_task_delay(EM7028_MOCK_STEP_GAP_MS);

    test_case_soft_reset();
    osal_task_delay(EM7028_MOCK_STEP_GAP_MS);

    test_case_apply_config();
    osal_task_delay(EM7028_MOCK_STEP_GAP_MS);

    test_case_start();
    osal_task_delay(EM7028_MOCK_STEP_GAP_MS);

    test_case_stop();
    osal_task_delay(EM7028_MOCK_STEP_GAP_MS);

    test_case_read_frame_hrs1_only();
    osal_task_delay(EM7028_MOCK_STEP_GAP_MS);

    test_case_read_frame_both();
    osal_task_delay(EM7028_MOCK_STEP_GAP_MS);

    test_case_read_reg_raw();
    osal_task_delay(EM7028_MOCK_STEP_GAP_MS);

    test_case_write_reg_raw();
    osal_task_delay(EM7028_MOCK_STEP_GAP_MS);

    test_case_null_param_rejected();
    osal_task_delay(EM7028_MOCK_STEP_GAP_MS);

    test_case_deinit_clears_is_init();

    DEBUG_OUT(i, EM7028_LOG_TAG,
              "===== SUMMARY: %u PASS / %u FAIL =====",
              (unsigned)s_pass_count, (unsigned)s_fail_count);
    if (0U == s_fail_count)
    {
        DEBUG_OUT(d, EM7028_LOG_TAG, "em7028_mock_test_task ALL PASS");
    }
    else
    {
        DEBUG_OUT(e, EM7028_ERR_LOG_TAG,
                  "em7028_mock_test_task %u FAIL -- review log above",
                  (unsigned)s_fail_count);
    }

    for (;;)
    {
        osal_task_delay(5000U);
    }
}
//******************************* Functions *********************************//
