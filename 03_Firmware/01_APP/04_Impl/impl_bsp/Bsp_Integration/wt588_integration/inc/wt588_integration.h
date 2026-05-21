/******************************************************************************
 * @file wt588_intergration.h
 *
 * @par dependencies
 * - bsp_wt588_handler.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection for the WT588 voice handler.
 *
 * Assembles concrete GPIO, busy-detect, PWM-DMA, OS, and heap
 * implementations and exports wt588_handler_input_args / wt588_handler_inst
 * for use by the task resource config.
 *
 * Call wt588_integration_inst() once at system initialisation to wire up
 * all interfaces and start the handler threads.
 *
 * @version V1.0 2026-04-16
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __WT588_INTEGRATION_H__
#define __WT588_INTEGRATION_H__

//******************************** Includes *********************************//
#include "bsp_wt588_handler.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
/**
 * @brief Assembled OS/hardware input arguments for the WT588 handler.
 *        Pass to wt588_handler_inst() as the second argument.
 */
extern wt_handler_input_args_t wt588_handler_input_args;

//******************************* Declaring *********************************//

#endif /* __WT588_INTEGRATION_H__ */
