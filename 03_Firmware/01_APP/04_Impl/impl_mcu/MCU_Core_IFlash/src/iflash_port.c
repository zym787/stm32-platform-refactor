/******************************************************************************
 * @file iflash_port.c
 *
 * @par dependencies
 * - iflash_port.h
 * - stm32f4xx_hal.h
 *
 * @author Ethan-Hang
 *
 * @brief STM32F411 internal-Flash erase/program implementation. Wraps the
 *        HAL_FLASH unlock/erase/program/lock sequence in a __disable_irq()
 *        critical section because F411 is a single-bank part — any ISR that
 *        runs while erase/program is busy will try to fetch instructions
 *        from a stalled Flash and hard-lock the core.
 *
 * @version V1.0 2026-05-17
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "board_types.h"
#include <string.h>

#include "iflash_port.h"

#include "stm32f4xx_hal.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
platform_err_t mcu_iflash_erase_sector(UINT8_t sector)
{
    UINT32_t primask = __get_PRIMASK();
    __disable_irq();

    platform_err_t result = PLATFORM_ERR_HW_FAULT;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        goto exit;
    }

    FLASH_EraseInitTypeDef erase_init;
    erase_init.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase_init.Sector       = sector;
    erase_init.NbSectors    = 1U;

    UINT32_t sector_error = 0xFFFFFFFFU;
    if (HAL_FLASHEx_Erase(&erase_init, &sector_error) == HAL_OK)
    {
        result = PLATFORM_OK;
    }

    (void)HAL_FLASH_Lock();

exit:
    /**
    * Restore PRIMASK exactly as we found it: only re-enable if it was clear
    * coming in; otherwise leave masked (caller had a reason to mask).
    **/
    if (primask == 0U)
    {
        __enable_irq();
    }
    return result;
}

platform_err_t mcu_iflash_program_words(UINT32_t        addr,
                                const UINT32_t *src,
                                UINT32_t        n_words)
{
    if ((src == NULL) || ((addr & 0x3U) != 0U))
    {
        return PLATFORM_ERR_PARAM;
    }

    UINT32_t primask = __get_PRIMASK();
    __disable_irq();

    platform_err_t result = PLATFORM_ERR_HW_FAULT;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        goto exit;
    }

    HAL_StatusTypeDef hs = HAL_OK;
    for (UINT32_t i = 0U; i < n_words; i++)
    {
        hs = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                               addr + (i * sizeof(UINT32_t)),
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
        if (memcmp((const void *)addr,
                   (const void *)src,
                   n_words * sizeof(UINT32_t)) == 0)
        {
            result = PLATFORM_OK;
        }
    }

exit:
    if (primask == 0U)
    {
        __enable_irq();
    }
    return result;
}
//******************************* Functions *********************************//
