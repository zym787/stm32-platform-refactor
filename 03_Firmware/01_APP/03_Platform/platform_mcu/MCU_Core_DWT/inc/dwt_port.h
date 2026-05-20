/******************************************************************************
 * @file dwt_port.h
 *
 * @par dependencies
 * - stm32f4xx.h (CoreDebug / DWT register definitions, SystemCoreClock)
 *
 * @author Ethan-Hang
 *
 * @brief MCU-port DWT cycle-counter abstraction — microsecond-precision
 *        busy-wait delay for paths where SysTick (1 ms) is too coarse.
 *
 * Backed by the Cortex-M4 DWT (Data Watchpoint and Trace) cycle counter
 * running at SystemCoreClock.  Use this for tight bit-bang timing (soft I2C
 * setup/hold, sensor reset pulse widths, etc.) — not for sleeps the RTOS
 * could service via osal_task_delay.
 *
 * Init contract:
 *   core_dwt_init() must be called once before the first delay.  The delay
 *   function will lazy-init if the cycle counter is not yet enabled, so a
 *   missed init only costs a one-time bit-mask check, not a hang.
 *
 * @version V1.0 2026-04-26
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __DWT_PORT_H__
#define __DWT_PORT_H__

//******************************** Includes *********************************//
#include <stdint.h>
//******************************** Includes *********************************//

//******************************* Declaring *********************************//

/**
 * @brief  Enable the DWT trace block and start the free-running cycle
 *         counter.  Idempotent — safe to call more than once.
 */
void core_dwt_init(void);

/**
 * @brief  Busy-wait for the requested number of microseconds.
 *         Resolution and accuracy are bounded by 1 / SystemCoreClock
 *         (10 ns at 100 MHz on STM32F411).
 *
 *         For us values large enough that cycles overflow uint32_t
 *         (us > ~42 s at 100 MHz), the wait will be truncated — this
 *         function is intended for short pulses, not long sleeps.
 *
 * @param[in] us : Microseconds to wait.  us == 0 returns immediately.
 */
void core_dwt_delay_us(uint32_t us);

//******************************* Declaring *********************************//

#endif /* __DWT_PORT_H__ */
