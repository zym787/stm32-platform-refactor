/******************************************************************************
 * @file bsp_adapter_port_externflash.h
 *
 * @par dependencies
 * - bsp_wrapper_externflash.h
 *
 * @author Ethan-Hang
 *
 * @brief Adapter registration interface for the W25Q64 NOR flash driver.
 *        Bridges the W25Q64 handler layer to the externflash wrapper vtable.
 *        Call drv_adapter_externflash_register() once at system initialisation
 *        before any externflash_drv_* API is used.
 *
 * Processing flow:
 *   drv_adapter_externflash_register()
 *     → fills externflash_drv_t vtable with W25Q64-specific implementations
 *     → calls drv_adapter_externflash_mount() to register into the wrapper
 *
 * @version V1.0 2026-05-02
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_ADAPTER_PORT_EXTERNFLASH_H__
#define __BSP_ADAPTER_PORT_EXTERNFLASH_H__

//******************************** Includes *********************************//
#include "bsp_wrapper_externflash.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
/**
 * @brief   Register the W25Q64 driver into the externflash wrapper.
 *          Must be called once before any externflash_drv_* API is invoked.
 *
 * @return  true  - Registration successful.
 *          false - Mount failed (index out of range or NULL vtable).
 */
bool drv_adapter_externflash_register(void);
//******************************* Declaring *********************************//

#endif /* __BSP_ADAPTER_PORT_EXTERNFLASH_H__ */
