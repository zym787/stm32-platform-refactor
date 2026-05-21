/******************************************************************************
 * @file st7789_mock_test_task.c
 *
 * @par dependencies
 * - user_task_reso_config.h
 * - bsp_st7789_driver.h
 * - bsp_st7789_reg.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief ST7789 driver logic validation without a real panel.
 *
 * Processing flow:
 * Mount fake SPI / timebase / OS vtables that log every CS/DC/RST transition,
 * every SPI byte stream and every ms delay via DEBUG_OUT to RTT Terminal 5
 * (DEBUG_RTT_CH_DISPLAY).  Run pf_st7789_init and a handful of
 * set_addr_window / draw_pixel calls so the RTT log can be diff'd against
 * the ST7789T3 datasheet expectations printed alongside each case.
 *
 * @version V1.0 2026-04-24
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

#include "user_task_reso_config.h"
#include "bsp_st7789_driver.h"
#include "bsp_st7789_reg.h"
#include "Debug.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define ST7789_MOCK_PANEL_WIDTH    240U
#define ST7789_MOCK_PANEL_HEIGHT   320U
#define ST7789_MOCK_PANEL_X_OFFSET 0U
#define ST7789_MOCK_PANEL_Y_OFFSET 0U

#define ST7789_MOCK_BOOT_WAIT_MS   2000U
#define ST7789_MOCK_STEP_GAP_MS    300U

/* Cap hex dump per transmit so a future fill_color cannot flood RTT. */
#define ST7789_MOCK_MAX_DUMP_BYTES 32U

#define ST7789_MOCK_PIN_UNKNOWN    0xFFU
#define ST7789_MOCK_PIN_LOW        0U
#define ST7789_MOCK_PIN_HIGH       1U

/**
 * Must match bsp_st7789_driver.c's ST7789_FILL_TILE_BYTES.  Duplicated here
 * because the driver keeps the constant private; if the driver's tile size
 * changes, update this too or the tile-count assertions below will drift.
 */
#define ST7789_MOCK_TILE_BYTES     512U
#define ST7789_MOCK_FONT_WIDTH       7U
#define ST7789_MOCK_FONT_HEIGHT     10U
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static bsp_st7789_driver_t         s_mock_driver;
static st7789_spi_interface_t      s_mock_spi;
static st7789_timebase_interface_t s_mock_timebase;
static st7789_os_interface_t       s_mock_os;
static const st7789_panel_config_t s_mock_panel = {
    .width    = ST7789_MOCK_PANEL_WIDTH,
    .height   = ST7789_MOCK_PANEL_HEIGHT,
    .x_offset = ST7789_MOCK_PANEL_X_OFFSET,
    .y_offset = ST7789_MOCK_PANEL_Y_OFFSET,
};

/* Only log a pin when its level actually changes; spamming every CS toggle
 * inside a tight command sequence drowns the interesting data. */
static uint8_t s_last_cs       = ST7789_MOCK_PIN_UNKNOWN;
static uint8_t s_last_dc       = ST7789_MOCK_PIN_UNKNOWN;
static uint8_t s_last_rst      = ST7789_MOCK_PIN_UNKNOWN;

/* Monotonic fake tick advanced by every mock delay so pf_get_tick_ms stays
 * coherent with the driver's view of elapsed time. */
static uint32_t s_fake_tick_ms = 0U;

/**
 * Per-case statistics captured by the SPI mock callbacks.  Each case
 * resets these, runs the driver call, then asserts the expected counts /
 * final pin levels against what actually happened.  This turns the log
 * from "eyeball against datasheet" into a self-checking regression gate.
 */
typedef struct
{
    uint32_t tx_count;        /* pf_spi_transmit (blocking) invocations  */
    uint32_t tx_dma_count;    /* pf_spi_transmit_dma invocations         */
    uint32_t wait_dma_count;  /* pf_spi_wait_dma_complete invocations    */
    uint32_t last_tx_dma_len; /* data_length of most recent DMA transfer */
    uint8_t  last_cs;
    uint8_t  last_dc;
} mock_stats_t;

