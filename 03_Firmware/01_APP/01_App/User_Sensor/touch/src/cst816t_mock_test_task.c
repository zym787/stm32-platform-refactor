/******************************************************************************
 * @file cst816t_mock_test_task.c
 *
 * @par dependencies
 * - user_task_reso_config.h
 * - bsp_cst816t_driver.h
 * - bsp_cst816t_reg.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief CST816T driver logic validation without a real touch panel.
 *
 * Processing flow:
 * Mount fake I2C / timebase / delay / OS-yield vtables that capture every
 * mem read / write / delay call so the driver entry points can be exercised
 * against the HYN19x2-AN-CST816T register map datasheet.  Output is routed
 * to RTT Terminal 6 (DEBUG_RTT_CH_TOUCH) and each case self-asserts on the
 * captured register address, payload and invocation counts so a clean log
 * becomes a go/no-go signal instead of "eyeball against the datasheet".
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
#include <string.h>

#include "user_task_reso_config.h"
#include "bsp_cst816t_driver.h"
#include "bsp_cst816t_reg.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define CST816T_MOCK_EXPECTED_CHIP_ID       0xB4u
#define CST816T_MOCK_SENTINEL_PTR           ((void *)0xC081C081u)

#define CST816T_MOCK_BOOT_WAIT_MS           2000u
#define CST816T_MOCK_STEP_GAP_MS             200u

#define CST816T_MOCK_EXPECTED_BOOT_DELAY_MS   50u
#define CST816T_MOCK_EXPECTED_IRQ_PULSE       10u
#define CST816T_MOCK_EXPECTED_NOR_SCAN_PER     1u
#define CST816T_MOCK_EXPECTED_LONG_PRESS_TICK 100u
#define CST816T_MOCK_EXPECTED_MOTION_MASK      5u   /* MOTION_ALLENABLE     */
#define CST816T_MOCK_EXPECTED_AUTO_SLEEP_TIME  2u
#define CST816T_MOCK_EXPECTED_IRQ_CTL        0x11u  /* EN_MOTION | ONCE_WLP */
#define CST816T_MOCK_EXPECTED_DIS_AUTO_SLEEP  0x01u
#define CST816T_MOCK_EXPECTED_INIT_WRITES      7u   /* see cst816t_init     */
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static bsp_cst816t_driver_t              s_mock_driver;
static cst816t_iic_driver_interface_t    s_mock_iic;
static cst816t_timebase_interface_t      s_mock_timebase;
static cst816t_delay_interface_t         s_mock_delay;
static cst816t_os_delay_interface_t      s_mock_os;
static void (*s_int_cb)(void *, void *)  = NULL;

/**
 * Per-case capture: every I2C op lands here so the case body can assert on
 * register address, payload, length and call count.  The "next_read" buffer
 * lets a test seed a synthetic chip response before calling the driver.
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
} mock_stats_t;

static mock_stats_t s_stats;
static uint8_t      s_next_read[8];
static uint16_t     s_next_read_len;
static uint32_t     s_fake_tick_ms;

static uint32_t     s_pass_count;
static uint32_t     s_fail_count;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
static void stats_reset(void)
{
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
static cst816t_status_t mock_iic_init(void const * const hi2c)
{
    (void)hi2c;
    s_stats.iic_init_count++;
    DEBUG_OUT(i, CST816T_LOG_TAG, "mock iic_init");
    return CST816T_OK;
}

static cst816t_status_t mock_iic_deinit(void const * const hi2c)
{
    (void)hi2c;
    s_stats.iic_deinit_count++;
    DEBUG_OUT(i, CST816T_LOG_TAG, "mock iic_deinit");
    return CST816T_OK;
}

static cst816t_status_t mock_iic_mem_write(void    *i2c,
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
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "mock mem_write addr=0x%02X reg=0x%02X len=%u data0=0x%02X",
              (unsigned)des_addr, (unsigned)mem_addr, (unsigned)size,
              (unsigned)s_stats.last_write_byte);
    return CST816T_OK;
}

static cst816t_status_t mock_iic_mem_read(void    *i2c,
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
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "mock mem_read addr=0x%02X reg=0x%02X len=%u served=%u",
              (unsigned)des_addr, (unsigned)mem_addr, (unsigned)size,
              (unsigned)copy);
    return CST816T_OK;
}

/* ---- Timebase / delay / OS mocks ---------------------------------------- */
static uint32_t mock_tb_get_tick_count(void)
{
    return s_fake_tick_ms;
}

