/******************************************************************************
 * @file upgrade_service.h
 *
 * @par dependencies
 * - stdint.h
 * - cfg_ota.h
 *
 * @author Ethan-Hang
 *
 * @brief APP-side OTA flag access + post-OTA boot helpers.
 *
 *        Counterpart of Bootloader's `Tasks/Bootmanager/inc/ota_flag.h`.
 *        APP writes the flag to signal "image staged, please apply" before
 *        a soft reset; bootloader reads it on the next boot and drives
 *        OTA_StateManager. After the bootloader-driven apply jumps into the
 *        new APP, this module also clears the CHECK_START state inside the
 *        IWDG window to confirm the new image is alive.
 *
 * @version V1.0 2026-05-14
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __UPGRADE_SERVICE_H__
#define __UPGRADE_SERVICE_H__

//******************************** Includes *********************************//
#include <stdint.h>

#include "cfg_ota.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
* @brief Read the OTA flag struct from internal Flash.
*
* @param[out] out : caller-supplied buffer; populated on success.
*
* @return  0  success (magic valid)
*         -1  invalid magic (caller should treat as uninitialised)
* */
int8_t ota_flag_read(ota_flag_t *out);

/**
* @brief Erase Sector 2 and program a fresh OTA flag struct.
*
* @param[in] in : struct to program; caller fills magic = CFG_OTA_FLAG_MAGIC.
*
* @return  0 success, -1 erase/program failure.
*
* @warning Disables global interrupts for the duration (~1 s on F411 single
*          bank). All ISR-driven activity (SysTick / UART RX / DMA / SPI)
*          pauses while this runs. Call from a low-priority context, never
*          from an ISR.
* */
int8_t ota_flag_write(const ota_flag_t *in);

/**
* @brief Refresh the IWDG counter (raw register write, no HAL module needed).
*
* @param[in]  : None.
* @param[out] : None.
* @return     : None.
*
* @note STM32F411 IWDG cannot be disabled once started; once the bootloader
*       has enabled it via ota_watchdog_start, the APP MUST refresh it on
*       a regular cadence (≤ reload window) forever, regardless of whether
*       the current boot is a post-OTA verification or a normal boot path.
* */
void ota_iwdg_refresh(void);
//******************************* Functions *********************************//

#endif /* __UPGRADE_SERVICE_H__ */
