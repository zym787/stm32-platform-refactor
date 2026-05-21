/******************************************************************************
 * @file cst816t_integration.h
 *
 * @par dependencies
 * - bsp_cst816t_driver.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection bundle for the CST816T capacitive touch driver.
 *
 * Assembles concrete I2C / timebase / blocking delay / OS-yield
 * implementations and exports cst816t_input_arg for use by the touch
 * adapter port.  Changing the target MCU or RTOS requires only editing
 * this file.
 *
 * @version V1.0 2026-04-26
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __CST816T_INTEGRATION_H__
#define __CST816T_INTEGRATION_H__

//******************************** Includes *********************************//
#include "bsp_cst816t_driver.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
/**
 * @brief Bundle of every interface bsp_cst816t_inst() needs.
 *
 * The CST816T driver does not define a single input-arg struct of its own;
 * this aggregate keeps the integration layer the only place that knows
 * about the concrete implementations.
 */
typedef struct
{
    cst816t_iic_driver_interface_t  *p_iic_interface;
    cst816t_timebase_interface_t    *p_timebase_interface;
    cst816t_delay_interface_t       *p_delay_interface;
    cst816t_os_delay_interface_t    *p_os_interface;
} cst816t_driver_input_arg_t;

/**
 * @brief Assembled CST816T input arg — I2C, timebase, delay, OS interfaces.
 *        Pass to bsp_cst816t_inst() at startup from the touch adapter.
 */
extern cst816t_driver_input_arg_t cst816t_input_arg;
//******************************* Declaring *********************************//

#endif /* __CST816T_INTEGRATION_H__ */