static void mock_delay_init(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG, "mock delay_init");
}

static void mock_delay_ms(uint32_t const ms)
{
    s_stats.delay_ms_count++;
    s_stats.last_delay_ms = ms;
    s_fake_tick_ms += ms;
    DEBUG_OUT(i, CST816T_LOG_TAG, "mock delay_ms %u", (unsigned)ms);
}

static void mock_delay_us(uint32_t const us)
{
    (void)us;
}

static void mock_os_yield(uint32_t const ms)
{
    s_stats.yield_ms_count++;
    s_stats.last_yield_ms = ms;
    s_fake_tick_ms += ms;
    DEBUG_OUT(i, CST816T_LOG_TAG, "mock os_yield %u", (unsigned)ms);
}

/* ---- Wire-up ------------------------------------------------------------- */
/**
 * @brief      Bind mock vtables and perform the driver instantiation path.
 *
 * @param[in]  : None.
 *
 * @param[out] : None.
 *
 * @return     cst816t_status_t CST816T_OK on success, error code otherwise.
 * */
static cst816t_status_t mock_driver_bind(void)
{
    s_mock_iic.hi2c              = CST816T_MOCK_SENTINEL_PTR;
    s_mock_iic.pf_iic_init       = mock_iic_init;
    s_mock_iic.pf_iic_deinit     = mock_iic_deinit;
    s_mock_iic.pf_iic_mem_write  = mock_iic_mem_write;
    s_mock_iic.pf_iic_mem_read   = mock_iic_mem_read;

    s_mock_timebase.pf_get_tick_count = mock_tb_get_tick_count;

    s_mock_delay.pf_delay_init   = mock_delay_init;
    s_mock_delay.pf_delay_ms     = mock_delay_ms;
    s_mock_delay.pf_delay_us     = mock_delay_us;

    s_mock_os.pf_rtos_yield      = mock_os_yield;

    /**
     * Arm the CHIPID response before inst, because bsp_cst816t_inst invokes
     * cst816t_init which reads CHIPID synchronously.
     **/
    uint8_t chip_id = CST816T_MOCK_EXPECTED_CHIP_ID;
    mock_arm_read(&chip_id, 1U);

    return bsp_cst816t_inst(&s_mock_driver, &s_mock_iic, &s_mock_timebase,
                            &s_mock_delay, &s_mock_os, &s_int_cb);
}

/* ---- Pass/fail helpers -------------------------------------------------- */
static void case_report(const char *name, bool ok)
{
    if (ok)
    {
        s_pass_count++;
        DEBUG_OUT(d, CST816T_LOG_TAG, "[PASS] %s", name);
    }
    else
    {
        s_fail_count++;
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "[FAIL] %s  stats w=%u r=%u reg_w=0x%02X reg_r=0x%02X"
                  " byte=0x%02X",
                  name,
                  (unsigned)s_stats.write_count, (unsigned)s_stats.read_count,
                  (unsigned)s_stats.last_write_reg,
                  (unsigned)s_stats.last_read_reg,
                  (unsigned)s_stats.last_write_byte);
    }
}

/**
 * @brief      Shared body for single-byte register-write cases.
 *
 * @param[in]  name   : Human-readable case label for logging.
 *
 * @param[in]  reg    : Expected 8-bit register the driver should hit.
 *
 * @param[in]  value  : Expected payload byte the driver should send.
 *
 * @param[in]  ret    : Status value returned by the driver call.
 *
 * @return     None.
 * */
