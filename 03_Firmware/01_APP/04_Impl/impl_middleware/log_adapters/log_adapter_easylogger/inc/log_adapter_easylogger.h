/******************************************************************************
 * @file log_adapter_easylogger.h
 *
 * @par dependencies
 * - elog.h
 *
 * @author Ethan-Hang
 *
 * @brief EasyLogger backend adapter for the logging frontend (log_if.h).
 *
 *        Maps the generic LOG_BK_x backend hooks onto EasyLogger's short-alias
 *        level macros (elog_a/e/w/i/d/v).  This is the ONLY header that knows
 *        about elog.h; log_if.h pulls it in via the compile-time backend
 *        switch.  Replacing EasyLogger means providing a sibling
 *        log_adapter_<lib>.h with the same LOG_BK_x set.
 *
 * @version V1.0 2026-6-5
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __LOG_ADAPTER_EASYLOGGER_H__
#define __LOG_ADAPTER_EASYLOGGER_H__

//******************************** Includes *********************************//
#include "elog.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

/*
 * Backend level hooks consumed by log_if.h's LOG_x macros. elog_a/e/w/i/d/v
 * are EasyLogger's short aliases for assert/error/warn/info/debug/verbose;
 * each expands to elog_output() capturing file/func/line at the call site.
 */
#define LOG_BK_a(tag, ...)  elog_a(tag, ##__VA_ARGS__)
#define LOG_BK_e(tag, ...)  elog_e(tag, ##__VA_ARGS__)
#define LOG_BK_w(tag, ...)  elog_w(tag, ##__VA_ARGS__)
#define LOG_BK_i(tag, ...)  elog_i(tag, ##__VA_ARGS__)
#define LOG_BK_d(tag, ...)  elog_d(tag, ##__VA_ARGS__)
#define LOG_BK_v(tag, ...)  elog_v(tag, ##__VA_ARGS__)

//******************************** Defines **********************************//

#endif /* __LOG_ADAPTER_EASYLOGGER_H__ */
