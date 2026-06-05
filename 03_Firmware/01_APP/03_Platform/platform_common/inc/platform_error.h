/******************************************************************************
 * @file platform_error.h
 *
 * @par dependencies
 * - platform_type.h
 *
 * @author Ethan-Hang
 *
 * @brief Platform-layer unified error code vocabulary.
 *
 *        `platform_err_t` is the *new* return-code vocabulary used by code
 *        written from this point forward (middleware abstraction, lvgl HAL,
 *        future service layers, etc.).
 *
 *        It coexists with the existing OSAL_* error codes (osal_error.h).
 *        We do NOT mass-migrate OSAL_* — they are stable and the call sites
 *        already compare to OSAL_SUCCESS correctly. The new vocabulary is
 *        introduced so future code stops conflating "boolean false" with
 *        "successful return", which bit us once in Ymodem (see memory:
 *        osal-success-vs-false-collision).
 *
 *        Numeric values are kept compatible with osal_error.h (PLATFORM_OK
 *        == OSAL_SUCCESS == 0) so a service layer can transparently
 *        propagate a lower-layer OSAL return as a platform_err_t.
 *
 * @version V1.1 2026-6-5
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __PLATFORM_ERROR_H__
#define __PLATFORM_ERROR_H__

//******************************** Includes *********************************//
#include "platform_type.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

/**
 * @brief Unified platform return code.
 *
 *        Only PLATFORM_OK means success — every other value is an error.
 *        Use PLATFORM_IS_OK() / PLATFORM_IS_ERR() to test return values;
 *        never compare against a "false-like" sentinel.
 *
 *        Keep this as an enum (not a #define) so a debugger shows the
 *        symbolic name instead of a raw integer.
 */
typedef enum
{
    PLATFORM_OK                   = 0,    /**< Success */
    PLATFORM_ERR_GENERAL          = 1,    /**< Generic failure */
    PLATFORM_ERR_PARAM            = 2,    /**< Invalid argument */
    PLATFORM_ERR_NULL_PTR         = 3,    /**< NULL pointer argument */
    PLATFORM_ERR_TIMEOUT          = 4,    /**< Operation timed out */
    PLATFORM_ERR_BUSY             = 5,    /**< Resource busy */
    PLATFORM_ERR_NO_MEMORY        = 6,    /**< Out of memory */
    PLATFORM_ERR_NO_RESOURCE      = 7,    /**< No free instance / slot */
    PLATFORM_ERR_NOT_SUPPORTED    = 8,    /**< Operation not supported */
    PLATFORM_ERR_NOT_INITIALIZED  = 9,    /**< Subsystem not initialized */
    PLATFORM_ERR_ALREADY_INIT     = 10,   /**< Already initialized */
    PLATFORM_ERR_IN_ISR           = 11,   /**< Disallowed in ISR context */
    PLATFORM_ERR_HW_FAULT         = 12,   /**< Underlying hardware fault */
    PLATFORM_ERR_CRC              = 13,   /**< Data integrity check failed */
    PLATFORM_ERR_OVERFLOW         = 14,   /**< Buffer/queue overflow */
    PLATFORM_ERR_UNDERFLOW        = 15,   /**< Buffer/queue underflow */
    PLATFORM_ERR_RESERVED         = 0x7FFFFFFF /**< Force enum to 32-bit */
} platform_err_t;

/** @brief True when `err` indicates success. */
#define PLATFORM_IS_OK(err)     ((err) == PLATFORM_OK)

/** @brief True when `err` indicates any kind of failure. */
#define PLATFORM_IS_ERR(err)    ((err) != PLATFORM_OK)

//******************************** Defines **********************************//

#endif /* __PLATFORM_ERROR_H__ */
