/******************************************************************************
 * @file watchdog_port.h
 *
 * @par dependencies
 * - platform_type.h
 *
 * @author Ethan-Hang
 *
 * @brief MCU-port independent-watchdog abstraction — refresh-only.
 *
 *        On STM32F411 the IWDG is fused-on: once the bootloader writes the
 *        start key (KR = 0xCCCC) the watchdog cannot be disabled until the
 *        next system reset. This port therefore exposes only the refresh
 *        operation; start/stop are intentionally absent because there is no
 *        software stop, and the start is owned by the bootloader's OTA
 *        verification path (not by the APP).
 *
 *        Replacing the MCU only requires reimplementing the source file —
 *        callers never touch IWDG_KR directly.
 *
 * @version V1.0 2026-05-17
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __WATCHDOG_PORT_H__
#define __WATCHDOG_PORT_H__

//******************************** Includes *********************************//
#include "platform_type.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
/**
* @brief Reload the independent-watchdog counter.
*
* @param[in]  : None.
* @param[out] : None.
* @return     : None.
*
* @note Safe to call when IWDG is not active — the write is silently ignored
*       by the peripheral until the start key has been issued. Cost is a
*       single MMIO store; callable from any thread context (not ISR-only).
* */
void mcu_watchdog_refresh(void);
//******************************* Declaring *********************************//

#endif /* __WATCHDOG_PORT_H__ */
