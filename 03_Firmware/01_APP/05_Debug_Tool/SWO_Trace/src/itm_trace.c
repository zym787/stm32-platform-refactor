/******************************************************************************
 * @file itm_trace.c
 *
 * @author Ethan-Hang
 *
 * @brief ITM/SWO trace initialisation and character output.
 *
 * @version V1.0 2026-04-23
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "itm_trace.h"
#include "stm32f4xx.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//

void itm_trace_init(uint32_t cpu_hz, uint32_t swo_hz)
{
    uint32_t prescaler = (cpu_hz / swo_hz) - 1U;

    /* Enable DWT and ITM blocks via CoreDebug */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* TPIU: NRZ (UART) serial-wire output, set prescaler */
    TPI->SPPR = 0x00000002U;
    TPI->ACPR = prescaler;

    /* Unlock ITM registers */
    ITM->LAR = 0xC5ACCE55U;

    /* Enable ITM, SWOENA; assign TraceID = 1 */
    ITM->TCR = ITM_TCR_ITMENA_Msk
             | ITM_TCR_SWOENA_Msk
             | (1U << ITM_TCR_TraceBusID_Pos);

    /* Enable stimulus port 0 only */
    ITM->TER = 0x00000001U;

    /* Enable DWT cycle counter (optional; used by some trace tools) */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

int itm_is_active(void)
{
    return ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0U) &&
           ((ITM->TCR        & ITM_TCR_ITMENA_Msk)          != 0U) &&
           ((ITM->TER        & 0x00000001U)                  != 0U);
}

int itm_putchar(int ch)
{
    /**
     * Gate on CoreDebug->DHCSR.C_DEBUGEN: that bit is asserted only while a
     * debugger is physically attached via the DAP and cannot be set from
     * software, making it the one reliable runtime answer to "is anybody
     * reading the SWO stream right now?".
     *
     * The earlier comment claimed ITM_SendChar() was a silent no-op without
     * a debugger, but that was wrong: itm_trace_init() pre-arms TCR.ITMENA
     * and TER.0 in software, so the CMSIS guard passes either way and the
     * function busy-spins on `ITM->PORT[0].u32 == 0` waiting for a TPIU
     * FIFO drain that will never happen — the first printf at boot would
     * lock up the calling task (and with it the LVGL render loop) until
     * J-Link reattaches.  Skip the write outright when no host is there.
     **/
    if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) == 0U)
    {
        return ch;
    }
    return (int)ITM_SendChar((uint32_t)ch);
}

//******************************* Functions *********************************//
