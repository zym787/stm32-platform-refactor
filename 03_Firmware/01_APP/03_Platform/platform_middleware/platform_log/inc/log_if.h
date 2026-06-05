/******************************************************************************
 * @file log_if.h
 *
 * @par dependencies
 * - platform_error.h
 * - log_adapter_easylogger.h (selected backend)
 *
 * @author Ethan-Hang
 *
 * @brief Middleware-platform logging frontend (facade) abstraction.
 *
 *        Logger-agnostic frontend so upper layers (Debug.h / DEBUG_OUT) bind
 *        to a generic LOG_x macro family instead of a specific logging
 *        library.  The concrete library is chosen at COMPILE TIME via the
 *        backend switch below and supplied by one log_adapter_<lib>.h that
 *        defines the LOG_BK_x macros.  Swapping logging libraries means
 *        adding an adapter and flipping the switch — Debug.h, Debug.c and all
 *        call sites stay untouched.  Pure macros + a one-shot init: zero
 *        runtime overhead, no per-line indirection.
 *
 *        Companion to the backend transport sink (log_sink.h): the frontend
 *        picks the library, the sink picks where formatted bytes go.
 *
 * @version V1.0 2026-6-5
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __LOG_IF_H__
#define __LOG_IF_H__

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

/*
 * Compile-time backend selection. Defaults to EasyLogger. To add a backend:
 * define CFG_LOG_BACKEND_<LIB> in the build, drop a log_adapter_<lib>.h that
 * defines LOG_BK_a/e/w/i/d/v, and add an #elif branch here.
 */
#if !defined(CFG_LOG_BACKEND_EASYLOGGER)
#define CFG_LOG_BACKEND_EASYLOGGER  (1)
#endif

#if CFG_LOG_BACKEND_EASYLOGGER
#include "log_adapter_easylogger.h"
#else
#error "log_if.h: no logging backend selected"
#endif

/*
 * Generic frontend log macros. The level token (a/e/w/i/d/v = assert / error
 * / warn / info / debug / verbose) is part of the macro name, so there is no
 * runtime level argument. Tokens are lowercase to match the existing
 * DEBUG_OUT(LEVEL, ...) call sites verbatim (LOG_##LEVEL must resolve).
 */
#define LOG_a(tag, ...)  LOG_BK_a(tag, ##__VA_ARGS__)
#define LOG_e(tag, ...)  LOG_BK_e(tag, ##__VA_ARGS__)
#define LOG_w(tag, ...)  LOG_BK_w(tag, ##__VA_ARGS__)
#define LOG_i(tag, ...)  LOG_BK_i(tag, ##__VA_ARGS__)
#define LOG_d(tag, ...)  LOG_BK_d(tag, ##__VA_ARGS__)
#define LOG_v(tag, ...)  LOG_BK_v(tag, ##__VA_ARGS__)

//******************************** Defines **********************************//

//******************************** Declaring ********************************//

/**
 * @brief Bring the selected logging library up.
 *
 *        Implemented by the active backend adapter. Must run once at startup,
 *        after the transport sink is registered (the EasyLogger adapter's
 *        elog_init() reaches elog_port_init() -> log_sink_init()).
 *
 * @param[in] : none
 *
 * @param[out] : none
 *
 * @return PLATFORM_OK on success, or a backend-specific error.
 *
 * */
platform_err_t log_frontend_init(void);

//******************************** Declaring ********************************//

#endif /* __LOG_IF_H__ */
