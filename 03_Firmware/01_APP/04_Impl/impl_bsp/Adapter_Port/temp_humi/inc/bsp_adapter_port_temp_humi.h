/******************************************************************************
 * @file bsp_adapter_port_temp_humi.h
 *
 * @par dependencies
 * - bsp_wrapper_temp_humi.h
 *
 * @author Ethan-Hang
 *
 * @brief Adapter port interface for AHT21 temperature/humidity sensor.
 *
 * Registers a concrete driver implementation into the bsp_wrapper_temp_humi
 * vtable.
 *
 * Read mode is selected by application API:
 * - *_sync: caller blocks until data is ready.
 * - *_async: caller returns immediately and receives data via callback.
 *
 * @version V1.0 2026-04-12
 * @version V2.0 2026-04-12
 * @upgrade 2.0: Wrapper read functions made synchronous via internal event
 *               group; no changes to handler or driver layers.
 * @version V3.0 2026-04-12
 * @upgrade 3.0: Mutex serialisation + request-ID binding for full concurrency
 *               safety; stale callbacks are discarded; sync API returns
 *               temp_humi_status_t; async API accepts user_ctx.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_ADAPTER_PORT_TEMP_HUMI_H__
#define __BSP_ADAPTER_PORT_TEMP_HUMI_H__

//******************************** Includes *********************************//
#include "bsp_wrapper_temp_humi.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Register the AHT21 driver into the bsp_wrapper_temp_humi vtable.
 *
 * Call once during BSP initialisation, before any wrapper read call.
 *
 * @return true on success.
 */
bool drv_adapter_temp_humi_register(void);
//******************************* Functions *********************************//

#endif /* __BSP_ADAPTER_PORT_TEMP_HUMI_H__ */
