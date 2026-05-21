/******************************************************************************
 * @file wt588_cmd.h
 *
 * @par dependencies
 *
 * @author Ethan-Hang
 *
 * @brief Define WT588 command ranges for playback and volume control.
 *
 * Processing flow:
 * Provide command-code boundaries used by WT588 driver logic.
 * @version V1.0 2026-4-5
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define WT588_MAX_PLAY_CODE      (0xDFU)
#define WT588_MIN_PLAY_CODE      (0x00U)
#define WT588_START_PLAY_CODE    (0xF3U)
#define WT588_STOP_PLAY_CODE     (0xFEU)

#define WT588_MAX_VOLUME_CODE    (0xEFU)
#define WT588_MIN_VOLUME_CODE    (0xE0U)

//******************************** Defines **********************************//
