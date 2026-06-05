/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2015-04-28
 */

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "elog.h"
#include "log_sink.h"
#include "Debug.h"


/**
 * EasyLogger port initialize
 *
 * @return result
 */
ElogErrCode elog_port_init(void)
{
    ElogErrCode result = ELOG_NO_ERR;

    /*
     * Bring the active log transport up via the platform sink abstraction.
     * The concrete transport (RTT today) must already be registered by
     * debug_init() before elog_init() reaches here.  A missing registration
     * is non-fatal: logging is simply dropped, exactly as it would be if the
     * transport were down, so we still report ELOG_NO_ERR and let boot
     * proceed rather than stalling on a debug-only facility.
     */
    (void)log_sink_init();

    return result;
}

/**
 * EasyLogger port deinitialize
 *
 */
void elog_port_deinit(void)
{

    /* add your code here */
}

/**
 * output log port interface
 *
 * @param log output of log
 * @param size log size
 */
void elog_port_output(const char *log, size_t size)
{
    /*
     * g_debug_rtt_channel holds a virtual channel index (0-9) chosen by the
     * Debug.c tag-routing table.  It is transport-agnostic: the active sink
     * decides how to use it (the RTT sink maps it to a Viewer Terminal tab).
     * elog_port_output() is void, so a dropped fragment cannot be reported
     * upward — same non-blocking, drop-on-full behaviour as the former direct
     * RTT write.
     */
    (void)log_sink_write((uint8_t)g_debug_rtt_channel, log, size);
}

/**
 * output lock
 */
void elog_port_output_lock(void)
{
    /*
     * Serialise the format+write phase across tasks.
     * portENTER_CRITICAL() is re-entrant (counted) so nesting with
     * SEGGER_RTT_Write's own BASEPRI lock is safe on Cortex-M4.
     * Do not call DEBUG_OUT() from ISR context.
     */
    // portENTER_CRITICAL();
}

/**
 * output unlock
 */
void elog_port_output_unlock(void)
{
    // portEXIT_CRITICAL();
}

/**
 * get current time interface
 *
 * @return current time
 */
const char *elog_port_get_time(void)
{
    // static char cur_system_time[16] = {0};
    // uint32_t tick = BSP_GetTick();
    
    // snprintf(cur_system_time, sizeof(cur_system_time), "%u", tick);
    
    // return cur_system_time;
    return "";
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char *elog_port_get_p_info(void)
{

    /* add your code here */
    return "";
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char *elog_port_get_t_info(void)
{

    /* add your code here */
    return "";
}
