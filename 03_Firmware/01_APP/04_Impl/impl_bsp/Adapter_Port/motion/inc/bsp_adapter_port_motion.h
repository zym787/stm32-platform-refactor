/******************************************************************************
 * @file bsp_adapter_port_motion.h
 *
 * @par dependencies
 * - bsp_wrapper_motion.h
 *
 * @author Ethan-Hang
 *
 * @brief Adapter registration interface for the MPU6050 motion driver.
 *        Bridges the MPU6050 handler layer to the motion wrapper vtable.
 *        Call drv_adapter_motion_register() once at system initialisation
 *        before any motion_drv_* API is used.
 *
 * Processing flow:
 *   drv_adapter_motion_register()
 *     → fills motion_drv_t vtable with MPU6050-specific implementations
 *     → calls drv_adapter_motion_mount() to register into the wrapper
 *
 * @version V1.0 2026-04-16
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_ADAPTER_PORT_MOTION_H__
#define __BSP_ADAPTER_PORT_MOTION_H__

//******************************** Includes *********************************//
#include "bsp_wrapper_motion.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
/**
 * @brief   Register the MPU6050 driver into the motion wrapper.
 *          Must be called once before any motion_drv_* API is invoked.
 *
 * @return  true  - Registration successful.
 *          false - Mount failed (index out of range or NULL vtable).
 */
bool drv_adapter_motion_register(void);
//******************************* Declaring *********************************//

#endif /* __BSP_ADAPTER_PORT_MOTION_H__ */
