/******************************************************************************
 * @file osal_config.h
 *
 * @par dependencies
 * - osal_common_types.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL compile-time configuration definitions.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once 
#ifndef __OSAL_CONFIG_H__
#define __OSAL_CONFIG_H__
//******************************** Includes *********************************//
#include "osal_common_types.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
 * @brief Enable FreeRTOS as the RTOS backend.
 */
#define FREERTOS_SUPPORT (1)

/**
 * @brief Select the active RTOS backend used by OSAL.
 */
#define OSAL_RTOS_SUPPORT (FREERTOS_SUPPORT)

/**
 * @brief Platform ISR context probe for Cortex-M targets.
 *
 * Host-side unit tests and non-ARM targets fall back to 0U.
 */
#if defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) ||                \
	defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_BASE__) ||          \
	defined(__ARM_ARCH_8M_MAIN__) || defined(__ARM_ARCH_8_1M_MAIN__)
#include "cmsis_gcc.h"
#define OSAL_PLATFORM_IS_IN_ISR() (__get_IPSR() != 0U)
#else
#define OSAL_PLATFORM_IS_IN_ISR() (0U)
#endif

//******************************** Defines **********************************//

#endif // __OSAL_CONFIG_H__
