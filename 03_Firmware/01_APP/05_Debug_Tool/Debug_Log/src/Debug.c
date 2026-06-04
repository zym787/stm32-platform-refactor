/******************************************************************************
 * @file Debug.c
 *
 * @par dependencies
 * - "Debug.h"
 *
 * @author Ethan-Hang
 *
 * @brief Debug subsystem initialisation: EasyLogger setup and RTT
 *        virtual-terminal routing initialisation.
 *
 * @version V1.0 2025-11-20
 * @version V2.0 2026-04-13
 * @version V3.0 2026-06-04
 * @upgrade 2.0:
 * Virtual-terminal routing via SEGGER_RTT_SetTerminal(): all log data
 * travels through physical RTT channel 0, and elog_port_output() inserts
 * SetTerminal escape sequences so J-Link RTT Viewer sorts messages into
 * the correct Terminal tab.
 * @upgrade 3.0:
 * Tag routing is now table-driven.  s_route_table[] holds one row per
 * active tag and debug_route_lookup() does a single linear scan, replacing
 * the three separate strcmp chains (allow / channel / ITM).  The table is
 * the single source of truth for every tag's enable state and destination.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "Debug.h"
//******************************** Includes *********************************//

//******************************* Variables *********************************//

/*
 * Virtual terminal selector: written by DEBUG_OUT() before every elog
 * call, read by elog_port_output() to call SEGGER_RTT_SetTerminal().
 * All log data is sent through physical RTT channel 0; this index (0-9)
 * determines which Terminal tab in J-Link RTT Viewer the message appears
 * under.
 */
volatile uint8_t g_debug_rtt_channel = 0u;

//******************************* Variables *********************************//

//******************************* Functions *********************************//

/**
 * @brief Initialise the debug subsystem.
 *
 * Configures EasyLogger output format and starts the logger.
 * Must be called once from the main initialisation sequence before any
 * DEBUG_OUT() call.
 *
 * @param[in] : none
 *
 * @param[out] : none
 *
 * @return void
 *
 * */
void debug_init(void)
{
#if DEBUG
    /** Configure EasyLogger output format for every severity level. */
    elog_init();
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_ERROR , ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_WARN  , ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_INFO  , ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_DEBUG , ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_start();
#endif /* DEBUG */
}

/*
 * g_debug_rtt_channel is written by DEBUG_OUT() immediately before the
 * elog_* call and read by elog_port_output() to select the RTT channel.
 * EasyLogger's output lock (portENTER_CRITICAL) serialises the
 * format+write phase so log messages are never interleaved.
 */


/*
 * Single source of truth for tag routing.  Each row binds a tag literal to
 * its RTT terminal and its routing decision.  The table preserves the exact
 * behaviour of the former three strcmp chains, including their quirks:
 *   - "allowed but unlisted in the channel map" tags resolve to terminal 0
 *     (e.g. USER_INIT, CORE_ERR, WT588_HAL_PORT, the YMODEM family, LVGL).
 *   - CORE_LOG_TAG was never in the allow list, so it stays absent here and
 *     keeps being dropped.
 * Tags that were commented out in the old allow list (AHT21, TEMP_HUMI,
 * UNPACK, W25Q64_MOCK) are likewise omitted; re-enable one by adding a row.
 * Grouped by terminal only for readability — lookup is a flat linear scan.
 */
