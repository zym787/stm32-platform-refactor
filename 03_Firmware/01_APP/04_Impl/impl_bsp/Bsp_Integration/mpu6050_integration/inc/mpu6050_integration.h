/******************************************************************************
 * @file mpu6050_integration.h
 *
 * @par dependencies
 * - bsp_mpuxxxx_handler.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection bundle for the MPU6050 motion sensor handler.
 *
 * Aggregates the I2C bus, timebase, delay, yield, OS and interrupt interface
 * instances needed by the MPU6050 handler layer.
 *
 * @version V1.0 2026-04-10
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __MPU6050_INTEGRATION_H__
#define __MPU6050_INTEGRATION_H__

//******************************** Includes *********************************//
#include "bsp_mpuxxxx_handler.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/**
 * @brief Assembled MPU6050 handler input arguments — I2C, timebase, delay,
 *        yield, OS and interrupt interfaces wired together.
 *        Pass to handler task at startup via mpuxxxx_handler_thread().
 */
extern mpuxxxx_handler_input_args_t   mpu6050_input_args;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//

//******************************* Functions *********************************//

#endif /* __MPU6050_INTEGRATION_H__ */
