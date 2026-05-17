/******************************************************************************
 * @file cfg_ota.h
 *
 * @par dependencies
 * - stdint.h
 *
 * @author Ethan-Hang
 *
 * @brief Project-level OTA configuration shared between APP and Bootloader.
 *
 *        Owns:
 *          - ota_flag_t layout at internal Flash 0x08008000 (Sector 2, 16 KB)
 *          - State enum mirror of bootloader's EE_OTA_* values
 *          - Magic constant for valid-flag detection
 *
 *        Both projects compile independently with no shared include path,
 *        so this header is hand-mirrored in
 *        `00_Bootloader/Tasks/Bootmanager/inc/ota_flag.h`. Any change here
 *        MUST be applied to the bootloader side too — the on-flash struct
 *        layout is the cross-image binary contract.
 *
 * @version V1.0 2026-05-14
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __CFG_OTA_H__
#define __CFG_OTA_H__

//******************************** Includes *********************************//
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
 * @brief Absolute internal-Flash address of the OTA flag sector.
 *        Sector 2 (16 KB at 0x08008000) is reserved by both linker scripts;
 *        the struct lives at the very base of the sector and the rest is
 *        scratch for future flag growth.
 */
#define CFG_OTA_FLAG_ADDRESS  (0x08008000UL)

/**
 * @brief Internal-Flash sector index holding the OTA flag (F411 Sector 2).
 *        Decoupled from the HAL's FLASH_SECTOR_2 macro so cfg_ota.h does not
 *        drag stm32f4xx_hal.h into every consumer; mcu_iflash_erase_sector()
 *        takes the raw sector number.
 */
#define CFG_OTA_FLAG_SECTOR   (2U)

/**
 * @brief Magic value at the struct head. Distinguishes a real flag from a
 *        freshly-erased sector (which reads back as 0xFFFFFFFF) and from a
 *        zero-initialised BSS region.
 */
#define CFG_OTA_FLAG_MAGIC    (0xA55A5AA5UL)

/* OTA state machine values — must match bootloader's bootmanager.h. */
#define CFG_OTA_NO_APP_UPDATE     (0x00U)
#define CFG_OTA_DOWNLOADING       (0x11U)
#define CFG_OTA_DOWNLOAD_FINISHED (0x22U)
#define CFG_OTA_APP_CHECK_START   (0x33U)
#define CFG_OTA_APP_CHECKING      (0x44U)
#define CFG_OTA_INIT_NO_APP       (0xFFU)

/**
 * @brief OTA flag struct stored at CFG_OTA_FLAG_ADDRESS.
 *
 * Frozen 16-byte layout — append-only growth so old/new image pairs stay
 * forward-compatible while the contract is iterated. Each ota_flag_write
 * costs ~1 s (16 KB sector erase) so callers should coalesce updates.
 */
typedef struct 
{
    uint32_t magic;             /**< CFG_OTA_FLAG_MAGIC; mismatch ⇒ uninit */
    uint32_t state;             /**< CFG_OTA_* widened to 32 bits */
    uint32_t image_size;        /**< staged image bytes in W25Q64 BLOCK_1 */
    uint32_t current_app_size;  /**< current APP size for rollback backup */
} ota_flag_t;
//******************************** Defines **********************************//

#endif /* __CFG_OTA_H__ */
