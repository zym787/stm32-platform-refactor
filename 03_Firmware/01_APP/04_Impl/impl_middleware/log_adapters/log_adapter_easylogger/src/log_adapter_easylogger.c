/******************************************************************************
 * @file log_adapter_easylogger.c
 *
 * @par dependencies
 * - log_if.h
 * - log_adapter_easylogger.h
 * - elog.h
 *
 * @author Ethan-Hang
 *
 * @brief EasyLogger backend bring-up for the logging frontend.
 *
 *        Implements log_frontend_init() by configuring and starting
 *        EasyLogger.  This is the only translation unit that initialises the
 *        elog library; the startup block was relocated here from debug_init()
 *        so Debug.c stays logger-agnostic.
 *
 * @version V1.0 2026-6-5
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include "log_if.h"
#include "log_adapter_easylogger.h"
#include "elog.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//

/**
 * @brief Bring EasyLogger up: init the library, fix the per-level format,
 *        then enable output.
 *
 *        Format is LVL|TAG for ASSERT..DEBUG — no time/file/func/line — to
 *        keep RTT lines compact.  VERBOSE is intentionally left at its
 *        elog_init() default, matching the former debug_init() behaviour byte
 *        for byte.  elog_init() runs the port init chain (elog_port_init ->
 *        log_sink_init), so the transport sink must already be registered
 *        before this is called.
 *
 * @param[in] : none
 *
 * @param[out] : none
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_GENERAL if elog_init() fails.
 *
 * */
platform_err_t log_frontend_init(void)
{
    /** Bring the library + its port (transport) up. */
    if (elog_init() != ELOG_NO_ERR)
    {
        return PLATFORM_ERR_GENERAL;
    }

    /** Compact output: level + tag only, ASSERT..DEBUG (VERBOSE untouched). */
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_ERROR , ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_WARN  , ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_INFO  , ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_DEBUG , ELOG_FMT_LVL | ELOG_FMT_TAG);

    /** Enable actual output. */
    elog_start();

    return PLATFORM_OK;
}

//******************************* Functions *********************************//
