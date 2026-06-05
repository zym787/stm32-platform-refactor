/******************************************************************************
 * @file platform_def.h
 *
 * @par dependencies
 * - platform_type.h
 *
 * @author Ethan-Hang
 *
 * @brief Platform-layer common constants and utility macros.
 *
 *        This header only collects truly cross-cutting macros. Anything that
 *        is OS-specific, MCU-specific, or BSP-specific belongs in its own
 *        platform layer.
 *
 *        Note: no `platform_delay_ms` / `platform_delay_us` is declared here.
 *        Delay primitives already live in MCU_Core_Systick (busy-wait /
 *        SysTick) and OSAL (osal_task_delay). Re-exposing yet another delay
 *        symbol here would force every caller to pick one of three sources.
 *
 * @version V1.1 2026-6-5
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __PLATFORM_DEF_H__
#define __PLATFORM_DEF_H__

//******************************** Includes *********************************//
#include "platform_type.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

#ifndef NULL
#define NULL                    ((void *)0)
#endif

#define PLATFORM_TRUE           ((BOOL_T)true)
#define PLATFORM_FALSE          ((BOOL_T)false)

/** @brief Default alignment unit on the Cortex-M4 ABI (word). */
#define PLATFORM_ALIGN_SIZE     (4U)

/** @brief Round @p n up to the nearest multiple of PLATFORM_ALIGN_SIZE. */
#define PLATFORM_ALIGN(n)       (((n) + PLATFORM_ALIGN_SIZE - 1U) & ~(PLATFORM_ALIGN_SIZE - 1U))

/** @brief Round @p n up to the nearest multiple of @p a (must be power of 2). */
#define PLATFORM_ALIGN_TO(n, a) (((n) + ((a) - 1U)) & ~((a) - 1U))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)         (sizeof(arr) / sizeof((arr)[0]))
#endif

/** @brief Mark intentionally unused parameter / variable. */
#ifndef PLATFORM_UNUSED
#define PLATFORM_UNUSED(x)      ((void)(x))
#endif

/** @brief Min / Max — protected against double evaluation. */
#ifndef PLATFORM_MIN
#define PLATFORM_MIN(a, b)      (((a) < (b)) ? (a) : (b))
#endif

#ifndef PLATFORM_MAX
#define PLATFORM_MAX(a, b)      (((a) > (b)) ? (a) : (b))
#endif

//******************************** Defines **********************************//

#endif /* __PLATFORM_DEF_H__ */
