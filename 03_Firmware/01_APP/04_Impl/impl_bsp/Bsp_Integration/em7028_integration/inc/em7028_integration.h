/******************************************************************************
 * @file em7028_integration.h
 *
 * @par dependencies
 * - bsp_em7028_handler.h
 *
 * @author Ethan-Hang
 *
 * @brief Dependency injection bundle for the EM7028 PPG sensor handler.
 *
 * Assembles concrete I2C / timebase / delay / OS-queue / OS-delay
 * implementations into em7028_input_arg, which is passed as the task
 * argument to em7028_handler_thread() at startup.
 *
 * The EM7028 sits on the shared sensor I2C bus (CORE_I2C_BUS_1) together
 * with MPU6050 and AHT21; bus arbitration is owned by the MCU I2C port
 * layer's bus mutex, so this integration only provides the leaf-level
 * mem read/write trampolines.
 *
 * @version V1.0 2026-05-05
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __EM7028_INTEGRATION_H__
#define __EM7028_INTEGRATION_H__

//******************************** Includes *********************************//
#include "bsp_em7028_handler.h"
//******************************** Includes *********************************//

//******************************* Declaring *********************************//
/**
 * @brief Assembled EM7028 input arg -- pass to em7028_handler_thread() as
 *        the task argument. The handler thread owns the underlying driver
 *        instance, the cmd queue and the frame queue created from this
 *        bundle.
 */
extern em7028_handler_input_args_t em7028_input_arg;
//******************************* Declaring *********************************//

#endif /* __EM7028_INTEGRATION_H__ */
