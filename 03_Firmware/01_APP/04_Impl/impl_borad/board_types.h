/******************************************************************************
 * @file board_types.h
 *
 * @par dependencies
 * - (none — concrete toolchain mapping, no headers)
 *
 * @author Ethan-Hang
 *
 * @brief Board/toolchain concrete fixed-width base types.
 *
 *        Lowest-level type layer: maps the project's fixed-width vocabulary
 *        (UINT8 / INT32 / FLOAT64 ...) onto this board+toolchain's built-in
 *        C types. This is the ONE file to edit when porting to another
 *        toolchain (IAR / arm-clang) or a board with different widths.
 *
 *        03_Platform/platform_common/platform_type.h layers the project's
 *        `_T` naming (UINT8_T ...) on top of these names; upper code uses
 *        platform_type.h, not this header directly.
 *
 * @version V1.0 2026-6-5
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BOARD_TYPES_H__
#define __BOARD_TYPES_H__

//******************************** Includes *********************************//
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

//******************************** Includes *********************************//


//******************************** Defines **********************************//
typedef signed char    INT8_t;
typedef unsigned char  UINT8_t;
typedef signed short   INT16_t;
typedef unsigned short UINT16_t;
/* 32-bit maps to `long`, not `int`: arm-none-eabi-gcc defines uint32_t as
 * `unsigned long`. Matching that underlying type keeps UINT32(_T) pointer-
 *-compatible with the toolchain / CMSIS / FreeRTOS uint32_t — e.g. the
 * static-task stack buffers (StackType_t) bound via OSAL_TASK_STATIC_DEFINE.
 * `int` is also 32-bit here but is a *distinct* type and would warn. */
typedef signed long        INT32_t;
typedef unsigned long      UINT32_t;
typedef signed long long   INT64_t;
typedef unsigned long long UINT64_t;
typedef float              FLOAT;
typedef double             DOUBLE;

typedef size_t SIZE_t;

typedef bool BOOL;

typedef uintptr_t UINTPTR_t;
//******************************** Defines **********************************//


#endif /* __BOARD_TYPES_H__ */
