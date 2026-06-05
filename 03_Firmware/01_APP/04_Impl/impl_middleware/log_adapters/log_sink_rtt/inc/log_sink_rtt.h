/******************************************************************************
 * @file log_sink_rtt.h
 *
 * @par dependencies
 * - platform_error.h
 *
 * @author Ethan-Hang
 *
 * @brief SEGGER RTT log transport adapter registration.
 *
 *        Concrete log_sink_t implementation that writes formatted log bytes
 *        to SEGGER RTT physical channel 0, using SetTerminal() to route each
 *        fragment to the virtual terminal named by the channel argument.
 *        This is the only place that knows about SEGGER_RTT.
 *
 *        Call log_sink_rtt_register() once at startup, BEFORE elog_init(),
 *        so the sink is active when elog_port_init() reaches it.
 *
 * @version V1.0 2026-6-5
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __LOG_SINK_RTT_H__
#define __LOG_SINK_RTT_H__

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Declaring ********************************//

/**
 * @brief Register the SEGGER RTT sink as the active log transport.
 *
 * @param[in] : none
 *
 * @param[out] : none
 *
 * @return PLATFORM_OK on success, or the log_sink_register() error.
 *
 * */
platform_err_t log_sink_rtt_register(void);

//******************************** Declaring ********************************//

#endif /* __LOG_SINK_RTT_H__ */
