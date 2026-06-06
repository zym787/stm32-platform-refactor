/******************************************************************************
 * @file bsp_wrapper_audio.c
 *
 * @par dependencies
 * - bsp_wrapper_audio.h
 * - wt588_cmd.h
 *
 * @author Ethan-Hang
 *
 * @brief Implementation of the abstract audio interface.
 *        Maintains a static array of registered driver vtables and dispatches
 *        all public API calls to the currently active slot.
 *        Volume levels (1..16) are mapped to WT588 device volume codes
 *        (0xE0..0xEF) via an internal helper.
 *
 * Processing flow:
 *   1. audio_drv_mount() copies the adapter vtable into s_audio_drv[].
 *   2. All public API functions resolve the active driver via
 *      s_cur_audio_drv_idx and forward through the corresponding function ptr.
 *
 * @version V1.0 2026-04-19
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_wrapper_audio.h"
#include "wt588_cmd.h"

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define AUDIO_DRV_MAX_NUM 1

static drv_audio_t s_audio_drv[AUDIO_DRV_MAX_NUM] = {0};
static UINT8_T     s_cur_audio_drv_idx            =   0;

/**
 * @brief   Convert application volume level (1..16) to WT588 hardware code.
 *          Maps linearly: level 1 -> 0xE0, level 16 -> 0xEF.
 *
 * @param[in] level : Application volume level (1..16).
 *
 * @return  WT588 volume command byte.
 */
static UINT8_T prv_volume_level_to_cmd(UINT8_T level)
{
    return (UINT8_T)(WT588_MIN_VOLUME_CODE + (level - 1U));
}

//******************************** Defines **********************************//

//******************************* Declaring *********************************//

//******************************* Declaring *********************************//

//******************************* Functions *********************************//

/**
 * @brief   Register an audio driver into the wrapper slot table.
 *
 * @param[in] idx : Slot index (0 ~ AUDIO_DRV_MAX_NUM-1).
 * @param[in] drv : Pointer to the vtable-filled driver struct.
 *
 * @return  true  - Mounted successfully.
 *          false - Invalid index or NULL drv.
 */
BOOL_T audio_drv_mount(UINT8_T idx, drv_audio_t *const drv)
{
    if (NULL == drv || idx >= AUDIO_DRV_MAX_NUM)
    {
        return false;
    }

    /**
    * drv_audio_t is a flat vtable, so a whole-struct copy captures every
    * slot — replaces the per-field copy. idx is pinned to the slot.
    **/
    s_audio_drv[idx]     = *drv;
    s_audio_drv[idx].idx = idx;

    s_cur_audio_drv_idx = idx;

    return true;
}


/**
 * @brief   Initialise the currently active audio driver.
 */
void audio_drv_init  (void)
{
    drv_audio_t *p_drv = &s_audio_drv[s_cur_audio_drv_idx];
    if (p_drv->pf_audio_drv_init)
    {
        p_drv->pf_audio_drv_init(p_drv);
    }
}

/**
 * @brief   Deinitialise the currently active audio driver.
 */
void audio_drv_deinit(void)
{
    drv_audio_t *p_drv = &s_audio_drv[s_cur_audio_drv_idx];
    if (p_drv->pf_audio_drv_deinit)
    {
        p_drv->pf_audio_drv_deinit(p_drv);
    }
}

/**
 * @brief   Forward play request to the audio driver.
 *          Validates volume range (1..16) and converts to hardware code
 *          before dispatching.
 *
 * @param[in] priority   : Playback priority.
 * @param[in] volume     : Volume level (1..16).
 * @param[in] voice_addr : Voice clip address in the audio device.
 *
 * @return  PLATFORM_OK on success, error code otherwise.
 */
platform_err_t audio_drv_play  (UINT8_T    priority,
                                   UINT8_T      volume,
                                   UINT8_T  voice_addr)
{
    if (volume < AUDIO_VOLUME_LEVEL_MIN || volume > AUDIO_VOLUME_LEVEL_MAX)
    {
        return PLATFORM_ERR_PARAM;
    }

    UINT8_T volume_cmd = prv_volume_level_to_cmd(volume);

    drv_audio_t *p_drv = &s_audio_drv[s_cur_audio_drv_idx];
    if (p_drv->pf_audio_drv_play)
    {
        return p_drv->pf_audio_drv_play(p_drv, priority, volume_cmd, voice_addr);
    }
    return PLATFORM_ERR_GENERAL;
}

/**
 * @brief   Forward stop request to the audio driver.
 */
platform_err_t audio_drv_stop  (void)
{
    drv_audio_t *p_drv = &s_audio_drv[s_cur_audio_drv_idx];
    if (p_drv->pf_audio_drv_stop)
    {
        return p_drv->pf_audio_drv_stop(p_drv);
    }
    return PLATFORM_ERR_GENERAL;
}

//******************************* Functions *********************************//