static void assert_single_write(const char *name, uint8_t reg, uint8_t value,
                                cst816t_status_t ret)
{
    const bool ok = (CST816T_OK == ret)                              &&
                    (1U == s_stats.write_count)                      &&
                    (0U == s_stats.read_count)                       &&
                    (reg == s_stats.last_write_reg)                  &&
                    (1U == s_stats.last_write_len)                   &&
                    (value == s_stats.last_write_byte)               &&
                    (CST816T_MOCK_SENTINEL_PTR == s_stats.last_i2c_handle);
    case_report(name, ok);
}

/* ---- Cases --------------------------------------------------------------- */
static void test_case_init(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 1: pf_cst816t_init -- boot delay + chip id + "
              "default cfg =====");
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "expected: iic_init=1, yield=1 ms=%u, read CHIPID(0xA7)=0xB4,"
              " then %u writes: IRQPLUSEWIDTH/NORSCANPER/LONGPRESSTICK/"
              "MOTIONMASK/AUTOSLEEPTIME/IRQCTL/DISAUTOSLEEP",
              (unsigned)CST816T_MOCK_EXPECTED_BOOT_DELAY_MS,
              (unsigned)CST816T_MOCK_EXPECTED_INIT_WRITES);
    stats_reset();
    uint8_t chip_id = CST816T_MOCK_EXPECTED_CHIP_ID;
    mock_arm_read(&chip_id, 1U);

    cst816t_status_t ret = s_mock_driver.pf_cst816t_init(s_mock_driver);

    const bool ok = (CST816T_OK == ret)                                     &&
                    (1U == s_stats.iic_init_count)                          &&
                    (1U == s_stats.yield_ms_count)                          &&
                    (CST816T_MOCK_EXPECTED_BOOT_DELAY_MS ==
                                                     s_stats.last_yield_ms) &&
                    (1U == s_stats.read_count)                              &&
                    (CHIPID == s_stats.last_read_reg)                       &&
                    (CST816T_MOCK_EXPECTED_INIT_WRITES ==
                                                      s_stats.write_count) &&
                    (DISAUTOALEEP == s_stats.last_write_reg)                &&
                    (CST816T_MOCK_EXPECTED_DIS_AUTO_SLEEP ==
                                                    s_stats.last_write_byte);
    case_report("CASE 1 init", ok);
}

static void test_case_get_chip_id(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 2: pf_cst816t_get_chip_id -- read 0xA7 =====");
    stats_reset();
    uint8_t chip_id_mock = CST816T_MOCK_EXPECTED_CHIP_ID;
    mock_arm_read(&chip_id_mock, 1U);

    uint8_t chip_id = 0U;
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_get_chip_id(s_mock_driver, &chip_id);

    const bool ok = (CST816T_OK == ret)                    &&
                    (1U == s_stats.read_count)             &&
                    (0U == s_stats.write_count)            &&
                    (CHIPID == s_stats.last_read_reg)      &&
                    (1U == s_stats.last_read_len)          &&
                    (CST816T_MOCK_EXPECTED_CHIP_ID == chip_id);
    case_report("CASE 2 get_chip_id", ok);
}

static void test_case_get_finger_num(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 3: pf_cst816t_get_finger_num -- read 0x02 =====");
    stats_reset();
    uint8_t fingers = 1U;
    mock_arm_read(&fingers, 1U);

    uint8_t num = 0U;
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_get_finger_num(s_mock_driver, &num);

    const bool ok = (CST816T_OK == ret)                 &&
                    (1U == s_stats.read_count)          &&
                    (FINGER_NUM == s_stats.last_read_reg) &&
                    (1U == s_stats.last_read_len)       &&
                    (1U == num);
    case_report("CASE 3 get_finger_num", ok);
}

