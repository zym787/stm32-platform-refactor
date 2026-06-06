/******************************************************************************
 * @file service_storage_facade.h
 *
 * @par dependencies
 * - osal_common_types.h
 *
 * @author Ethan-Hang
 *
 * @brief Service-layer storage manager: blocking read/write helpers backed
 *        by the externflash wrapper, plus the storage manager task entry
 *        and the first-boot LVGL asset bootstrap declaration.
 *
 *        Lives under 01_SERVICE/service_storage/ so both 01_APP (LVGL
 *        sub-region reads/writes) and 01_SERVICE/service_ota (Ymodem
 *        staging into the OTA sub-region) can call into the same blocking
 *        facade over the async BSP externflash wrapper.
 *
 * Architecture:
 *
 *   Caller (e.g. lvgl_display_task)
 *       Read_LvglData(addr,size,buf)       <- blocking
 *           1. take s_caller_mutex
 *           2. publish (addr,size,buf,op) into s_pending
 *           3. set EVENT_LVGL_R / EVENT_LVGL_W in event group
 *           4. take s_done_sem  (blocks)
 *           5. release s_caller_mutex
 *
 *   storage_manager_task
 *       wait event-group bits
 *           dispatch via externflash_drv_read / write (async + callback)
 *           continue waiting; never blocks on completion itself
 *
 *   Adapter callback (handler-thread context)
 *           record status, give s_done_sem to wake caller
 *
 * @version V1.0 2026-05-08
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __USER_EXTERNFLASH_MANAGE_H__
#define __USER_EXTERNFLASH_MANAGE_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"
#include "platform_type.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* ── W25Q64 byte-offset memory map (mirrors the reference firmware) ────── *
 * The CALIB sub-region is carved out of what used to be the front of the
 * FLASHDB reserved region.  4 KB is the W25Q64 erase granularity, which is
 * also the smallest unit we can rewrite — so one sector is the natural fit
 * for the ~40-byte touch calibration record (rest stays 0xFF).  FLASHDB has
 * no consumer yet; if it gains one it must declare its base at the post-
 * CALIB address below. */
#define MEMORY_OTA_START_ADDRESS         (0x000000UL)
#define MEMORY_OTA_END_ADDRESS           (0x0FFFFFUL)
#define MEMORY_OTA_REGION_SIZE           (MEMORY_OTA_END_ADDRESS - \
                                          MEMORY_OTA_START_ADDRESS + 1U)
#define MEMORY_CALIB_START_ADDRESS       (0x100000UL)
#define MEMORY_CALIB_END_ADDRESS         (0x100FFFUL)
#define MEMORY_CALIB_REGION_SIZE         (MEMORY_CALIB_END_ADDRESS - \
                                          MEMORY_CALIB_START_ADDRESS + 1U)
#define MEMORY_FLASHDB_START_ADDRESS     (0x101000UL)
#define MEMORY_FLASHDB_END_ADDRESS       (0x1FFFFFUL)
#define MEMORY_FATFS_START_ADDRESS       (0x200000UL)
#define MEMORY_FATFS_END_ADDRESS         (0x2FFFFFUL)
#define MEMORY_LVGL_START_ADDRESS        (0x300000UL)
#define MEMORY_LVGL_END_ADDRESS          (0x5FFFFFUL)
#define MEMORY_LVGL_REGION_SIZE          (MEMORY_LVGL_END_ADDRESS - \
                                          MEMORY_LVGL_START_ADDRESS + 1U)

/* ── Event-group bits used by the storage manager task ─────────────────── */
#define EVENT_OTA          (1UL << 0)
#define EVENT_FLASHDB      (1UL << 1)
#define EVENT_FATFS        (1UL << 2)
#define EVENT_LVGL_R       (1UL << 3)
#define EVENT_LVGL_W       (1UL << 4)
#define EVENT_CALIB_R      (1UL << 5)
#define EVENT_CALIB_W      (1UL << 6)
//******************************** Defines **********************************//

//******************************** Typedefs *********************************//
typedef enum
{
    EXT_FLASH_OK             = 0,
    EXT_FLASH_ERROR          = 1,
    EXT_FLASH_ERRORTIMEOUT   = 2,
    EXT_FLASH_ERRORRESOURCE  = 3,
    EXT_FLASH_ERRORPARAMETER = 4,
    EXT_FLASH_ERRORNOMEMORY  = 5,
} ext_flash_status_t;
//******************************** Typedefs *********************************//

//******************************* Declaring *********************************//
/**
 * @brief Initialise OS resources (event group + binary semaphore + caller
 *        mutex) used by Read_LvglData / Write_LvglData.
 *
 *        MUST be called from user_init_task_function() BEFORE any of the
 *        application tasks are created so that storage_manager_task and the
 *        first caller find the resources already in place.
 *
 * @return EXT_FLASH_OK on success, EXT_FLASH_ERRORRESOURCE on failure.
 */
