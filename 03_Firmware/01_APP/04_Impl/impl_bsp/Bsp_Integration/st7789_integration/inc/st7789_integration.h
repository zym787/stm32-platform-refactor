/******************************************************************************
 * @file st7789_integration.h
 *
 * @par dependencies
 * - bsp_st7789_driver.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection bundle for the ST7789 LCD driver.
 *
 * Assembles concrete SPI / timebase / OS implementations and exports
 * st7789_input_arg for use by the display adapter port.  Changing the
 * target MCU or RTOS requires only editing this file.
 *
 * @version V1.0 2026-04-25
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __ST7789_INTEGRATION_H__
#define __ST7789_INTEGRATION_H__

//******************************** Includes *********************************//
#include "bsp_st7789_driver.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
/**
 * @brief Assembled ST7789 input arg — SPI, timebase, OS interfaces + panel.
 *        Pass to bsp_st7789_driver_inst() at startup from the display adapter.
 */
extern st7789_driver_input_arg_t st7789_input_arg;
//******************************* Declaring *********************************//

#endif /* __ST7789_INTEGRATION_H__ */