static void test_case_get_gesture_id(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 4: pf_cst816t_get_gesture_id -- read 0x01 (CLICK) "
              "=====");
    stats_reset();
    uint8_t gesture_raw = (uint8_t)CLICK;   /* 0x05 */
    mock_arm_read(&gesture_raw, 1U);

    cst816t_gesture_id_t gesture = NOGESTURE;
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_get_gesture_id(s_mock_driver, &gesture);

    const bool ok = (CST816T_OK == ret)                   &&
                    (1U == s_stats.read_count)            &&
                    (GESTURE_ID == s_stats.last_read_reg) &&
                    (1U == s_stats.last_read_len)         &&
                    (CLICK == gesture);
    case_report("CASE 4 get_gesture_id", ok);
}

static void test_case_get_xy_axis(void)
{
    /* XposH[3:0]=0x0, XposL=0x78 -> X=0x078=120
     * YposH[3:0]=0x0, YposL=0xC8 -> Y=0x0C8=200 */
    static const uint8_t xy_raw[4] = {0x00U, 0x78U, 0x00U, 0xC8U};
    const uint16_t expected_x = 120U;
    const uint16_t expected_y = 200U;

    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 5: pf_cst816t_get_xy_axis -- burst 4B from 0x03 "
              "=====");
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "expected: x=%u y=%u (driver masks XposH/YposH high nibbles)",
              (unsigned)expected_x, (unsigned)expected_y);
    stats_reset();
    mock_arm_read(xy_raw, sizeof(xy_raw));

    cst816t_xy_t xy = {0U, 0U};
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_get_xy_axis(s_mock_driver, &xy);

    const bool ok = (CST816T_OK == ret)                &&
                    (1U == s_stats.read_count)         &&
                    (X_POSH == s_stats.last_read_reg)  &&
                    (4U == s_stats.last_read_len)      &&
                    (expected_x == xy.x_pos)           &&
                    (expected_y == xy.y_pos);
    case_report("CASE 5 get_xy_axis", ok);
}

static void test_case_sleep(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 6: pf_cst816t_sleep -- write 0x03 to SLEEPMODE "
              "=====");
    stats_reset();
    cst816t_status_t ret = s_mock_driver.pf_cst816t_sleep(s_mock_driver);
    assert_single_write("CASE 6 sleep", SLEEPMODE, 0x03U, ret);
}

static void test_case_wakeup(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 7: pf_cst816t_wakeup -- poke DISAUTOSLEEP with "
              "0x01 =====");
    stats_reset();
    cst816t_status_t ret = s_mock_driver.pf_cst816t_wakeup(s_mock_driver);
    assert_single_write("CASE 7 wakeup", DISAUTOALEEP, 0x01U, ret);
}

static void test_case_set_err_reset_ctl(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 8: set_err_reset_ctl(ERR_RESET_DOUBLE=0x01) =====");
    stats_reset();
    cst816t_status_t ret = s_mock_driver.pf_cst816t_set_err_reset_ctl(
        s_mock_driver, ERR_RESET_DOUBLE);
    assert_single_write("CASE 8 set_err_reset_ctl", ERRRESETCTL,
                        (uint8_t)ERR_RESET_DOUBLE, ret);
}

static void test_case_set_long_press_tick(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 9: set_long_press_tick(0x64) -> LONGPRESSTICK "
              "=====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_long_press_tick(s_mock_driver, 0x64U);
    assert_single_write("CASE 9 set_long_press_tick", LONGPRESSTICK, 0x64U,
                        ret);
}

static void test_case_set_motion_mask(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 10: set_motion_mask(EN_DCLICK) =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_motion_mask(s_mock_driver, EN_DCLICK);
    assert_single_write("CASE 10 set_motion_mask", MOTIONMASK,
                        (uint8_t)EN_DCLICK, ret);
}

static void test_case_set_irq_pulse_width(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 11: set_irq_pulse_width(20) -> IRQPLUSEWIDTH =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_irq_pulse_width(s_mock_driver, 20U);
    assert_single_write("CASE 11 set_irq_pulse_width", IRQPLUSEWIDTH, 20U,
                        ret);
}

