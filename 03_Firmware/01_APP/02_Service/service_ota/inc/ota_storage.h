/******************************************************************************
 * @file ota_storage.h
 *
 * @par dependencies
 * - stdint.h
 *
 * @author Ethan-Hang
 *
 * @brief Platform-agnostic OTA staging storage interface.
 *
 *        Abstracts away the concrete medium the OTA image is staged on
 *        (external SPI NOR, eMMC, internal flash A/B, ...). service_ota
 *        and the consumer task include ONLY this header — they never
 *        know about W25Q64 / SFUD / specific sector sizes.
 *
 *        Binding is compile-time: exactly one .c file in 01_APP/
 *        Service_Adapters/ implements every function declared here.
 *
 * @version V1.0 2026-05-18
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __OTA_STORAGE_H__
#define __OTA_STORAGE_H__

//******************************** Includes *********************************//
#include "platform_type.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
typedef enum
{
    OTA_STORAGE_OK  =  0,
    OTA_STORAGE_ERR = -1,
} ota_storage_status_t;
//******************************** Defines **********************************//

//******************************* Functions *********************************//
/**
* @brief One-shot storage init. Called from user_init before any OTA
*        task is spawned. Allocates any OS objects the adapter needs and
*        verifies the underlying medium is reachable.
*
* @return OTA_STORAGE_OK on success.
* */
ota_storage_status_t ota_storage_init(void);

/**
* @brief Sector size of the staging medium in bytes — drives the
*        consumer's RAM coalescing buffer. The adapter MUST guarantee
*        that ota_storage_write() with @p len == this size + an offset
*        aligned to this size is atomic w.r.t. preceding writes
*        (i.e. doesn't blow away neighbouring data via sector erase).
*
* @return Sector size in bytes (e.g. 4096 for W25Q64).
* */
UINT32_T ota_storage_sector_size(void);

/**
* @brief Erase + program a chunk of the staging area.
*
* @param[in]  offset : byte offset from start of OTA partition.
* @param[in]  buf    : data to write.
* @param[in]  len    : length in bytes. For maximum efficiency keep this
*                      a multiple of ota_storage_sector_size() and offset
*                      sector-aligned — the underlying driver erases
*                      whole sectors and a sub-sector write costs an
*                      extra read-modify-write cycle (or wipes the
*                      sector outright, depending on adapter).
*
* @return OTA_STORAGE_OK on success, OTA_STORAGE_ERR on any I/O failure
*         (offset out of range, erase fail, program fail, verify mismatch).
* */
ota_storage_status_t ota_storage_write(UINT32_T       offset,
                                       const UINT8_T *buf,
                                       UINT32_T       len);

/**
* @brief Read a range of bytes from the staging area.
*
* @param[in]  offset : byte offset from start of OTA partition.
* @param[out] buf    : destination buffer, at least @p len bytes.
* @param[in]  len    : bytes to read.
*
* @return OTA_STORAGE_OK on success, OTA_STORAGE_ERR otherwise.
* */
ota_storage_status_t ota_storage_read(UINT32_T  offset,
                                      UINT8_T  *buf,
                                      UINT32_T  len);
//******************************* Functions *********************************//

#endif /* __OTA_STORAGE_H__ */