ext_flash_status_t storage_manager_resources_init(void);

/**
 * @brief Blocking read from the LVGL sub-region of the external flash.
 *
 * @param[in]  addr     Byte offset INSIDE the LVGL sub-region.
 * @param[in]  size     Number of bytes to read.
 * @param[out] out_buf  Destination buffer (caller-owned, >= @p size bytes).
 *
 * @return EXT_FLASH_OK on success, error code otherwise.
 */
ext_flash_status_t Read_LvglData(UINT32_T  addr,
                                 UINT32_T  size,
                                 UINT8_T  *out_buf);

/**
 * @brief Blocking write (with sector auto-erase by the underlying handler)
 *        into the LVGL sub-region.
 *
 * @param[in] addr    Byte offset INSIDE the LVGL sub-region.
 * @param[in] size    Number of bytes to write.
 * @param[in] in_buf  Source buffer.  Must remain valid until the call
 *                    returns -- this API is blocking so caller stack/static
 *                    data is fine.
 *
 * @return EXT_FLASH_OK on success, error code otherwise.
 */
ext_flash_status_t Write_LvglData(UINT32_T        addr,
                                  UINT32_T        size,
                                  const UINT8_T  *in_buf);

/**
 * @brief Blocking read from the OTA sub-region of the external flash.
 *
 * @param[in]  addr     Byte offset INSIDE the OTA sub-region (0 .. OTA_REGION_SIZE - 1).
 * @param[in]  size     Number of bytes to read.
 * @param[out] out_buf  Destination buffer.
 *
 * @return EXT_FLASH_OK on success, error code otherwise.
 */
ext_flash_status_t Read_OtaData(UINT32_T  addr,
                                UINT32_T  size,
                                UINT8_T  *out_buf);

/**
 * @brief Blocking write (with sector auto-erase) into the OTA sub-region.
 *
 * @param[in] addr    Byte offset INSIDE the OTA sub-region.
 * @param[in] size    Number of bytes to write.
 * @param[in] in_buf  Source buffer.
 *
 * @return EXT_FLASH_OK on success, error code otherwise.
 */
ext_flash_status_t Write_OtaData(UINT32_T        addr,
                                 UINT32_T        size,
                                 const UINT8_T  *in_buf);

/**
 * @brief Blocking read from the calibration sub-region of the external flash.
 *
 *        The calibration sub-region is a single 4-KB sector dedicated to the
 *        touch panel's saved affine coefficients (see cfg_touch.h for the
 *        in-region layout).  Callers typically only need the first ~40 bytes,
 *        but the underlying erase granularity means any write rewrites the
 *        whole sector.
 *
 * @param[in]  addr     Byte offset INSIDE the calibration sub-region
 *                      (0 .. CALIB_REGION_SIZE - 1).
 * @param[in]  size     Number of bytes to read.
 * @param[out] out_buf  Destination buffer.
 *
 * @return EXT_FLASH_OK on success, error code otherwise.
 */
ext_flash_status_t Read_CalibData (UINT32_T  addr,
                                   UINT32_T  size,
                                   UINT8_T  *out_buf);

/**
 * @brief Blocking write (with sector auto-erase) into the calibration sub-region.
 *
 *        Each write erases the full 4-KB sector before programming, so the
 *        recommended usage pattern is "stage the whole sector buffer, then
 *        write once" rather than multiple sub-sector writes.
 *
 * @param[in] addr    Byte offset INSIDE the calibration sub-region.
 * @param[in] size    Number of bytes to write.
 * @param[in] in_buf  Source buffer.
 *
 * @return EXT_FLASH_OK on success, error code otherwise.
 */
ext_flash_status_t Write_CalibData(UINT32_T        addr,
                                   UINT32_T        size,
                                   const UINT8_T  *in_buf);

/**
 * @brief Storage manager task entry.  Drains the event group and dispatches
 *        each request to the externflash wrapper (async + callback).
 */
void storage_manager_task(void *argument);

/**
 * @brief First-boot bootstrap: if the LVGL sub-region's magic does not
 *        match CFG_LVGL_ASSET_MAGIC, erase the asset footprint and write
 *        the three pointer-sprite images, then write the magic.  Always
 *        re-reads the three sprites into RAM so that the LVGL needle
 *        descriptors point at valid data.
 *
 *        Idempotent: a magic-match call returns immediately after the
 *        re-read.  Must be invoked AFTER storage_manager_task and the W25Q64
 *        flash_handler_thread are running -- it relies on the synchronous
 *        Read_/Write_LvglData APIs.
 *
 *        Recommended call site: lvgl_display_task, immediately before
 *        setup_ui().
 *
 * @return EXT_FLASH_OK on success, error code otherwise.
 */
ext_flash_status_t storage_assets_bootstrap(void);
//******************************* Declaring *********************************//

#endif /* __USER_EXTERNFLASH_MANAGE_H__ */
