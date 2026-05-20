/******************************************************************************
 * @file systick_port.c
 *
 * @par dependencies
 * - systick_port.h
 * - stm32f4xx_hal.h
 *
 * @author Ethan-Hang
 *
 * @brief Thin wrapper over HAL_GetTick() — keeps HAL_* contained inside the
 *        MCU port layer so integration and driver code never depends on it
 *        directly.  Porting to a different MCU only requires replacing this
 *        file.
 *
 * @version V1.0 2026-04-26
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "systick_port.h"
#include "stm32f4xx_hal.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//

uint32_t core_systick_get_ms(void)
{
    return HAL_GetTick();
}

uint32_t core_systick_elapsed_ms(uint32_t start_ms)
{
    return (uint32_t)(HAL_GetTick() - start_ms);
}

//******************************* Functions *********************************//