static const debug_route_t s_route_table[] =
{
    /* --- Terminal 0 (default) : unmapped-but-enabled tags --------------- */
    { USER_INIT_LOG_TAG,           DEBUG_RTT_CH_DEFAULT, DEBUG_ROUTE_RTT },
    { USER_INIT_ERR_LOG_TAG,       DEBUG_RTT_CH_DEFAULT, DEBUG_ROUTE_RTT },
    { HAL_IIC_ERR_LOG_TAG,         DEBUG_RTT_CH_DEFAULT, DEBUG_ROUTE_RTT },
    { CORE_ERR_LOG_TAG,            DEBUG_RTT_CH_DEFAULT, DEBUG_ROUTE_RTT },
    { WT588_HAL_PORT_LOG_TAG,      DEBUG_RTT_CH_DEFAULT, DEBUG_ROUTE_RTT },
    { WT588_HAL_PORT_ERR_LOG_TAG,  DEBUG_RTT_CH_DEFAULT, DEBUG_ROUTE_RTT },
    { YMODEM_LOG_TAG,              DEBUG_RTT_CH_DEFAULT, DEBUG_ROUTE_RTT },
    { YMODEM_FILE_LOG_TAG,         DEBUG_RTT_CH_DEFAULT, DEBUG_ROUTE_RTT },
    { YMODEM_DATA_LOG_TAG,         DEBUG_RTT_CH_DEFAULT, DEBUG_ROUTE_RTT },
    { YMODEM_PACKET_LOG_TAG,       DEBUG_RTT_CH_DEFAULT, DEBUG_ROUTE_RTT },
    { LVGL_LOG_TAG,                DEBUG_RTT_CH_DEFAULT, DEBUG_ROUTE_RTT },

    /* --- Terminal 1 : AHT21 / temperature-humidity --------------------- */
    { AHT21_ERR_LOG_TAG,           DEBUG_RTT_CH_SENSOR0, DEBUG_ROUTE_RTT },
    { TEMP_HUMI_ERR_LOG_TAG,       DEBUG_RTT_CH_SENSOR0, DEBUG_ROUTE_RTT },
    { TEMP_HUMI_TEST_LOG_TAG,      DEBUG_RTT_CH_SENSOR0, DEBUG_ROUTE_RTT },
    { TEMP_HUMI_TEST_ERR_LOG_TAG,  DEBUG_RTT_CH_SENSOR0, DEBUG_ROUTE_RTT },

    /* --- Terminal 2 : WT588 handler / test ----------------------------- */
    { WT588_HANDLER_LOG_TAG,       DEBUG_RTT_CH_SENSOR1, DEBUG_ROUTE_RTT },
    { WT588_HANDLER_ERR_LOG_TAG,   DEBUG_RTT_CH_SENSOR1, DEBUG_ROUTE_RTT },
    { WT588_LOG_TAG,               DEBUG_RTT_CH_SENSOR1, DEBUG_ROUTE_RTT },
    { WT588_ERR_LOG_TAG,           DEBUG_RTT_CH_SENSOR1, DEBUG_ROUTE_RTT },
    { WT588_TEST_LOG_TAG,          DEBUG_RTT_CH_SENSOR1, DEBUG_ROUTE_RTT },
    { WT588_TEST_ERR_LOG_TAG,      DEBUG_RTT_CH_SENSOR1, DEBUG_ROUTE_RTT },

    /* --- Terminal 3 : MPU6050 / motion parsing ------------------------- */
    { MPUXXXX_LOG_TAG,             DEBUG_RTT_CH_SENSOR2, DEBUG_ROUTE_RTT },
    { MPUXXXX_ERR_LOG_TAG,         DEBUG_RTT_CH_SENSOR2, DEBUG_ROUTE_RTT },
    { UNPACK_ERR_LOG_TAG,          DEBUG_RTT_CH_SENSOR2, DEBUG_ROUTE_RTT },
    { LIST_LOG_TAG,                DEBUG_RTT_CH_SENSOR2, DEBUG_ROUTE_RTT },
    { LIST_ERR_LOG_TAG,            DEBUG_RTT_CH_SENSOR2, DEBUG_ROUTE_RTT },

    /* --- Terminal 4 : ST7789 TFT-LCD driver ---------------------------- */
    { ST7789_LOG_TAG,              DEBUG_RTT_CH_DISPLAY, DEBUG_ROUTE_RTT },
    { ST7789_MOCK_LOG_TAG,         DEBUG_RTT_CH_DISPLAY, DEBUG_ROUTE_RTT },
    { ST7789_ERR_LOG_TAG,          DEBUG_RTT_CH_DISPLAY, DEBUG_ROUTE_RTT },

    /* --- Terminal 5 : CST816T touch + calibration ---------------------- */
    { CST816T_LOG_TAG,               DEBUG_RTT_CH_TOUCH, DEBUG_ROUTE_RTT },
    { CST816T_ERR_LOG_TAG,           DEBUG_RTT_CH_TOUCH, DEBUG_ROUTE_RTT },
    { TOUCH_CALIB_LOG_TAG,           DEBUG_RTT_CH_TOUCH, DEBUG_ROUTE_RTT },
    { TOUCH_CALIB_ERR_LOG_TAG,       DEBUG_RTT_CH_TOUCH, DEBUG_ROUTE_RTT },

    /* --- Terminal 6 : W25Q64 SPI NOR flash ----------------------------- */
    { W25Q64_LOG_TAG,              DEBUG_RTT_CH_STORAGE, DEBUG_ROUTE_RTT },
    { W25Q64_ERR_LOG_TAG,          DEBUG_RTT_CH_STORAGE, DEBUG_ROUTE_RTT },
    { W25Q64_MOCK_ERR_LOG_TAG,     DEBUG_RTT_CH_STORAGE, DEBUG_ROUTE_RTT },
    { W25Q64_HDL_MOCK_LOG_TAG,     DEBUG_RTT_CH_STORAGE, DEBUG_ROUTE_RTT },
    { W25Q64_HDL_MOCK_ERR_LOG_TAG, DEBUG_RTT_CH_STORAGE, DEBUG_ROUTE_RTT },
    { W25Q64_HAL_TEST_LOG_TAG,     DEBUG_RTT_CH_STORAGE, DEBUG_ROUTE_RTT },
    { W25Q64_HAL_TEST_ERR_LOG_TAG, DEBUG_RTT_CH_STORAGE, DEBUG_ROUTE_RTT },

    /* --- Terminal 7 : EM7028 PPG heart-rate ---------------------------- */
    { EM7028_LOG_TAG,                  DEBUG_RTT_CH_PPG, DEBUG_ROUTE_RTT },
    { EM7028_ERR_LOG_TAG,              DEBUG_RTT_CH_PPG, DEBUG_ROUTE_RTT },
    { HR_ALGO_LOG_TAG,                 DEBUG_RTT_CH_PPG, DEBUG_ROUTE_RTT },
    { HR_ALGO_ERR_LOG_TAG,             DEBUG_RTT_CH_PPG, DEBUG_ROUTE_RTT },

    /* --- Terminal 8 : stack high-water monitor ------------------------- */
    { STACK_MONITOR_LOG_TAG,         DEBUG_RTT_CH_STACK, DEBUG_ROUTE_RTT },
    { STACK_MONITOR_ERR_LOG_TAG,     DEBUG_RTT_CH_STACK, DEBUG_ROUTE_RTT },
    { RTOS_TRACE_TASK_OUT_TAG,       DEBUG_RTT_CH_STACK, DEBUG_ROUTE_RTT },

    /* --- ITM / SWO path (channel field unused) ------------------------- */
    { CORE_ITM_LOG_TAG,            0u,                   DEBUG_ROUTE_ITM },
    { USER_INIT_ITM_LOG_TAG,       0u,                   DEBUG_ROUTE_ITM },
};

/**
 * @brief Resolve a log tag to its routing record.
 *
 * @param[in] tag : log tag string (one of the *_LOG_TAG / *_ITM_LOG_TAG
 *                  macros)
 *
 * @param[out] : none
 *
 * @return pointer to the matching s_route_table[] row, or NULL when the tag
 *         is empty or absent from the table (caller emits nothing)
 *
 * */
const debug_route_t *debug_route_lookup(const char *tag)
{
    /** Reject empty/NULL tags up front so the scan never dereferences them. */
    if ((tag == NULL) || (tag[0] == '\0'))
    {
        return NULL;
    }

    /** Flat linear scan: first exact match wins, mirroring the old chains. **/
    size_t table_size = sizeof(s_route_table) / sizeof(s_route_table[0]);
    for (size_t i = 0u; i < table_size; i++)
    {
        if (strcmp(s_route_table[i].tag, tag) == 0)
        {
            return &s_route_table[i];
        }
    }

    return NULL;
}

//******************************* Functions *********************************//
