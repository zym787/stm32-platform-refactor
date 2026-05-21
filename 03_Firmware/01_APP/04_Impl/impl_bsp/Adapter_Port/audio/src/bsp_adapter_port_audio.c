/******************************************************************************
 * @file bsp_adapter_port_audio.c
 *
 * @par dependencies
 * - bsp_wt588_handler.h
 * - bsp_adapter_port_audio.h
 *
 * @author Ethan-Hang
 *
 * @brief WT588F adapter implementation for the audio wrapper layer.
 *        Implements the drv_audio_t vtable using the WT588F handler API,
 *        then registers itself into the bsp_wrapper_audio.
 *
 * Processing flow:
 *   wt588_drv_play()
 *     -> wt588_handler_play_request(voice_addr, volume, priority)
 *   wt588_drv_stop()
 *     -> wt588_handler_stop()
 *
 * @version V1.0 2026-04-19
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_wt588_handler.h"
#include "bsp_adapter_port_audio.h"


//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief   Adapter init — no hardware action needed; WT588F is initialised
 *          by its own handler task.
 */
void (wt588_drv_init  )(struct _drv_audio_t * const dev)
{
    (void)dev;
}

/**
 * @brief   Adapter deinit — no hardware action needed here.
 */
void (wt588_drv_deinit)(struct _drv_audio_t * const dev)
{
    (void)dev;
}

/**
 * @brief   Forward play request to the WT588F handler.
 *
 * @param[in] dev        : Audio driver vtable (unused; adapter owns state).
 * @param[in] priority   : Playback priority.
 * @param[in] volume     : Volume code (pre-mapped by wrapper).
 * @param[in] voice_addr : Voice clip address.
 *
 * @return  WP_AUDIO_OK on success, WP_AUDIO_ERROR on handler failure.
 */
wp_audio_status_t (wt588_drv_play  )(struct _drv_audio_t * const dev,
                                                 uint8_t    priority,
                                                 uint8_t      volume,
                                                 uint8_t  voice_addr)
{
    (void)dev;
    wt_handler_status_t ret = WT_HANDLER_OK;

    ret = wt588_handler_play_request(voice_addr, volume, priority);
    if (WT_HANDLER_OK != ret)
    {
        return WP_AUDIO_ERROR;
    }

    return WP_AUDIO_OK;
}

/**
 * @brief   Forward stop request to the WT588F handler.
 */
wp_audio_status_t (wt588_drv_stop  )(struct _drv_audio_t * const dev)
{
    (void)dev;
    wt_handler_status_t ret = WT_HANDLER_OK;
    ret = wt588_handler_stop();
    if (WT_HANDLER_OK != ret)
    {
        return WP_AUDIO_ERROR;
    }

    return WP_AUDIO_OK;
}


/**
 * @brief   Register the WT588F driver into the motion wrapper.
 *          Fills the vtable and calls audio_drv_mount().
 *
 * @return  true  - Registration successful.
 *          false - Mount failed.
 */
bool drv_adapter_audio_register(void)
{
    drv_audio_t audio_drv = {
        .dev_id              =                0,
        .idx                 =                0,
        .user_data           =             NULL,
        .pf_audio_drv_init   =   wt588_drv_init,
        .pf_audio_drv_deinit = wt588_drv_deinit,
        .pf_audio_drv_play   =   wt588_drv_play,
        .pf_audio_drv_stop   =   wt588_drv_stop,
    };

    return audio_drv_mount(0U, &audio_drv);
}

//******************************* Functions *********************************//
