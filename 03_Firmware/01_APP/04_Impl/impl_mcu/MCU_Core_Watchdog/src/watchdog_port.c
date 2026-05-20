/******************************************************************************
 * @file watchdog_port.c
 *
 * @par dependencies
 * - watchdog_port.h
 * - stm32f4xx.h (IWDG register definitions)
 *
 * @author Ethan-Hang
 *
 * @brief STM32F4 IWDG refresh implementation. Direct register access — the
 *        IWDG HAL module is disabled in stm32f4xx_hal_conf.h to save flash,
 *        and a refresh is a single store anyway.
 *
 * @version V1.0 2026-05-17
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "watchdog_port.h"

#include "stm32f4xx.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
 * @brief F4 IWDG_KR magic values (RM0383 §19.4.1).
 *          0x0000AAAA — reload the watchdog counter
 *          0x0000CCCC — enable the watchdog (one-way, owned by bootloader)
 *          0x00005555 — unlock PR / RLR / WINR for write
 */
#define WDG_KR_REFRESH  (0x0000AAAAUL)
//******************************** Defines **********************************//

//******************************* Functions *********************************//
void mcu_watchdog_refresh(void)
{
    IWDG->KR = WDG_KR_REFRESH;
}
//******************************* Functions *********************************//
