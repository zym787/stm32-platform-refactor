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

//******************************** Defines **********************************//
typedef signed char        INT8;
typedef unsigned char      UINT8;
typedef signed short       INT16;
typedef unsigned short     UINT16;
/* 32-bit maps to `long`, not `int`: arm-none-eabi-gcc defines uint32_t as
 * `unsigned long`. Matching that underlying type keeps UINT32(_T) pointer-
 * compatible with the toolchain / CMSIS / FreeRTOS uint32_t — e.g. the
 * static-task stack buffers (StackType_t) bound via OSAL_TASK_STATIC_DEFINE.
 * `int` is also 32-bit here but is a *distinct* type and would warn. */
typedef signed long        INT32;
typedef unsigned long      UINT32;
typedef signed long long   INT64;
typedef unsigned long long UINT64;
typedef float              FLOAT32;
typedef double             FLOAT64;

//******************************** Defines **********************************//


#endif /* __BOARD_TYPES_H__ */
