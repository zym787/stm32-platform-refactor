/******************************************************************************
 * @file dwt_port.c
 *
 * @par dependencies
 * - dwt_port.h
 * - stm32f4xx.h (DWT / CoreDebug, SystemCoreClock)
 *
 * @author Ethan-Hang
 *
 * @brief Cortex-M4 DWT cycle-counter implementation of the MCU-port us delay.
 *        Keeps Cortex CMSIS register touches contained inside the MCU port
 *        layer — porting to a non-Cortex MCU only requires replacing this
 *        file.
 *
 * @version V1.0 2026-04-26
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "dwt_port.h"
#include "stm32f4xx.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//

void core_dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT       = 0U;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
}

void core_dwt_delay_us(uint32_t us)
{
    if (0U == us)
    {
        return;
    }

    /* Lazy-init: if the cycle counter is disabled, enable it now. */
    if (0U == (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk))
    {
        core_dwt_init();
    }

    uint32_t const start  = DWT->CYCCNT;
    uint32_t const cycles = us * (SystemCoreClock / 1000000U);

    while ((uint32_t)(DWT->CYCCNT - start) < cycles)
    {
        /* spin */
    }
}

//******************************* Functions *********************************//
