/******************************************************************************
 * @file bsp_wrapper_audio.h
 *
 * @par dependencies
 * - platform_type.h
 *
 * @author Ethan-Hang
 *
 * @brief Abstract interface for audio (voice playback) access.
 *        Decouples the application from any specific audio driver.
 *        The adapter layer registers a concrete driver via audio_drv_mount();
 *        the application calls the public API without knowing which hardware
 *        is underneath.
 *
 * Processing flow:
 *   1. Adapter calls audio_drv_mount() to register the driver vtable.
 *   2. Application calls audio_drv_init() once at startup.
 *   3. Application uses audio_drv_play() / audio_drv_stop() to control
 *      voice playback through the registered driver.
 *
 * @version V1.0 2026-04-19
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __BSP_WRAPPER_AUDIO_H__
#define __BSP_WRAPPER_AUDIO_H__

//******************************** Includes *********************************//
#include "platform_type.h"
#include "platform_error.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define AUDIO_VOLUME_LEVEL_MIN  (1U)   /* Minimum volume level exposed to app */
#define AUDIO_VOLUME_LEVEL_MAX  (16U)  /* Maximum volume level exposed to app */
//******************************** Defines **********************************//

//******************************* Declaring *********************************//


/**
 * @brief Driver vtable for an audio (voice) playback device.
 *        Filled by the adapter layer; opaque to the application.
 */
typedef struct _drv_audio_t
{
    uint32_t                       idx;       /* Slot index in wrapper array    */
    uint32_t                    dev_id;       /* Hardware device identifier     */
    void            *        user_data;       /* Adapter private context        */

    void              (*pf_audio_drv_init   )(struct _drv_audio_t * const dev);
    void              (*pf_audio_drv_deinit )(struct _drv_audio_t * const dev);

    platform_err_t (*pf_audio_drv_play   )(struct _drv_audio_t * const dev,
                                                          uint8_t    priority,
                                                          uint8_t      volume,
                                                          uint8_t  voice_addr);
    platform_err_t (*pf_audio_drv_stop   )(struct _drv_audio_t * const dev);

} drv_audio_t;

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief   Register an audio driver into the wrapper slot table.
 *          Called exclusively by the adapter layer at system init.
 *
 * @param[in] idx : Slot index (0 ~ AUDIO_DRV_MAX_NUM-1).
 * @param[in] drv : Pointer to the vtable-filled driver struct.
 *
 * @return  true  - Mounted successfully.
 *          false - Invalid index or NULL drv.
 */
bool audio_drv_mount (uint8_t idx, drv_audio_t * const drv);

/**
 * @brief   Initialise the currently active audio driver.
 *          Forwards to pf_audio_drv_init in the registered vtable.
 */
void              audio_drv_init  (void);

/**
 * @brief   Deinitialise the currently active audio driver.
 */
void              audio_drv_deinit(void);

/**
 * @brief   Play a voice clip at the given priority and volume level.
 *
 * @param[in] priority  : Playback priority (lower number = higher urgency).
 * @param[in] volume    : Volume level 1..16 (mapped to hardware code internally).
 * @param[in] voice_addr: Address/index of the voice clip in the audio device.
 *
 * @return  PLATFORM_OK           - Play command accepted.
 *          PLATFORM_ERR_PARAM - Volume out of range.
 *          PLATFORM_ERR_GENERAL        - Driver error.
 */
platform_err_t audio_drv_play  (uint8_t    priority,
                                   uint8_t      volume,
                                   uint8_t  voice_addr);

/**
 * @brief   Stop the currently playing voice clip.
 *
 * @return  PLATFORM_OK on success, PLATFORM_ERR_GENERAL on driver error.
 */
platform_err_t audio_drv_stop  (void);

//******************************* Functions *********************************//

#endif /* __BSP_WRAPPER_AUDIO_H__ */
