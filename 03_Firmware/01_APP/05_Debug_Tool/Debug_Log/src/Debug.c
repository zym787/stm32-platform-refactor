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
 * @upgrade 2.0:
 * Virtual-terminal routing via SEGGER_RTT_SetTerminal(): all log data
 * travels through physical RTT channel 0, and elog_port_output() inserts
 * SetTerminal escape sequences so J-Link RTT Viewer sorts messages into
 * the correct Terminal tab.
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


int debug_is_tag_allowed(const char *tag)
{
    if ((tag == NULL) || (tag[0] == '\0'))
    {
        return 0;
    }

    return\
            (strcmp(    RTOS_TRACE_TASK_OUT_TAG, tag) == 0)                  ||
            // (strcmp(             UNPACK_LOG_TAG, tag) == 0)                  ||
            (strcmp(      WT588_HANDLER_LOG_TAG, tag) == 0)                  ||
            (strcmp(        MPUXXXX_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(              WT588_LOG_TAG, tag) == 0)                  ||
            // (strcmp(              AHT21_LOG_TAG, tag) == 0)                  ||
            // (strcmp(          TEMP_HUMI_LOG_TAG, tag) == 0)                  ||
            (strcmp(            MPUXXXX_LOG_TAG, tag) == 0)                  ||
            (strcmp(          WT588_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(      TEMP_HUMI_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(          AHT21_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(  WT588_HANDLER_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(          USER_INIT_LOG_TAG, tag) == 0)                  ||
            (strcmp(        HAL_IIC_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(         UNPACK_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(      USER_INIT_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(           CORE_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(     TEMP_HUMI_TEST_LOG_TAG, tag) == 0)                  ||
            (strcmp( TEMP_HUMI_TEST_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(      STACK_MONITOR_LOG_TAG, tag) == 0)                  ||
            (strcmp(  STACK_MONITOR_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(     WT588_HAL_PORT_LOG_TAG, tag) == 0)                  ||
            (strcmp( WT588_HAL_PORT_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(               LIST_LOG_TAG, tag) == 0)                  ||
            (strcmp(           LIST_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(         WT588_TEST_LOG_TAG, tag) == 0)                  ||
            (strcmp(     WT588_TEST_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(             ST7789_LOG_TAG, tag) == 0)                  ||
            (strcmp(        ST7789_MOCK_LOG_TAG, tag) == 0)                  ||
            (strcmp(         ST7789_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(        CST816T_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(            CST816T_LOG_TAG, tag) == 0)                  ||
            (strcmp(    TOUCH_CALIB_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(        TOUCH_CALIB_LOG_TAG, tag) == 0)                  ||
            (strcmp(             W25Q64_LOG_TAG, tag) == 0)                  ||
            (strcmp(         W25Q64_ERR_LOG_TAG, tag) == 0)                  ||
            // (strcmp(       W25Q64_MOCK_LOG_TAG, tag) == 0)                  ||
            (strcmp(    W25Q64_MOCK_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(    W25Q64_HDL_MOCK_LOG_TAG, tag) == 0)                  ||
            (strcmp(W25Q64_HDL_MOCK_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(    W25Q64_HAL_TEST_LOG_TAG, tag) == 0)                  ||
            (strcmp(W25Q64_HAL_TEST_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(             EM7028_LOG_TAG, tag) == 0)                  ||
            (strcmp(         EM7028_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(            HR_ALGO_LOG_TAG, tag) == 0)                  ||
            (strcmp(        HR_ALGO_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(             YMODEM_LOG_TAG, tag) == 0)                  ||
            (strcmp(        YMODEM_FILE_LOG_TAG, tag) == 0)                  ||
            (strcmp(        YMODEM_DATA_LOG_TAG, tag) == 0)                  ||
            (strcmp(      YMODEM_PACKET_LOG_TAG, tag) == 0)
            ;
}

/**
 * @brief Map a log tag string to its designated RTT up-channel index.
 *
 * @param[in] tag : log tag string (one of the *_LOG_TAG macros above)
 *
 * @return RTT channel index (0 = default, 2+ = custom terminals)
 *
 * */
uint8_t debug_tag_to_rtt_channel(const char *tag)
{
    /* === Terminal 1 : stack high-water monitor === */
    if (
            (strcmp(      STACK_MONITOR_LOG_TAG, tag) == 0)                  ||
            (strcmp(  STACK_MONITOR_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(    RTOS_TRACE_TASK_OUT_TAG, tag) == 0)
       )
    {
        return DEBUG_RTT_CH_STACK;
    }

    /* === Terminal 2 : AHT21 and temperature/humidity modules === */
    if (
            (strcmp(              AHT21_LOG_TAG, tag) == 0)                  ||
            (strcmp(          AHT21_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(          TEMP_HUMI_LOG_TAG, tag) == 0)                  ||
            (strcmp(      TEMP_HUMI_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(     TEMP_HUMI_TEST_LOG_TAG, tag) == 0)                  ||
            (strcmp( TEMP_HUMI_TEST_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(               CORE_LOG_TAG, tag) == 0)     
       )
    {
        return DEBUG_RTT_CH_SENSOR0;
    }

    if (
            (strcmp(      WT588_HANDLER_LOG_TAG, tag) == 0)                  ||
            (strcmp(  WT588_HANDLER_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(              WT588_LOG_TAG, tag) == 0)                  ||
            (strcmp(          WT588_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(         WT588_TEST_LOG_TAG, tag) == 0)                  ||
            (strcmp(     WT588_TEST_ERR_LOG_TAG, tag) == 0)
       )
    {
        return DEBUG_RTT_CH_SENSOR1;
    }

    if (
            (strcmp(        MPUXXXX_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(            MPUXXXX_LOG_TAG, tag) == 0)                  ||
            (strcmp(             UNPACK_LOG_TAG, tag) == 0)                  ||
            (strcmp(         UNPACK_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(               LIST_LOG_TAG, tag) == 0)                  ||
            (strcmp(           LIST_ERR_LOG_TAG, tag) == 0)
       )
    {
        return DEBUG_RTT_CH_SENSOR2;
    }

    /* === Terminal 5 : ST7789 TFT-LCD driver === */
    if (
            (strcmp(             ST7789_LOG_TAG, tag) == 0)                  ||
            (strcmp(        ST7789_MOCK_LOG_TAG, tag) == 0)                  ||
            (strcmp(         ST7789_ERR_LOG_TAG, tag) == 0)
       )
    {
        return DEBUG_RTT_CH_DISPLAY;
    }

    /* === Terminal 6 : CST816T capacitive touch driver + calibration === */
    if (
            (strcmp(            CST816T_LOG_TAG, tag) == 0)                  ||
            (strcmp(        CST816T_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(        TOUCH_CALIB_LOG_TAG, tag) == 0)                  ||
            (strcmp(    TOUCH_CALIB_ERR_LOG_TAG, tag) == 0)
       )
    {
        return DEBUG_RTT_CH_TOUCH;
    }

    /* === Terminal 7 : W25Q64 SPI NOR flash driver === */
    if (
            (strcmp(             W25Q64_LOG_TAG, tag) == 0)                  ||
            (strcmp(         W25Q64_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(        W25Q64_MOCK_LOG_TAG, tag) == 0)                  ||
            (strcmp(    W25Q64_MOCK_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(    W25Q64_HDL_MOCK_LOG_TAG, tag) == 0)                  ||
            (strcmp(W25Q64_HDL_MOCK_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(    W25Q64_HAL_TEST_LOG_TAG, tag) == 0)                  ||
            (strcmp(W25Q64_HAL_TEST_ERR_LOG_TAG, tag) == 0)
       )
    {
        return DEBUG_RTT_CH_STORAGE;
    }

    /* === Terminal 8 : EM7028 PPG heart-rate sensor + algorithm === */
    if (
            (strcmp(             EM7028_LOG_TAG, tag) == 0)                  ||
            (strcmp(         EM7028_ERR_LOG_TAG, tag) == 0)                  ||
            (strcmp(            HR_ALGO_LOG_TAG, tag) == 0)                  ||
            (strcmp(        HR_ALGO_ERR_LOG_TAG, tag) == 0)
       )
    {
        return DEBUG_RTT_CH_PPG;
    }

    return DEBUG_RTT_CH_DEFAULT;
}

int debug_is_itm_tag(const char *tag)
{
    if ((tag == NULL) || (tag[0] == '\0'))
    {
        return 0;
    }

    return ( strcmp(           CORE_ITM_LOG_TAG, tag) == 0)                  ||
           ( strcmp(      USER_INIT_ITM_LOG_TAG, tag) == 0);
}


//******************************* Functions *********************************//
