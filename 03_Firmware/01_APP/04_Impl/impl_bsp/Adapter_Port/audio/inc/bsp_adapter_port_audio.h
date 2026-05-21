/******************************************************************************
 * @file bsp_adapter_port_audio.h
 *
 * @par dependencies
 * - bsp_wrapper_audio.h
 *
 * @author Ethan-Hang
 *
 * @brief Adapter port interface for WT588F voice playback.
 *
 * Registers a concrete WT588F driver implementation into the
 * bsp_wrapper_audio vtable.
 *
 * Processing flow:
 *   drv_adapter_audio_register()
 *     -> fills drv_audio_t vtable with WT588F-specific implementations
 *     -> calls audio_drv_mount() to register into the wrapper
 *
 * @version V1.0 2026-04-19
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_ADAPTER_PORT_AUDIO_H__
#define __BSP_ADAPTER_PORT_AUDIO_H__

//******************************** Includes *********************************//
#include "bsp_wrapper_audio.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief   Register the WT588F driver into the bsp_wrapper_audio vtable.
 *          Call once during BSP initialisation, before any wrapper API call.
 *
 * @return  true  - Registration successful.
 *          false - Mount failed.
 */
bool drv_adapter_audio_register(void);

//******************************* Functions *********************************//

#endif /* __BSP_ADAPTER_PORT_AUDIO_H__ */
