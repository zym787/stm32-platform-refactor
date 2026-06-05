/******************************************************************************
 * @file log_sink.c
 *
 * @par dependencies
 * - log_sink.h
 *
 * @author Ethan-Hang
 *
 * @brief Log transport sink dispatcher (wrapper half of the abstraction).
 *
 *        Holds the single active sink and forwards init/write to it. NULL
 *        safe, no allocation, no locking — serialisation of the format+write
 *        phase stays the responsibility of EasyLogger's output lock, so this
 *        layer adds no new concurrency surface.
 *
 * @version V1.0 2026-6-5
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include "log_sink.h"
//******************************** Includes *********************************//

//******************************* Variables *********************************//

/*
 * Active transport sink. Registered once at startup (before elog_init) and
 * read on every log line. Const-pointer: the dispatcher never mutates the
 * vtable, it only invokes through it.
 */
static const log_sink_t *s_activeSink = NULL;

//******************************* Variables *********************************//

//******************************* Functions *********************************//

/**
 * @brief Register the active log transport sink.
 *
 * @param[in] sink : sink vtable to activate; must outlive every log call
 *
 * @param[out] : none
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NULL_PTR when sink or its
 *         pf_write is NULL.
 *
 * */
platform_err_t log_sink_register(const log_sink_t *sink)
{
    /** pf_write is the only mandatory hook; pf_init/pf_deinit may be NULL. */
    if ((sink == NULL) || (sink->pf_write == NULL))
    {
        return PLATFORM_ERR_NULL_PTR;
    }

    s_activeSink = sink;

    return PLATFORM_OK;
}

/**
 * @brief Initialise the active sink. Called from elog_port_init().
 *
 * @param[in] : none
 *
 * @param[out] : none
 *
 * @return PLATFORM_OK (also when no pf_init is provided),
 *         PLATFORM_ERR_NOT_INITIALIZED when no sink is registered, or the
 *         sink's own pf_init result.
 *
 * */
platform_err_t log_sink_init(void)
{
    /** No sink wired up yet: report it; caller decides whether to proceed. */
    if (s_activeSink == NULL)
    {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    /** A transport with nothing to set up is a valid, successful case. */
    if (s_activeSink->pf_init == NULL)
    {
        return PLATFORM_OK;
    }

    return s_activeSink->pf_init((log_sink_t *)s_activeSink);
}

/**
 * @brief Forward one formatted log fragment to the active sink.
 *
 * @param[in] channel : virtual channel selector (RTT terminal index today)
 * @param[in] data    : pointer to the formatted bytes
 * @param[in] size    : number of bytes at data
 *
 * @param[out] : none
 *
 * @return PLATFORM_OK on success, PLATFORM_ERR_NOT_INITIALIZED when no sink
 *         is registered, PLATFORM_ERR_NULL_PTR on a bad buffer, or the
 *         sink's own pf_write result.
 *
 * */
platform_err_t log_sink_write(uint8_t channel, const char *data, size_t size)
{
    /** Drop (do not buffer) when unconfigured — matches RTT-full semantics. */
    if (s_activeSink == NULL)
    {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    /** Guard the buffer; a zero-size write is a harmless no-op for callers. */
    if (data == NULL)
    {
        return PLATFORM_ERR_NULL_PTR;
    }

    return s_activeSink->pf_write((log_sink_t *)s_activeSink,
                                  channel, data, size);
}

//******************************* Functions *********************************//
