/******************************************************************************
 * @file osal_internal_globaldefs.h
 *
 * @par dependencies
 * - osal_common_types.h
 * - osal_error.h
 * - osal_macros.h
 *
 * @author Ethan-Hang
 *
 * @brief OSAL internal global definitions.
 *
 * @version V1.0 2026-4-9
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __OSAL_INTERNAL_GLOBALDEFS_H__
#define __OSAL_INTERNAL_GLOBALDEFS_H__

//******************************** Includes *********************************//
#include "osal_common_types.h"
#include "osal_error.h"
#include "osal_macros.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#ifndef OSAL_WAIT_FOREVER
#define OSAL_WAIT_FOREVER OSAL_MAX_DELAY
#endif

#ifndef OSAL_UNUSED
#define OSAL_UNUSED(x) ((void)(x))
#endif

#define OSAL_CHECK_POINTER(ptr) ARGCHECK((ptr) != NULL, OSAL_INVALID_POINTER)

#define OSAL_CHECK_SIZE(val)                                                  \
    ARGCHECK((val) > 0 && (val) < (UINT32_MAX / 2), OSAL_ERR_INVALID_SIZE)

#define OSAL_CHECK_STRING(str, maxlen, errcode)                               \
    do                                                                        \
    {                                                                         \
        OSAL_CHECK_POINTER(str);                                              \
        LENGTHCHECK(str, maxlen, errcode);                                    \
    } while (0)
//******************************** Defines **********************************//

#endif /* __OSAL_INTERNAL_GLOBALDEFS_H__ */
