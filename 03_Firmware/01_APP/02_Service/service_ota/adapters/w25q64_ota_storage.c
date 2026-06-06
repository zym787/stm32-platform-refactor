/******************************************************************************
 * @file w25q64_ota_storage.c
 *
 * @par dependencies
 * - ota_storage.h
 * - service_storage_facade.h (service_storage façade)
 *
 * @author Ethan-Hang
 *
 * @brief Board-specific implementation of ota_storage.h on top of the
 *        W25Q64 SPI NOR flash. Bridges service_ota's abstract storage
 *        contract to the existing service_storage blocking façade
 *        (Read_OtaData / Write_OtaData → storage_manager_task → BSP
 *        externflash_drv_*).
 *
 *        Replacing this file with e.g. emmc_ota_storage.c on a board with
 *        eMMC swaps the OTA staging medium with zero changes to
 *        service_ota / Ymodem source.
 *
 * @version V1.0 2026-05-18
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_type.h"

#include "service_storage_facade.h"

#include "ota_storage.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
* @brief W25Q64 sector size. Service_storage / BSP driver erase one full
*        4 KB sector per Write_OtaData call regardless of the size
*        argument, so the consumer task must coalesce sub-sector writes
*        to a 4 KB boundary before calling — see project memory
*        [[w25q64-write-erases-sector]].
*/
#define W25Q64_SECTOR_SIZE  (4096U)
//******************************** Defines **********************************//

//******************************* Functions *********************************//
ota_storage_status_t ota_storage_init(void)
{
    /* Service_storage owns the SPI bus + storage_manager_task — both are
       brought up by storage_manager_resources_init() in user_init.c. No
       per-adapter init needed here. */
    return OTA_STORAGE_OK;
}

UINT32_T ota_storage_sector_size(void)
{
    return W25Q64_SECTOR_SIZE;
}

ota_storage_status_t ota_storage_write(UINT32_T       offset,
                                       const UINT8_T *buf,
                                       UINT32_T       len)
{
    /* Validate the caller buffer before touching the medium. */
    if ((NULL == buf) || (0U == len))
    {
        return OTA_STORAGE_ERR;
    }

    /**
    * Enforce the sector-aligned / sector-multiple contract documented in
    * ota_storage.h. Each Write_OtaData call erases the whole containing
    * 4 KB sector (see W25Q64_SECTOR_SIZE note + [[w25q64-write-erases-sector]]),
    * so a sub-sector or unaligned write would silently wipe a neighbouring
    * partial sector. Rejecting it here turns that latent corruption into a
    * loud, testable error at the seam -- the OTA consumer already coalesces
    * to full sectors before calling, so this never fires on the happy path.
    **/
    if (((offset % W25Q64_SECTOR_SIZE) != 0U) ||
        ((len % W25Q64_SECTOR_SIZE) != 0U))
    {
        return OTA_STORAGE_ERR;
    }

    /* Write_OtaData accepts non-const UINT8_T * for legacy reasons; the
       blocking façade copies into a request struct before posting to the
       storage queue so the const-cast is safe. */
    ext_flash_status_t st = Write_OtaData(offset, len, (UINT8_T *)buf);
    return (EXT_FLASH_OK == st) ? OTA_STORAGE_OK : OTA_STORAGE_ERR;
}

ota_storage_status_t ota_storage_read(UINT32_T  offset,
                                      UINT8_T  *buf,
                                      UINT32_T  len)
{
    ext_flash_status_t st = Read_OtaData(offset, len, buf);
    return (EXT_FLASH_OK == st) ? OTA_STORAGE_OK : OTA_STORAGE_ERR;
}
//******************************* Functions *********************************//