static void test_case_set_nor_scan_per(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 12: set_nor_scan_per(5) -> NORSCANPER =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_nor_scan_per(&s_mock_driver, 5U);
    assert_single_write("CASE 12 set_nor_scan_per", NORSCANPER, 5U, ret);
}

static void test_case_set_motion_slope_angle(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 13: set_motion_slope_angle(0x33) -> MOTIONSLANGLE "
              "=====");
    stats_reset();
    cst816t_status_t ret = s_mock_driver.pf_cst816t_set_motion_slope_angle(
        &s_mock_driver, 0x33U);
    assert_single_write("CASE 13 set_motion_slope_angle", MOTIONSLANGLE,
                        0x33U, ret);
}

static void test_case_set_lp_auto_wake_time(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 14: set_low_power_auto_wake_time(3) -> "
              "LPAUTOWAKETIME =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_low_power_auto_wake_time(&s_mock_driver,
                                                              3U);
    assert_single_write("CASE 14 set_lp_auto_wake_time", LPAUTOWAKETIME, 3U,
                        ret);
}

static void test_case_set_lp_scan_th(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 15: set_lp_scan_th(0x20) -> LPSCANTH =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_lp_scan_th(&s_mock_driver, 0x20U);
    assert_single_write("CASE 15 set_lp_scan_th", LPSCANTH, 0x20U, ret);
}

static void test_case_set_lp_scan_win(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 16: set_lp_scan_win(3) -> LPSCANWIN =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_lp_scan_win(&s_mock_driver, 3U);
    assert_single_write("CASE 16 set_lp_scan_win", LPSCANWIN, 3U, ret);
}

static void test_case_set_lp_scan_freq(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 17: set_lp_scan_freq(7) -> LPSCANFREQ =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_lp_scan_freq(&s_mock_driver, 7U);
    assert_single_write("CASE 17 set_lp_scan_freq", LPSCANFREQ, 7U, ret);
}

static void test_case_set_lp_scan_idac(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 18: set_lp_scan_idac(0x40) -> LPSCANIDAC =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_lp_scan_idac(&s_mock_driver, 0x40U);
    assert_single_write("CASE 18 set_lp_scan_idac", LPSCANIDAC, 0x40U, ret);
}

static void test_case_set_auto_sleep_time(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 19: set_auto_sleep_time(5) -> AUTOSLEEPTIME =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_auto_sleep_time(&s_mock_driver, 5U);
    assert_single_write("CASE 19 set_auto_sleep_time", AUTOSLEEPTIME, 5U, ret);
}

static void test_case_set_irq_ctl(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 20: set_irq_ctl(EN_TOUCH) -> IRQCTL =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_irq_ctl(&s_mock_driver, EN_TOUCH);
    assert_single_write("CASE 20 set_irq_ctl", IRQCTL, (uint8_t)EN_TOUCH, ret);
}

static void test_case_set_auto_reset(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 21: set_auto_reset(5) -> AUTORESET =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_auto_reset(&s_mock_driver, 5U);
    assert_single_write("CASE 21 set_auto_reset", AUTORESET, 5U, ret);
}

static void test_case_set_long_press_time(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 22: set_long_press_time(10) -> LONGPRESSTIME =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_long_press_time(&s_mock_driver, 10U);
    assert_single_write("CASE 22 set_long_press_time", LONGPRESSTIME, 10U,
                        ret);
}

static void test_case_set_io_ctl(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 23: set_io_ctl(0x02) -> IOCTL (IIC OD) =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_set_io_ctl(&s_mock_driver, 0x02U);
    assert_single_write("CASE 23 set_io_ctl", IOCTL, 0x02U, ret);
}

static void test_case_disable_auto_sleep(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 24: disable_auto_sleep(1) -> DISAUTOSLEEP =====");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_disable_auto_sleep(&s_mock_driver, 1U);
    assert_single_write("CASE 24 disable_auto_sleep", DISAUTOALEEP, 1U, ret);
}