static mock_stats_t s_stats;
static uint32_t     s_pass_count;
static uint32_t     s_fail_count;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
static void mock_hex_dump(const char *label, const uint8_t *p_data,
                          uint32_t data_length)
{
    if ((NULL == p_data) || (0U == data_length))
    {
        DEBUG_OUT(i, ST7789_MOCK_LOG_TAG, "%s: <empty>", label);
        return;
    }

    char     buf[3U * ST7789_MOCK_MAX_DUMP_BYTES + 8U];
    int      pos  = 0;
    uint32_t dump = (data_length > ST7789_MOCK_MAX_DUMP_BYTES)
                        ? ST7789_MOCK_MAX_DUMP_BYTES
                        : data_length;

    for (uint32_t idx = 0U; idx < dump; ++idx)
    {
        int written = snprintf(&buf[pos], sizeof(buf) - (size_t)pos, "%02X ",
                               p_data[idx]);
        if (written <= 0)
        {
            break;
        }
        pos += written;
    }
    if (data_length > dump)
    {
        snprintf(&buf[pos], sizeof(buf) - (size_t)pos, "...");
    }

    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG, "%s len=%u : %s", label,
              (unsigned)data_length, buf);
}

/* ---- SPI mock ------------------------------------------------------------ */
static st7789_status_t mock_spi_init(void)
{
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG, "SPI init");
    return ST7789_OK;
}

static st7789_status_t mock_spi_deinit(void)
{
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG, "SPI deinit");
    return ST7789_OK;
}

static st7789_status_t mock_spi_transmit(uint8_t const *p_data,
                                         uint32_t       data_length)
{
    s_stats.tx_count++;
    mock_hex_dump("TX", p_data, data_length);
    return ST7789_OK;
}

static st7789_status_t mock_spi_transmit_dma(uint8_t const *p_data,
                                             uint32_t       data_length)
{
    s_stats.tx_dma_count++;
    s_stats.last_tx_dma_len = data_length;
    mock_hex_dump("TX-DMA", p_data, data_length);
    return ST7789_OK;
}

static st7789_status_t mock_spi_wait_dma_complete(uint32_t timeout_ms)
{
    s_stats.wait_dma_count++;
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG, "wait_dma_complete timeout=%u ms",
              (unsigned)timeout_ms);
    return ST7789_OK;
}

static st7789_status_t mock_spi_write_cs_pin(uint8_t state)
{
    s_stats.last_cs = state;
    if (state != s_last_cs)
    {
        DEBUG_OUT(i, ST7789_MOCK_LOG_TAG, "CS=%u", (unsigned)state);
        s_last_cs = state;
    }
    return ST7789_OK;
}

static st7789_status_t mock_spi_write_dc_pin(uint8_t state)
{
    s_stats.last_dc = state;
    if (state != s_last_dc)
    {
        DEBUG_OUT(i, ST7789_MOCK_LOG_TAG, "DC=%u (%s)", (unsigned)state,
                  (0U == state) ? "CMD" : "DATA");
        s_last_dc = state;
    }
    return ST7789_OK;
}

static st7789_status_t mock_spi_write_rst_pin(uint8_t state)
{
    if (state != s_last_rst)
    {
        DEBUG_OUT(i, ST7789_MOCK_LOG_TAG, "RST=%u", (unsigned)state);
        s_last_rst = state;
    }
    return ST7789_OK;
}

/* ---- Timebase / OS mock -------------------------------------------------- */
static uint32_t mock_tb_get_tick_ms(void)
{
    return s_fake_tick_ms;
}

static void mock_tb_delay_ms(uint32_t ms)
{
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG, "delay %u ms (busy)", (unsigned)ms);
    s_fake_tick_ms += ms;
}

static void mock_os_delay_ms(uint32_t ms)
{
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG, "delay %u ms (os)", (unsigned)ms);
    s_fake_tick_ms += ms;
}

