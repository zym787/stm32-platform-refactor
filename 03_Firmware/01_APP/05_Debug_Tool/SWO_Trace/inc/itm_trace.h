/******************************************************************************
 * @file itm_trace.h
 *
 * @author Ethan-Hang
 *
 * @brief ITM/SWO trace initialisation and character output.
 *
 * Usage:
 *   Call itm_trace_init() once after SystemClock_Config().
 *   __io_putchar() in main.c delegates to itm_putchar() so that printf()
 *   output is routed to the SWO pin and visible in JLink SWO Viewer / Ozone.
 *
 * @version V1.0 2026-04-23
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/
#ifndef ITM_TRACE_H
#define ITM_TRACE_H

#include <stdint.h>

/**
 * @brief Initialise TPIU and ITM for SWO output.
 *
 * @param[in] cpu_hz  HCLK frequency in Hz (e.g. 100000000U for 100 MHz)
 * @param[in] swo_hz  Desired SWO bit-rate in Hz (e.g. 2000000U for 2 Mbit/s)
 */
void itm_trace_init(uint32_t cpu_hz, uint32_t swo_hz);

/**
 * @brief Write one byte to ITM stimulus port 0.
 *
 * Returns immediately without blocking when ITM is not active.
 *
 * @param[in] ch  Character to transmit
 * @return ch unchanged
 */
int itm_putchar(int ch);

/**
 * @brief Return non-zero when ITM stimulus port 0 is ready to accept data.
 *
 * Checks DEMCR.TRCENA, ITM_TCR.ITMENA, and ITM_TER port-0 enable bit.
 */
int itm_is_active(void);

#endif /* ITM_TRACE_H */
