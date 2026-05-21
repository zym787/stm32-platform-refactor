/******************************************************************************
 * @file platform_io_register.h
 *
 * @par dependencies
 * - bsp_adapter_port_temp_humi.h
 * - bsp_adapter_port_motion.h
 *
 * @author Ethan-Hang
 *
 * @brief Centralized registration of all BSP platform IO adapters.
 *
 * @details Call platform_io_register() once at system initialisation (before
 *          any wrapper init functions) to mount every hardware adapter into
 *          its corresponding wrapper layer.
 *
 * @version V1.0 2026-4-16
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __PLATFORM_IO_REGISTER_H__
#define __PLATFORM_IO_REGISTER_H__

//******************************** Includes *********************************//

//******************************** Includes *********************************//

//******************************* Declaring *********************************//

/**
 * @brief Register all BSP platform IO adapters.
 *
 * @details Calls each adapter's register function in turn so that the
 *          corresponding wrapper layers are fully populated before any
 *          driver init or task creation takes place.
 */
void platform_io_register(void);

//******************************* Declaring *********************************//

#endif /* __PLATFORM_IO_REGISTER_H__ */
