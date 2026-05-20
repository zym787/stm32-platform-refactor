/******************************************************************************
 * @file platform_type.h
 *
 * @par dependencies
 * - stdint.h
 * - stddef.h
 * - stdbool.h
 *
 * @author Ethan-Hang
 *
 * @brief Platform-layer common base types.
 *
 *        This is the unified type outlet for the four 02_*_Platform layers
 *        (OS / MCU / BSP / Middleware) and the upper APP / SERVICE layers.
 *        Upper code should include <platform_type.h> instead of pulling in
 *        toolchain headers (stdint.h / stdbool.h) directly, so that:
 *
 *          - the integer/bool vocabulary stays consistent across platforms
 *          - replacing the underlying toolchain (e.g. moving off arm-none-
 *            eabi-gcc) only touches this header
 *
 *        We deliberately *alias* stdint.h types rather than redefine them —
 *        the project already uses stdint.h everywhere, and double-typedef
 *        of int8_t / uint8_t would collide with toolchain headers.
 *
 * @version V1.0 2026-05-20
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __PLATFORM_TYPE_H__
#define __PLATFORM_TYPE_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//

/** @brief Platform float vocabulary (kept distinct from C `float`/`double`). */
typedef float    float32_t;
typedef double   float64_t;

/** @brief Platform character vocabulary. */
typedef char     char_t;
typedef uint8_t  uchar_t;

/** @brief Platform bool — alias to C99 _Bool via stdbool.h. */
typedef bool     bool_t;

//******************************** Defines **********************************//

#endif /* __PLATFORM_TYPE_H__ */
