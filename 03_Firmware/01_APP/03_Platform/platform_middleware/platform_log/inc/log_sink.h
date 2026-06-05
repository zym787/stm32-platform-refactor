/******************************************************************************
 * @file log_sink.h
 *
 * @par dependencies
 * - platform_type.h
 * - platform_error.h
 *
 * @author Ethan-Hang
 *
 * @brief Middleware-platform log backend (transport sink) abstraction.
 *
 *        Decouples the logging middleware (EasyLogger) from the physical
 *        transport it writes to.  EasyLogger keeps doing the formatting; the
 *        formatted bytes are handed to whatever sink is registered here
 *        (today SEGGER RTT, tomorrow UART / ITM) without touching the elog
 *        port, the Debug routing table, or any call site.
 *
 *        This is the interface half of the wrapper(03_Platform) +
 *        adapter(04_Impl) + register pattern used across the BSP layer.
 *        A concrete transport implements log_sink_t and calls
 *        log_sink_register() once at startup (before elog_init()).
 *
 * @version V1.0 2026-6-5
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __LOG_SINK_H__
#define __LOG_SINK_H__

//******************************** Includes *********************************//
#include "platform_type.h"
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

/**
 * @brief Log transport sink vtable.
 *
 *        One active sink at a time.  The adapter fills this struct and hands
 *        it to log_sink_register().  pf_write is mandatory; pf_init and
 *        pf_deinit may be NULL when the transport needs no setup/teardown.
 */
typedef struct log_sink
{
    /** Adapter-private context; opaque to the dispatcher. */
    void *user_data;

    /** Bring the transport up. May be NULL (treated as success). */
    platform_err_t (*pf_init)(struct log_sink *self);

    /**
     * Emit one already-formatted log fragment.
     * @param channel virtual channel selector (e.g. RTT terminal index); a
     *                transport that has no notion of channels may ignore it.
     */
    platform_err_t (*pf_write)(struct log_sink *self,
                               uint8_t          channel,
                               const char      *data,
                               size_t           size);

    /** Tear the transport down. May be NULL. */
    void (*pf_deinit)(struct log_sink *self);
} log_sink_t;

//******************************** Defines **********************************//

//******************************** Declaring ********************************//

/**
 * @brief Register the active log transport sink.
 *
 * @param[in] sink : sink vtable to activate; must outlive every log call
 *                   (typically a file-scope static in the adapter)
 *
 * @param[out] : none
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NULL_PTR when sink or its
 *         pf_write is NULL.
 *
 * */
platform_err_t log_sink_register(const log_sink_t *sink);

/**
 * @brief Initialise the active sink. Called from elog_port_init().
 *
 * @param[in] : none
 *
 * @param[out] : none
 *
 * @return PLATFORM_OK on success (or when the sink has no pf_init),
 *         PLATFORM_ERR_NOT_INITIALIZED when no sink is registered, or the
 *         sink's own pf_init result.
 *
 * */
platform_err_t log_sink_init(void);

/**
 * @brief Forward one formatted log fragment to the active sink.
 *
 *        Called from elog_port_output().  When no sink is registered the
 *        fragment is dropped (not buffered) — matching the existing
 *        "RTT buffer full drops the line" non-blocking semantics.
 *
 * @param[in] channel : virtual channel selector (RTT terminal index today)
 * @param[in] data    : pointer to the formatted bytes
 * @param[in] size    : number of bytes at data
 *
 * @param[out] : none
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NOT_INITIALIZED when no sink
 *         is registered, or the sink's own pf_write result.
 *
 * */
platform_err_t log_sink_write(uint8_t channel, const char *data, size_t size);

//******************************** Declaring ********************************//

#endif /* __LOG_SINK_H__ */