/* ---- Wire-up ------------------------------------------------------------- */
static st7789_status_t mock_driver_bind(void)
{
    s_mock_spi.pf_spi_init              = mock_spi_init;
    s_mock_spi.pf_spi_deinit            = mock_spi_deinit;
    s_mock_spi.pf_spi_transmit          = mock_spi_transmit;
    s_mock_spi.pf_spi_transmit_dma      = mock_spi_transmit_dma;
    s_mock_spi.pf_spi_wait_dma_complete = mock_spi_wait_dma_complete;
    s_mock_spi.pf_spi_write_cs_pin      = mock_spi_write_cs_pin;
    s_mock_spi.pf_spi_write_dc_pin      = mock_spi_write_dc_pin;
    s_mock_spi.pf_spi_write_rst_pin     = mock_spi_write_rst_pin;

    s_mock_timebase.pf_get_tick_ms      = mock_tb_get_tick_ms;
    s_mock_timebase.pf_delay_ms         = mock_tb_delay_ms;

    s_mock_os.pf_os_delay_ms            = mock_os_delay_ms;

    return bsp_st7789_driver_inst(&s_mock_driver, &s_mock_spi, &s_mock_timebase,
                                  &s_mock_os, &s_mock_panel);
}

/* ---- Pass/fail helpers -------------------------------------------------- */
static void stats_reset(void)
{
    s_stats.tx_count        = 0U;
    s_stats.tx_dma_count    = 0U;
    s_stats.wait_dma_count  = 0U;
    s_stats.last_tx_dma_len = 0U;
    s_stats.last_cs         = ST7789_MOCK_PIN_UNKNOWN;
    s_stats.last_dc         = ST7789_MOCK_PIN_UNKNOWN;
}

static void case_report(const char *name, bool ok)
{
    if (ok)
    {
        s_pass_count++;
        DEBUG_OUT(d, ST7789_MOCK_LOG_TAG, "[PASS] %s", name);
    }
    else
    {
        s_fail_count++;
        DEBUG_OUT(e, ST7789_MOCK_LOG_TAG,
                  "[FAIL] %s  ret-path check failed (see stats above)", name);
    }
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "       stats: tx=%u tx_dma=%u wait=%u last_dma_len=%u"
              " cs=%u dc=%u",
              (unsigned)s_stats.tx_count, (unsigned)s_stats.tx_dma_count,
              (unsigned)s_stats.wait_dma_count,
              (unsigned)s_stats.last_tx_dma_len, (unsigned)s_stats.last_cs,
              (unsigned)s_stats.last_dc);
}