static void test_case_null_param_rejected(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 25: NULL output pointer must be rejected =====");
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "expected: ret=CST816T_ERRORPARAMETER (4), no I2C traffic");
    stats_reset();
    cst816t_status_t ret =
        s_mock_driver.pf_cst816t_get_xy_axis(s_mock_driver, NULL);

    const bool ok = (CST816T_ERRORPARAMETER == ret) &&
                    (0U == s_stats.read_count)      &&
                    (0U == s_stats.write_count);
    case_report("CASE 25 null-arg reject", ok);
}

static void test_case_deinit(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 26: pf_cst816t_deinit -- iic_deinit invoked =====");
    stats_reset();
    cst816t_status_t ret = s_mock_driver.pf_cst816t_deinit(&s_mock_driver);

    const bool ok = (CST816T_OK == ret)                    &&
                    (1U == s_stats.iic_deinit_count)       &&
                    (0U == s_stats.write_count)            &&
                    (0U == s_stats.read_count);
    case_report("CASE 26 deinit", ok);
}

static void test_case_int_callback_registered(void)
{
    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== CASE 27: bsp_cst816t_inst populates pp_int_callback "
              "=====");
    const bool ok = (s_int_cb != NULL);
    if (ok)
    {
        /**
         * Invoke once with NULL 'this' to make sure the default hook path
         * does not dereference NULL; the mock driver's gs_p_active_instance
         * was set by inst so the NULL path takes the early-return branch.
         **/
        s_int_cb(NULL, NULL);
    }
    case_report("CASE 27 int_callback registered", ok);
}
//******************************* Functions *********************************//

//******************************* Functions *********************************//
/**
 * @brief      Entry point for the CST816T mock test task.
 *
 * @param[in]  argument : Unused task argument.
 *
 * @return     None.
 * */
void cst816t_mock_test_task(void *argument)
{
    (void)argument;

    /**
     * Stagger against EasyLogger warm-up so our banner lines are not
     * shredded by concurrent boot logs from the rest of User_Init.
     **/
    osal_task_delay(CST816T_MOCK_BOOT_WAIT_MS);

    DEBUG_OUT(d, CST816T_LOG_TAG, "cst816t_mock_test_task start");

    cst816t_status_t bind_ret = mock_driver_bind();
    if (CST816T_OK != bind_ret)
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "bsp_cst816t_inst failed = %d (mock bind)",
                  (int)bind_ret);
        for (;;)
        {
            osal_task_delay(1000U);
        }
    }

    test_case_init();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_get_chip_id();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_get_finger_num();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_get_gesture_id();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_get_xy_axis();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_sleep();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_wakeup();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_err_reset_ctl();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_long_press_tick();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_motion_mask();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_irq_pulse_width();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_nor_scan_per();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_motion_slope_angle();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_lp_auto_wake_time();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_lp_scan_th();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_lp_scan_win();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_lp_scan_freq();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_lp_scan_idac();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_auto_sleep_time();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_irq_ctl();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_auto_reset();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_long_press_time();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_set_io_ctl();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_disable_auto_sleep();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_null_param_rejected();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_int_callback_registered();
    osal_task_delay(CST816T_MOCK_STEP_GAP_MS);

    test_case_deinit();

    DEBUG_OUT(i, CST816T_LOG_TAG,
              "===== SUMMARY: %u PASS / %u FAIL =====",
              (unsigned)s_pass_count, (unsigned)s_fail_count);
    if (0U == s_fail_count)
    {
        DEBUG_OUT(d, CST816T_LOG_TAG, "cst816t_mock_test_task ALL PASS");
    }
    else
    {
        DEBUG_OUT(e, CST816T_ERR_LOG_TAG,
                  "cst816t_mock_test_task %u FAIL -- review log above",
                  (unsigned)s_fail_count);
    }

    for (;;)
    {
        osal_task_delay(5000U);
    }
}
//******************************* Functions *********************************//
