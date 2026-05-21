/******************************************************************************
 * @file w25q64_integration.h
 *
 * @par dependencies
 * - bsp_w25q64_handler.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection bundle for the W25Q64 flash handler.
 *
 * Assembles concrete SPI / timebase / OS implementations and exports
 * w25q64_input_arg for use by the flash handler thread.  All HAL
 * coupling is localised in the matching .c file.
 *
 * @version V1.0 2026-04-27
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __W25Q64_INTEGRATION_H__
#define __W25Q64_INTEGRATION_H__

//******************************** Includes *********************************//
#include "bsp_w25q64_handler.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
/**
 * @brief Assembled W25Q64 input arg -- SPI, timebase, OS interfaces.
 *        Pass to flash_handler_thread() as the task argument.
 */
extern flash_input_args_t w25q64_input_arg;
//******************************* Declaring *********************************//

#endif /* __W25Q64_INTEGRATION_H__ */
