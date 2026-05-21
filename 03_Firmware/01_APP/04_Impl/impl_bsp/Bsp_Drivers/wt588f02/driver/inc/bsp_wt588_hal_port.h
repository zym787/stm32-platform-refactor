/******************************************************************************
 * @file bsp_wt588_hal_port.h
 *
 * @par dependencies
 * - bsp_wt588_driver.h
 *
 * @author Ethan-Hang
 *
 * @brief Expose STM32 HAL concrete implementations of the WT588 hardware
 *        interfaces (GPIO, busy-detect, PWM-DMA).
 *
 * Processing flow:
 * bsp_wt588_hal_port.c provides platform-specific implementations.
 * Integration layer references these structs for dependency injection.
 *
 * @version V1.0 2026-04-17
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/
#pragma once
#ifndef __BSP_WT588_HAL_PORT_H__
#define __BSP_WT588_HAL_PORT_H__

//******************************** Includes *********************************//
#include "bsp_wt588_driver.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
extern wt_gpio_interface_t    wt588_hal_gpio_interface;
extern wt_busy_interface_t    wt588_hal_busy_interface;
extern wt_pwm_dma_interface_t wt588_hal_pwm_dma_interface;
//******************************* Declaring *********************************//

#endif /* __BSP_WT588_HAL_PORT_H__ */
