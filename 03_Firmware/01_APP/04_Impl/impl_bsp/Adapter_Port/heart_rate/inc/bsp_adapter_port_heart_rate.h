/******************************************************************************
 * @file bsp_adapter_port_heart_rate.h
 *
 * @par dependencies
 * - bsp_wrapper_heart_rate.h
 *
 * @author Ethan-Hang
 *
 * @brief Adapter port interface for the EM7028 PPG heart-rate sensor.
 *        Registers a concrete driver implementation into the
 *        bsp_wrapper_heart_rate vtable.
 *
 * @version V1.0 2026-05-07
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_ADAPTER_PORT_HEART_RATE_H__
#define __BSP_ADAPTER_PORT_HEART_RATE_H__

//******************************** Includes *********************************//
#include "bsp_wrapper_heart_rate.h"
//******************************** Includes *********************************//

//******************************* Functions *********************************//
/**
 * @brief    Register the EM7028 driver into the bsp_wrapper_heart_rate
 *           vtable. Must be called once during pre-kernel BSP init,
 *           before any wrapper API call. Pure function-pointer assignment;
 *           no hardware probe and no OSAL primitive use.
 *
 * @return   true on success.
 * */
bool drv_adapter_heart_rate_register(void);
//******************************* Functions *********************************//

#endif /* __BSP_ADAPTER_PORT_HEART_RATE_H__ */
