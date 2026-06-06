/******************************************************************************
 * @file upgrade_service.c
 *
 * @par dependencies
 * - upgrade_service.h
 * - iflash_port.h (MCU port — erase/program internal Flash)
 * - cfg_ota.h
 * - string.h
 *
 * @author Ethan-Hang
 *
 * @brief Read/write the OTA flag struct stored at internal Flash Sector 2.
 *
 *        The struct is the hand-off between APP (writes "image staged") and
 *        Bootloader (reads it, runs the apply state machine on the next
 *        boot). All MCU-specific Flash plumbing — HAL unlock/erase/program
 *        and the critical-section that protects the single-bank Flash from
 *        an in-flight ISR — lives in 02_MCU_Platform/MCU_Core_IFlash/. This
 *        file holds only the OTA-flag semantics on top.
 *
 * @version V1.1 2026-05-17
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include <string.h>
#include "platform_type.h"

#include "upgrade_service.h"

#include "iflash_port.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
platform_err_t ota_flag_read(ota_flag_t *out)
{
    if (out == NULL)
    {
        return PLATFORM_ERR_PARAM;
    }

    /**
    * Internal Flash is memory-mapped; plain memcpy is safe and cheap. No
    * unlock or status polling needed for reads.
    **/
    memcpy(out, (const void *)CFG_OTA_FLAG_ADDRESS, sizeof(*out));

    /* Blank/garbage magic means the flag was never written — surface it as
     * "not initialised" so the caller can seed a fresh struct. */
    if (out->magic != CFG_OTA_FLAG_MAGIC)
    {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }
    return PLATFORM_OK;
}

platform_err_t ota_flag_write(const ota_flag_t *in)
{
    if (in == NULL)
    {
        return PLATFORM_ERR_PARAM;
    }

    /**
    * Sector-2 holds the OTA flag (see cfg_ota.h). Both erase and program
    * go through the MCU IFlash port, which owns the __disable_irq() critical
    * section and the read-back verify. Keep semantics out of this file —
    * propagate the port's platform_err_t straight through.
    **/
    platform_err_t er = mcu_iflash_erase_sector(CFG_OTA_FLAG_SECTOR);
    if (PLATFORM_IS_ERR(er))
    {
        return er;
    }

    const UINT32_T n_words = sizeof(*in) / sizeof(UINT32_T);

    return mcu_iflash_program_words(CFG_OTA_FLAG_ADDRESS,
                                    (const UINT32_T *)in,
                                    n_words);
}
//******************************* Functions *********************************//
