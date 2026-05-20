/******************************************************************************
 * @file os_freertos.h
 *
 * @par dependencies
 * - FreeRTOS.h
 *
 * @author Ethan-Hang
 *
 * @brief FreeRTOS helper macros for OSAL conversion.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OS_FREERTOS_H__
#define __OS_FREERTOS_H__

//******************************** Includes *********************************//
#include "FreeRTOS.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
 * @brief Convert time value from milliseconds to OS tick count.
 *
 * @param[in] osal_time_in_ms Time value in milliseconds.
 *
 * @return Converted tick count. If input equals portMAX_DELAY, keep it as
 *         portMAX_DELAY to preserve wait-forever semantics.
 */
#define OS_MS_TO_TICKS(osal_time_in_ms)                                       \
    ((osal_time_in_ms == portMAX_DELAY) ? (portMAX_DELAY) :                   \
    ((osal_tick_type_t)(((osal_tick_type_t)(osal_time_in_ms) *                \
    (osal_tick_type_t)configTICK_RATE_HZ) / (osal_tick_type_t)1000U)))

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

//******************************* Functions *********************************//

#endif // __OS_FREERTOS_H__
