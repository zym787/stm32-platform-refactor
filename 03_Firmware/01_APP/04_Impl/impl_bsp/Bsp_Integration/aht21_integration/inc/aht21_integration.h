/******************************************************************************
 * @file aht21_integration.h
 *
 * @par dependencies
 * - bsp_temp_humi_xxx_handler.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection for the AHT21 temperature/humidity handler.
 *
 * Assembles concrete IIC porting, OS queue, timebase and yield
 * implementations and exports aht21_input_arg for use by the task
 * resource config.
 *
 * @version V1.0 2026-04-12
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __AHT21_INTEGRATION_H__
#define __AHT21_INTEGRATION_H__

//******************************** Includes *********************************//
#include "bsp_temp_humi_xxx_handler.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
/**
 * @brief Assembled input argument for the AHT21 handler thread.
 *        Pass &aht21_input_arg to temp_humi_handler_thread() as argument.
 */
extern temp_humi_handler_input_arg_t aht21_input_arg;
//******************************* Declaring *********************************//

#endif /* __AHT21_INTEGRATION_H__ */
