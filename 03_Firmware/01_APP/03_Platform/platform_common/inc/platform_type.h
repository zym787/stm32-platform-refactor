/******************************************************************************
 * @file platform_type.h
 *
 * @par dependencies
 * - board_types.h  (concrete fixed-width source — impl_borad)
 * - stdint.h
 * - stddef.h
 * - stdbool.h
 *
 * @author Ethan-Hang
 *
 * @brief Platform-layer unified base-type vocabulary.
 *
 *        This is the single type outlet for the four 03_Platform layers
 *        (OS / MCU / BSP / Middleware) and the upper APP / SERVICE layers.
 *        Upper code should include "platform_type.h" instead of pulling in
 *        toolchain headers (stdint.h / stddef.h / stdbool.h) directly, so:
 *
 *          - the integer / bool vocabulary stays consistent across platforms
 *          - swapping the toolchain / board only touches board_types.h
 *
 *        Type source: the fixed-width vocabulary is sourced from the board
 *        layer's concrete types (impl_borad/board_types.h: UINT8 / UINT32 /
 *        FLOAT32 ...), NOT from stdint. board_types.h is where the actual
 *        toolchain mapping lives; this header only layers the project's
 *        `_T` naming on top of it.
 *
 *        Naming exception: the fixed-width type names are UPPER_CASE with a
 *        `_T` suffix (UINT8_T, INT32_T, ...), NOT the project default
 *        PascalCase-for-typedef. This is intentional and project-mandated:
 *
 *          - the `_T` names do NOT collide with the lowercase stdint names
 *            (uint8_t, ...) nor with board_types' un-suffixed names (UINT8,
 *            ...), so the three coexist without re-typedef clashes.
 *          - stdint stays included so lowercase uint8_t / size_t remain
 *            available to code that has not migrated to the `_T` vocab yet,
 *            and to provide uintptr_t for UINTPTR_T below.
 *
 * @version V1.2 2026-6-5
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __PLATFORM_TYPE_H__
#define __PLATFORM_TYPE_H__

//******************************** Includes *********************************//
#include "board_types.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//

/** @brief Fixed-width signed integers (sourced from board_types.h). */
typedef INT8      INT8_T;
typedef INT16     INT16_T;
typedef INT32     INT32_T;
typedef INT64     INT64_T;

/** @brief Fixed-width unsigned integers (sourced from board_types.h). */
typedef UINT8     UINT8_T;
typedef UINT16    UINT16_T;
typedef UINT32    UINT32_T;
typedef UINT64    UINT64_T;

/** @brief Floating point (sourced from board_types.h). */
typedef FLOAT32   FLOAT32_T;
typedef FLOAT64   FLOAT64_T;

/** @brief Boolean — alias to C99 _Bool via stdbool.h. */
typedef bool      BOOL_T;

/** @brief Character vocabulary. */
typedef char      CHAR_T;
typedef uint8_t   UCHAR_T;

/**
 * @brief Size and pointer-sized integer vocabulary.
 *
 *        Use SIZE_T for object sizes / counts / indices and UINTPTR_T for
 *        pointer arithmetic, mirroring the coding-standard guidance to keep
 *        sizes in size_t and never to truncate pointers through a 32-bit int.
 */
typedef size_t    SIZE_T;
typedef uintptr_t UINTPTR_T;

//******************************** Defines **********************************//

#endif /* __PLATFORM_TYPE_H__ */
