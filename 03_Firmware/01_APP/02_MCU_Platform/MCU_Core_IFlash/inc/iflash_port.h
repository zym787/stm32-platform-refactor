/******************************************************************************
 * @file iflash_port.h
 *
 * @par dependencies
 * - stdint.h
 * - stdbool.h
 *
 * @author Ethan-Hang
 *
 * @brief MCU-port internal-Flash abstraction — sector erase + word program.
 *
 *        STM32F411 has a single Flash bank: any erase or program operation
 *        stalls instruction fetch from Flash for ~1 s while busy. If any
 *        interrupt fires during that window the CPU will try to fetch ISR
 *        code from Flash and hard-lock. Both functions therefore wrap the
 *        full unlock → operation → lock sequence in a `__disable_irq()`
 *        critical section; callers never need to mask interrupts themselves.
 *
 *        Reads are intentionally NOT exposed — internal Flash is
 *        memory-mapped, so callers use plain pointer dereference / memcpy
 *        against the physical address.
 *
 *        Porting to another MCU only requires reimplementing the source
 *        file; sector identifiers and word size are reasonable lowest common
 *        denominator across STM32 / NXP / Renesas embedded Flash.
 *
 * @par BATCH USAGE NOTE
 *        Each call masks IRQs for ~1 s and SysTick stops advancing for the
 *        same window — FreeRTOS tick effectively pauses, software timers
 *        drift, osal_task_delay durations stretch. Single-shot use (current
 *        ota_flag write path: one sector at a time) is fine; back-to-back
 *        erases compound the tick loss. If you ever add a multi-sector
 *        operation (e.g. KVS / FlashDB), call this once per sector with at
 *        least one osal_task_delay(0) between calls so the scheduler can
 *        resync the tick base and the IWDG feeder can run.
 *
 * @version V1.0 2026-05-17
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __IFLASH_PORT_H__
#define __IFLASH_PORT_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdbool.h>
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
/**
* @brief Erase a single internal-Flash sector.
*
* @param[in]  sector : F4 sector index (0 .. 7 on F411CE; sectors 0,1 hold
*                      the bootloader, 2 is the OTA flag, 3..7 are the APP).
*
* @return  0  success
*         -1  HAL_FLASH_Unlock / HAL_FLASHEx_Erase failure
*
* @warning Disables global interrupts for the duration (~1 s on F411). All
*          ISR-driven activity (SysTick / UART RX / DMA / SPI) pauses while
*          this runs. Call from a low-priority task, never from an ISR.
*          Caller is responsible for not erasing the sector their own
*          execution lives in.
* */
int8_t mcu_iflash_erase_sector(uint8_t sector);

/**
* @brief Program @p n_words 32-bit words into internal Flash starting at
*        @p addr. Target region must already be erased (0xFFFFFFFF).
*
* @param[in]  addr    : Absolute Flash address; must be 4-byte aligned.
* @param[in]  src     : Source word array; must be non-NULL.
* @param[in]  n_words : Number of 32-bit words to program.
*
* @return  0  success and read-back verified
*         -1  alignment / NULL violation, program failure, or read-back
*             mismatch
*
* @warning Same interrupt-mask semantics as mcu_iflash_erase_sector().
* */
int8_t mcu_iflash_program_words(uint32_t        addr,
                                const uint32_t *src,
                                uint32_t        n_words);
//******************************* Declaring *********************************//

#endif /* __IFLASH_PORT_H__ */
