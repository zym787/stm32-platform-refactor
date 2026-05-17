/******************************************************************************
 * @file upgrade_service.c
 *
 * @par dependencies
 * - upgrade_service.h
 * - cfg_ota.h
 * - stm32f4xx_hal.h
 * - string.h
 *
 * @author Ethan-Hang
 *
 * @brief Read/write OTA flag at internal Flash Sector 2, plus a raw IWDG
 *        refresh helper.
 *
 * @version V1.0 2026-05-14
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include <string.h>

#include "stm32f4xx_hal.h"

#include "upgrade_service.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
 * @brief Direct IWDG_KR access — IWDG HAL module is disabled in
 *        stm32f4xx_hal_conf.h so we touch the register directly. The KR
 *        magic values are fixed in the F411 reference manual:
 *          0x0000AAAA — reload counter (refresh)
 *          0x0000CCCC — enable watchdog
 *          0x00005555 — unlock PR/RLR for write
 */
#define OTA_IWDG_KR_REFRESH (0x0000AAAAUL)
//******************************** Defines **********************************//

//******************************* Functions *********************************//
int8_t ota_flag_read(ota_flag_t *out)
{
    if (out == NULL)
    {
        return -1;
    }

    /**
    * Flash is memory-mapped; plain memcpy is safe and cheap. No unlock or
    * status polling needed for reads.
    **/
    memcpy(out, (const void *)CFG_OTA_FLAG_ADDRESS, sizeof(*out));

    if (out->magic != CFG_OTA_FLAG_MAGIC)
    {
        return -1;
    }
    return 0;
}

int8_t ota_flag_write(const ota_flag_t *in)
{
    if (in == NULL)
    {
        return -1;
    }

    /**
    * F411 has a single Flash bank, so erase/program stalls all instruction
    * fetch from Flash for ~1 s. If any interrupt fires during that window
    * the CPU will try to fetch ISR code from Flash and hard-lock. Disable
    * the global irq mask for the full sequence. EasyLogger may drop a
    * second's worth of log lines — acceptable for the rare OTA path.
    **/
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    int8_t result = -1;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        goto exit;
    }

    FLASH_EraseInitTypeDef erase_init;
    erase_init.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase_init.Sector       = FLASH_SECTOR_2;
    erase_init.NbSectors    = 1U;

    uint32_t sector_error = 0xFFFFFFFFU;
    if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK)
    {
        (void)HAL_FLASH_Lock();
        goto exit;
    }

    /**
    * Program 4×32-bit words at CFG_OTA_FLAG_ADDRESS. Each write is verified
    * by HAL via the FLASH_SR EOP flag inside HAL_FLASH_Program; if any
    * single word fails we bail with the partially-written sector — caller
    * sees the read-back mismatch below and treats the struct as invalid on
    * the next boot.
    **/
    const uint32_t *src = (const uint32_t *)in;
    HAL_StatusTypeDef hs = HAL_OK;
    for (uint32_t i = 0; i < (sizeof(*in) / sizeof(uint32_t)); i++)
    {
        hs = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                               CFG_OTA_FLAG_ADDRESS + (i * sizeof(uint32_t)),
                               src[i]);
        if (hs != HAL_OK)
        {
            break;
        }
    }

    (void)HAL_FLASH_Lock();

    if (hs == HAL_OK)
    {
        /**
        * Read-back check — catches a silent program failure (write-protect
        * trip, voltage glitch) that didn't propagate as HAL_ERROR.
        **/
        ota_flag_t verify;
        memcpy(&verify, (const void *)CFG_OTA_FLAG_ADDRESS, sizeof(verify));
        if (memcmp(&verify, in, sizeof(verify)) == 0)
        {
            result = 0;
        }
    }

exit:
    /**
    * Restore PRIMASK exactly as we found it. We unconditionally clear it
    * via __enable_irq() if it was clear coming in; otherwise leave masked
    * (caller had a reason to mask). This avoids accidentally opening a
    * critical section opened above us.
    **/
    if (primask == 0U)
    {
        __enable_irq();
    }
    return result;
}

void ota_iwdg_refresh(void)
{
    IWDG->KR = OTA_IWDG_KR_REFRESH;
}
//******************************* Functions *********************************//
