/******************************************************************************
 * @file bsp_adapter_port_touch.h
 *
 * @par dependencies
 * - bsp_wrapper_touch.h
 *
 * @author Ethan-Hang
 *
 * @brief Adapter port interface for the CST816T capacitive touch controller.
 *
 * The adapter mounts a full vtable (init + inst + read primitives) into
 * bsp_wrapper_touch at startup.  Consumers reach the driver through the
 * wrapper API — including driver instantiation (touch_drv_inst()).  The
 * adapter port header therefore only exposes the registration entry point.
 *
 * @version V1.0 2026-04-26
 * @version V2.0 2026-04-28
 * @version V3.0 2026-04-28
 * @upgrade 2.0: Split register (vtable mount, pre-kernel) and inst
 *               (driver instantiation, in-task) so OSAL-dependent setup
 *               no longer happens before osKernelStart().
 * @upgrade 3.0: inst now lives behind the wrapper vtable; consumers call
 *               touch_drv_inst() from bsp_wrapper_touch.h instead of
 *               coupling to this adapter port directly.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_ADAPTER_PORT_TOUCH_H__
#define __BSP_ADAPTER_PORT_TOUCH_H__

//******************************** Includes *********************************//
#include "bsp_wrapper_touch.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief Mount the touch adapter vtable into bsp_wrapper_touch.
 *
 * Pure vtable wiring; touches no hardware and no OSAL primitive, so it is
 * safe to call from platform_io_register() before osKernelStart().  The
 * driver instance behind the vtable stays uninstantiated until the consumer
 * task calls touch_drv_inst() — until then every wrapper call returns
 * WP_TOUCH_ERRORRESOURCE.
 */
void drv_adapter_touch_register(void);
//******************************* Functions *********************************//

#endif /* __BSP_ADAPTER_PORT_TOUCH_H__ */
