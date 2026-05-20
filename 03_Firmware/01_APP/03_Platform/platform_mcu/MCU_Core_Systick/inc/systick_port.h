/******************************************************************************
 * @file systick_port.h
 *
 * @par dependencies
 * - stm32f4xx_hal.h
 *
 * @author Ethan-Hang
 *
 * @brief MCU-port systick abstraction — monotonic millisecond timebase
 *        used by integration / driver layers in place of HAL_GetTick().
 *
 * Backed by HAL_GetTick() (SysTick interrupt), so the tick is valid as soon
 * as HAL_Init() has run — i.e. before the RTOS scheduler starts.  Use this
 * (not osal_get_tick_*) for any timeout calculation that may execute in a
 * pre-scheduler init path (panel reset, sensor power-on sequencing, etc.).
 *
 * @version V1.0 2026-04-26
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __SYSTICK_PORT_H__
#define __SYSTICK_PORT_H__

//******************************** Includes *********************************//
#include <stdint.h>
//******************************** Includes *********************************//

//******************************* Declaring *********************************//

/**
 * @brief  Read the current monotonic millisecond tick.
 *
 * @return Current tick value in milliseconds (rolls over every ~49.7 days).
 */
uint32_t core_systick_get_ms(void);

/**
 * @brief  Compute elapsed ms between a captured start tick and now.
 *         Handles 32-bit wrap-around correctly via modular subtraction.
 *
 * @param[in] start_ms : Tick previously captured via core_systick_get_ms.
 *
 * @return Elapsed milliseconds since start_ms.
 */
uint32_t core_systick_elapsed_ms(uint32_t start_ms);

//******************************* Declaring *********************************//

#endif /* __SYSTICK_PORT_H__ */
