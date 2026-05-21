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
#include <stdint.h>

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

uint32_t ota_storage_sector_size(void)
{
    return W25Q64_SECTOR_SIZE;
}

ota_storage_status_t ota_storage_write(uint32_t       offset,
                                       const uint8_t *buf,
                                       uint32_t       len)
{
    /* Write_OtaData accepts non-const uint8_t * for legacy reasons; the
       blocking façade copies into a request struct before posting to the
       storage queue so the const-cast is safe. */
    ext_flash_status_t st = Write_OtaData(offset, len, (uint8_t *)buf);
    return (EXT_FLASH_OK == st) ? OTA_STORAGE_OK : OTA_STORAGE_ERR;
}

ota_storage_status_t ota_storage_read(uint32_t  offset,
                                      uint8_t  *buf,
                                      uint32_t  len)
{
    ext_flash_status_t st = Read_OtaData(offset, len, buf);
    return (EXT_FLASH_OK == st) ? OTA_STORAGE_OK : OTA_STORAGE_ERR;
}
//******************************* Functions *********************************//