/* ---- Cases --------------------------------------------------------------- */
static void test_case_init(void)
{
    DEBUG_OUT(
        i, ST7789_MOCK_LOG_TAG,
        "===== CASE 1: pf_st7789_init -- expect HW reset + cmd seq =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: RST H->delay10 ->L->delay1 ->H->delay120,"
              " then SLPOUT(11) COLMOD(3A)/55 MADCTL(36)/00 ... ; ret=0"
              " ; clean exit with CS=1");
    stats_reset();
    st7789_status_t ret = s_mock_driver.pf_st7789_init(&s_mock_driver);

    /* Init sends many commands; exact count is fragile across datasheet
     * tweaks, so only assert ret and the CS returned HIGH. */
    const bool ok =
        (ST7789_OK == ret) && (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 1 init", ok);
}

static void test_case_addr_window_full(void)
{
    /* Full-panel fill: 240 x 320 px = 76800 px x 2 B = 153600 B.
     * 153600 / 512 = 300 full tiles, zero remainder. */
    const uint32_t expected_tiles = (uint32_t)ST7789_MOCK_PANEL_WIDTH *
                                    (uint32_t)ST7789_MOCK_PANEL_HEIGHT * 2U /
                                    ST7789_MOCK_TILE_BYTES;

    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 2: fill_region(0,0,239,319,BLACK) full panel =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: CASET=00 00 00 EF ; RASET=00 00 01 3F ; cmd 2C ;"
              " CS=0 DC=1, then %u x (TX-DMA len=512) with wait between,"
              " CS=1 at end ; ret=0",
              (unsigned)expected_tiles);
    stats_reset();
    st7789_status_t ret = s_mock_driver.pf_st7789_fill_region(
        &s_mock_driver, 0U, 0U, (uint16_t)(ST7789_MOCK_PANEL_WIDTH - 1U),
        (uint16_t)(ST7789_MOCK_PANEL_HEIGHT - 1U), BLACK);

    const bool ok = (ST7789_OK == ret) && (5U == s_stats.tx_count) &&
                    (expected_tiles == s_stats.tx_dma_count) &&
                    (expected_tiles == s_stats.wait_dma_count) &&
                    (ST7789_MOCK_TILE_BYTES == s_stats.last_tx_dma_len) &&
                    (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 2 full-panel fill", ok);
}

static void test_case_addr_window_sub(void)
{
    /* 100 x 100 px = 20000 B = 39 full tiles (19968 B) + 1 partial (32 B). */
    const uint32_t total_bytes = 100U * 100U * 2U;
    const uint32_t expected_tiles =
        (total_bytes + ST7789_MOCK_TILE_BYTES - 1U) / ST7789_MOCK_TILE_BYTES;
    const uint32_t expected_tail = total_bytes % ST7789_MOCK_TILE_BYTES;

    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 3: fill_region(10,20,109,119,BLACK) sub-rect =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: CASET=00 0A 00 6D ; RASET=00 14 00 77 ; cmd 2C ;"
              " %u DMA tiles (last len=%u), CS=1 at end ; ret=0",
              (unsigned)expected_tiles, (unsigned)expected_tail);
    stats_reset();
    st7789_status_t ret = s_mock_driver.pf_st7789_fill_region(
        &s_mock_driver, 10U, 20U, 109U, 119U, BLACK);

    const bool ok = (ST7789_OK == ret) && (5U == s_stats.tx_count) &&
                    (expected_tiles == s_stats.tx_dma_count) &&
                    (expected_tiles == s_stats.wait_dma_count) &&
                    (expected_tail == s_stats.last_tx_dma_len) &&
                    (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 3 sub-rect fill", ok);
}

static void test_case_addr_window_out_of_range(void)
{
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 4: fill_region(0,0,240,320) out-of-range =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: ret=ST7789_ERRORPARAMETER (4), no SPI traffic");
    stats_reset();
    st7789_status_t ret = s_mock_driver.pf_st7789_fill_region(
        &s_mock_driver, 0U, 0U, ST7789_MOCK_PANEL_WIDTH,
        ST7789_MOCK_PANEL_HEIGHT, BLACK);

    const bool ok = (ST7789_ERRORPARAMETER == ret) &&
                    (0U == s_stats.tx_count) && (0U == s_stats.tx_dma_count);
    case_report("CASE 4 out-of-range reject", ok);
}

static void test_case_addr_window_inverted(void)
{
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 5: fill_region(100,100,50,50) inverted =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: ret=ST7789_ERRORPARAMETER (4), no SPI traffic");
    stats_reset();
    st7789_status_t ret = s_mock_driver.pf_st7789_fill_region(
        &s_mock_driver, 100U, 100U, 50U, 50U, BLACK);

    const bool ok = (ST7789_ERRORPARAMETER == ret) &&
                    (0U == s_stats.tx_count) && (0U == s_stats.tx_dma_count);
    case_report("CASE 5 inverted reject", ok);
}

static void test_case_draw_pixel(void)
{
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 6: draw_pixel(50,50,RED=0xF800) =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: CASET TX=00 32 00 32 ; RASET TX=00 32 00 32 ;"
              " RAMWR ; TX len=2 : F8 00 ; ret=0 ; no DMA");
    stats_reset();
    st7789_status_t ret =
        s_mock_driver.pf_st7789_draw_pixel(&s_mock_driver, 50U, 50U, RED);

    /* set_addr_window emits 5 blocking TX (CASET cmd+data, RASET cmd+data,
     * RAMWR cmd) and draw_pixel then emits 1 more for the 2-byte pixel. */
    const bool ok = (ST7789_OK == ret) && (6U == s_stats.tx_count) &&
                    (0U == s_stats.tx_dma_count) &&
                    (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 6 draw_pixel", ok);
}

static void test_case_invert_colors(void)
{
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 7: invert_colors(on -> off) =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: INVON then INVOFF, both ret=0, no DMA");
    stats_reset();

    st7789_status_t ret_on = s_mock_driver.pf_invert_colors(&s_mock_driver, 1U);
    st7789_status_t ret_off =
        s_mock_driver.pf_invert_colors(&s_mock_driver, 0U);

    const bool ok = (ST7789_OK == ret_on) && (ST7789_OK == ret_off) &&
                    (2U == s_stats.tx_count) && (0U == s_stats.tx_dma_count) &&
                    (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 7 invert_colors", ok);
}

static void test_case_draw_filled_rectangle(void)
{
    const uint32_t total_bytes = 10U * 10U * 2U;

    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 8: draw_filled_rectangle(29,39,20,30) =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: normalized to 20..29 x 30..39, ret=0, one DMA len=%u",
              (unsigned)total_bytes);
    stats_reset();

    st7789_status_t ret = s_mock_driver.pf_st7789_draw_filled_rectangle(
        &s_mock_driver, 29U, 39U, 20U, 30U, GREEN);

    const bool ok = (ST7789_OK == ret) && (5U == s_stats.tx_count) &&
                    (1U == s_stats.tx_dma_count) &&
                    (1U == s_stats.wait_dma_count) &&
                    (total_bytes == s_stats.last_tx_dma_len) &&
                    (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 8 draw_filled_rectangle", ok);
}

static void test_case_draw_circle(void)
{
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 9: draw_circle(120,160,r=12) =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: ret=0, draw-pixel traffic only (no DMA)");
    stats_reset();

    st7789_status_t ret = s_mock_driver.pf_st7789_draw_circle(
        &s_mock_driver, 120U, 160U, 12U, BLUE);

    const bool ok = (ST7789_OK == ret) && (s_stats.tx_count > 0U) &&
                    (0U == s_stats.tx_dma_count) &&
                    (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 9 draw_circle", ok);
}

static void test_case_draw_filled_triangle(void)
{
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 10: draw_filled_triangle((80,80),(60,110),(100,110)) "
              "=====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: ret=0, horizontal-span fill traffic with DMA");
    stats_reset();

    st7789_status_t ret = s_mock_driver.pf_st7789_draw_filled_triangle(
        &s_mock_driver, 80U, 80U, 60U, 110U, 100U, 110U, RED);

    const bool ok = (ST7789_OK == ret) && (s_stats.tx_dma_count > 0U) &&
                    (s_stats.wait_dma_count == s_stats.tx_dma_count) &&
                    (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 10 draw_filled_triangle", ok);
}

static void test_case_draw_filled_circle(void)
{
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 11: draw_filled_circle(120,160,r=12) =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: ret=0, horizontal-span fill traffic with DMA");
    stats_reset();

    st7789_status_t ret = s_mock_driver.pf_st7789_draw_filled_circle(
        &s_mock_driver, 120U, 160U, 12U, YELLOW);

    const bool ok = (ST7789_OK == ret) && (s_stats.tx_dma_count > 0U) &&
                    (s_stats.wait_dma_count == s_stats.tx_dma_count) &&
                    (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 11 draw_filled_circle", ok);
}

static void test_case_draw_image(void)
{
    static const uint16_t image_4x4[16] = {
        RED, GREEN, BLUE, WHITE,
        GREEN, BLUE, WHITE, RED,
        BLUE, WHITE, RED, GREEN,
        WHITE, RED, GREEN, BLUE,
    };
    const uint32_t expected_bytes = (uint32_t)sizeof(image_4x4);

    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 12: draw_image(30,40,4x4) =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: ret=0, set-window tx=5, one DMA len=%u",
              (unsigned)expected_bytes);
    stats_reset();

    st7789_status_t ret = s_mock_driver.pf_st7789_draw_image(
        &s_mock_driver, 30U, 40U, 4U, 4U, image_4x4);

    const bool ok = (ST7789_OK == ret) && (5U == s_stats.tx_count) &&
                    (1U == s_stats.tx_dma_count) &&
                    (1U == s_stats.wait_dma_count) &&
                    (expected_bytes == s_stats.last_tx_dma_len) &&
                    (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 12 draw_image", ok);
}

static void test_case_draw_char(void)
{
    const uint32_t expected_bytes =
        ST7789_MOCK_FONT_WIDTH * ST7789_MOCK_FONT_HEIGHT * 2U;

    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 13: draw_char(10,20,'A') =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: ret=0, set-window tx=5, one DMA len=%u",
              (unsigned)expected_bytes);
    stats_reset();

    st7789_status_t ret = s_mock_driver.pf_st7789_draw_char(
        &s_mock_driver, 10U, 20U, 'A', YELLOW, BLACK);

    const bool ok = (ST7789_OK == ret) && (5U == s_stats.tx_count) &&
                    (1U == s_stats.tx_dma_count) &&
                    (1U == s_stats.wait_dma_count) &&
                    (expected_bytes == s_stats.last_tx_dma_len) &&
                    (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 13 draw_char", ok);
}

static void test_case_draw_string(void)
{
    static const char test_str[] = "OK";
    const uint32_t expected_chars =
        (uint32_t)(sizeof(test_str) / sizeof(test_str[0])) - 1U;
    const uint32_t expected_bytes =
        ST7789_MOCK_FONT_WIDTH * ST7789_MOCK_FONT_HEIGHT * 2U;

    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== CASE 14: draw_string(10,40,\"OK\") =====");
    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "expected: ret=0, %u chars -> tx=%u, dma=%u",
              (unsigned)expected_chars,
              (unsigned)(expected_chars * 5U),
              (unsigned)expected_chars);
    stats_reset();

    st7789_status_t ret = s_mock_driver.pf_st7789_draw_string(
        &s_mock_driver, 10U, 40U, test_str, CYAN, BLACK);

    const bool ok = (ST7789_OK == ret) &&
                    ((expected_chars * 5U) == s_stats.tx_count) &&
                    (expected_chars == s_stats.tx_dma_count) &&
                    (expected_chars == s_stats.wait_dma_count) &&
                    (expected_bytes == s_stats.last_tx_dma_len) &&
                    (ST7789_MOCK_PIN_HIGH == s_stats.last_cs);
    case_report("CASE 14 draw_string", ok);
}
//******************************* Functions *********************************//

//******************************* Functions *********************************//
void st7789_mock_test_task(void *argument)
{
    (void)argument;

    /* Give EasyLogger and the rest of User_Init a moment so our first lines
     * do not race with boot-time logging from other modules. */
    osal_task_delay(ST7789_MOCK_BOOT_WAIT_MS);

    DEBUG_OUT(d, ST7789_MOCK_LOG_TAG, "st7789_mock_test_task start");

    st7789_status_t bind_ret = mock_driver_bind();
    if (ST7789_OK != bind_ret)
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG, "bsp_st7789_driver_inst failed = %d",
                  (int)bind_ret);
        for (;;)
        {
            osal_task_delay(1000U);
        }
    }

    test_case_init();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_addr_window_full();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_addr_window_sub();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_addr_window_out_of_range();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_addr_window_inverted();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_draw_pixel();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_invert_colors();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_draw_filled_rectangle();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_draw_circle();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_draw_filled_triangle();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_draw_filled_circle();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_draw_image();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_draw_char();
    osal_task_delay(ST7789_MOCK_STEP_GAP_MS);

    test_case_draw_string();

    DEBUG_OUT(i, ST7789_MOCK_LOG_TAG,
              "===== SUMMARY: %u PASS / %u FAIL =====", (unsigned)s_pass_count,
              (unsigned)s_fail_count);
    if (0U == s_fail_count)
    {
        DEBUG_OUT(d, ST7789_MOCK_LOG_TAG, "st7789_mock_test_task ALL PASS");
    }
    else
    {
        DEBUG_OUT(e, ST7789_ERR_LOG_TAG,
                  "st7789_mock_test_task %u FAIL -- review log above",
                  (unsigned)s_fail_count);
    }

    for (;;)
    {
        osal_task_delay(5000U);
    }
}
//******************************* Functions *********************************//
