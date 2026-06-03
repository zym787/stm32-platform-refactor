/******************************************************************************
 * @file cfg_storage.h
 *
 * @par dependencies
 * - stdint.h
 *
 * @author Ethan-Hang
 *
 * @brief Project-level configuration for the external flash storage layer.
 *
 *        Holds:
 *          - User-tunable runtime sizes (LVGL transient buffer cap,
 *            LVGL font glyph scratch buffer)
 *          - Layout of the LVGL sub-region of W25Q64 (asset offsets / sizes)
 *          - Bootstrap magic used to detect a freshly-erased chip
 *
 *        Addresses here are byte offsets RELATIVE to the start of the LVGL
 *        sub-region (MEMORY_LVGL_START_ADDRESS); callers add the absolute
 *        sub-region base when invoking driver-level APIs.
 *
 * @version V1.2 2026-06-03  Move refreshed LVGL images/fonts to W25Q64.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __CFG_STORAGE_H__
#define __CFG_STORAGE_H__

//******************************** Includes *********************************//
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/**
 * @brief Default cap for the transient LVGL data buffer used by callers that
 *        want a one-shot blocking read.  Sized to fit the largest pointer
 *        sprite (1920 B for fen 80x8 alpha).  Large background images must
 *        NOT use this path -- they need a custom LVGL image decoder with
 *        line-level streaming.
 */
#ifndef CFG_LVGL_DATA_MAX_SIZE
#define CFG_LVGL_DATA_MAX_SIZE       (2048U)
#endif

/**
 * @brief Erase granularity of the W25Q64 (4-KiB sector).  Used by the
 *        bootstrap path to size sector-aligned writes.
 */
#define CFG_W25Q64_SECTOR_SIZE       (4096U)

/**
 * @brief Magic written to the first 4 bytes of the LVGL sub-region after a
 *        successful bootstrap.  Mismatch on boot triggers a re-flash of the
 *        LVGL pointer-asset images.
 *
 *        Bumped on every asset-layout change so chips holding an older map
 *        are forced to re-provision: 0xA55A5AA5 → 0xA55A5AA6 (new SquareLine
 *        UI assets) → 0xA55A5AA7 (3 watchdight 60×60 moved off .rodata onto
 *        W25Q64).
 */
#define CFG_LVGL_ASSET_MAGIC         (0xA55A5AA8UL)

/**
 * @brief Scratch buffer used by the W25Q64-backed LVGL font callback.
 *        Sized for the largest glyph in the current 82 px 4-bpp custom font.
 */
#ifndef CFG_LVGL_FONT_GLYPH_BUFFER_SIZE
#define CFG_LVGL_FONT_GLYPH_BUFFER_SIZE (8192U)
#endif

/* ── LVGL sub-region asset layout (offsets within the LVGL sub-region) ────
 *
 *   0x000000  magic                (4    B)
 *   0x000100  fen_80x8             (1920 B)   -- 256 B page-aligned sprite
 *   0x000900  miao_70x5            (1050 B)
 *   0x000E00  time_50x8            (1200 B)
 *   0x002000  MDLBG_240x280        (201600 B)  -- Clock_1/Clock_2 background
 *   0x034000  biaopan1_200x200     (120000 B)  -- Clock_3 analog dial
 *   0x052000  watchdight1_60x60    (10800 B)   -- Clock_3 step icon frame 1
 *   0x055000  watchdight2_60x60    (10800 B)   -- Clock_3 step icon frame 2
 *   0x058000  watchdight3_60x60    (10800 B)   -- Clock_3 step icon frame 3
 *   0x05B000  refreshed UI images  (sector-aligned entries)
 *   0x07F000  custom font bitmaps  (sector-aligned entries)
 *   ...       (free)
 *
 * Sprites (fen / miao / time) are kept in firmware .rodata as a fallback
 * seed so the bootstrap can self-provision a freshly-erased chip.  Larger
 * images live ONLY on W25Q64; firmware does not carry a copy.
 * `make flash-assets` populates the W25Q64 before the first boot; once the
 * magic is set the line-streaming LVGL decoder serves them from there.
 */
#define CFG_LVGL_ASSET_MAGIC_OFFSET     (0x000000UL)
#define CFG_LVGL_ASSET_MAGIC_SIZE       (4U)

/* fen sprite: 80x8 RGB565 + 8-bit alpha = 3 B/pixel = 1920 B. */
#define CFG_LVGL_ASSET_FEN_OFFSET       (0x000100UL)
#define CFG_LVGL_ASSET_FEN_W            (80U)
#define CFG_LVGL_ASSET_FEN_H            (8U)
#define CFG_LVGL_ASSET_FEN_PX_SIZE      (3U)
#define CFG_LVGL_ASSET_FEN_SIZE         (CFG_LVGL_ASSET_FEN_W * \
                                         CFG_LVGL_ASSET_FEN_H * \
                                         CFG_LVGL_ASSET_FEN_PX_SIZE)

/* miao sprite: 70x5 RGB565 + 8-bit alpha = 3 B/pixel = 1050 B. */
#define CFG_LVGL_ASSET_MIAO_OFFSET      (0x000900UL)
#define CFG_LVGL_ASSET_MIAO_W           (70U)
#define CFG_LVGL_ASSET_MIAO_H           (5U)
#define CFG_LVGL_ASSET_MIAO_PX_SIZE     (3U)
#define CFG_LVGL_ASSET_MIAO_SIZE        (CFG_LVGL_ASSET_MIAO_W * \
                                         CFG_LVGL_ASSET_MIAO_H * \
                                         CFG_LVGL_ASSET_MIAO_PX_SIZE)

/* time sprite: 50x8 RGB565 + 8-bit alpha = 3 B/pixel = 1200 B. */
#define CFG_LVGL_ASSET_TIME_OFFSET      (0x000E00UL)
#define CFG_LVGL_ASSET_TIME_W           (50U)
#define CFG_LVGL_ASSET_TIME_H           (8U)
#define CFG_LVGL_ASSET_TIME_PX_SIZE     (3U)
#define CFG_LVGL_ASSET_TIME_SIZE        (CFG_LVGL_ASSET_TIME_W * \
                                         CFG_LVGL_ASSET_TIME_H * \
                                         CFG_LVGL_ASSET_TIME_PX_SIZE)

/* MDLBG full-screen background: 240x280 RGB565 + 8-bit alpha = 3 B/pixel = 201600 B. */
#define CFG_LVGL_ASSET_MDLBG_OFFSET     (0x002000UL)
#define CFG_LVGL_ASSET_MDLBG_W          (240U)
#define CFG_LVGL_ASSET_MDLBG_H          (280U)
#define CFG_LVGL_ASSET_MDLBG_PX_SIZE    (3U)
#define CFG_LVGL_ASSET_MDLBG_SIZE       (CFG_LVGL_ASSET_MDLBG_W * \
                                         CFG_LVGL_ASSET_MDLBG_H * \
                                         CFG_LVGL_ASSET_MDLBG_PX_SIZE)

/* biaopan1 analog dial: 200x200 RGB565 + 8-bit alpha = 3 B/pixel = 120000 B.
 * Aligned to next 4 KB sector after MDLBG (0x002000 + 0x31380 = 0x33380 → 0x34000). */
#define CFG_LVGL_ASSET_BIAOPAN1_OFFSET  (0x034000UL)
#define CFG_LVGL_ASSET_BIAOPAN1_W       (200U)
#define CFG_LVGL_ASSET_BIAOPAN1_H       (200U)
#define CFG_LVGL_ASSET_BIAOPAN1_PX_SIZE (3U)
#define CFG_LVGL_ASSET_BIAOPAN1_SIZE    (CFG_LVGL_ASSET_BIAOPAN1_W * \
                                         CFG_LVGL_ASSET_BIAOPAN1_H * \
                                         CFG_LVGL_ASSET_BIAOPAN1_PX_SIZE)

/* watchdight 1/2/3 walking-step icon frames: 60x60 alpha = 10800 B each.
 * Stored on 4 KB-sector boundaries so each can be re-flashed in isolation.
 * Streamed by lv_port_extflash decoder (same lv_extflash_meta_t protocol
 * as MDLBG/biaopan1 -- no RAM mirror since the 3 together would consume
 * 32 KB of RAM).  Saves ~32 KB Flash vs. embedding in .rodata.
 *
 * Geometry macros are written as literals (not aliases to a shared
 * CFG_LVGL_ASSET_WATCHDIGHT_*) so the pack_assets.py macro parser, which
 * only resolves literal `#define` pairs, can synthesise *_SIZE from W*H*PX. */
#define CFG_LVGL_ASSET_WATCHDIGHT1_OFFSET  (0x052000UL)
#define CFG_LVGL_ASSET_WATCHDIGHT1_W       (60U)
#define CFG_LVGL_ASSET_WATCHDIGHT1_H       (60U)
#define CFG_LVGL_ASSET_WATCHDIGHT1_PX_SIZE (3U)
#define CFG_LVGL_ASSET_WATCHDIGHT1_SIZE    (CFG_LVGL_ASSET_WATCHDIGHT1_W * \
                                            CFG_LVGL_ASSET_WATCHDIGHT1_H * \
                                            CFG_LVGL_ASSET_WATCHDIGHT1_PX_SIZE)

#define CFG_LVGL_ASSET_WATCHDIGHT2_OFFSET  (0x055000UL)
#define CFG_LVGL_ASSET_WATCHDIGHT2_W       (60U)
#define CFG_LVGL_ASSET_WATCHDIGHT2_H       (60U)
#define CFG_LVGL_ASSET_WATCHDIGHT2_PX_SIZE (3U)
#define CFG_LVGL_ASSET_WATCHDIGHT2_SIZE    (CFG_LVGL_ASSET_WATCHDIGHT2_W * \
                                            CFG_LVGL_ASSET_WATCHDIGHT2_H * \
                                            CFG_LVGL_ASSET_WATCHDIGHT2_PX_SIZE)

#define CFG_LVGL_ASSET_WATCHDIGHT3_OFFSET  (0x058000UL)
#define CFG_LVGL_ASSET_WATCHDIGHT3_W       (60U)
#define CFG_LVGL_ASSET_WATCHDIGHT3_H       (60U)
#define CFG_LVGL_ASSET_WATCHDIGHT3_PX_SIZE (3U)
#define CFG_LVGL_ASSET_WATCHDIGHT3_SIZE    (CFG_LVGL_ASSET_WATCHDIGHT3_W * \
                                            CFG_LVGL_ASSET_WATCHDIGHT3_H * \
                                            CFG_LVGL_ASSET_WATCHDIGHT3_PX_SIZE)

#define CFG_LVGL_ASSET_SHESHIDU_OFFSET      (0x05B000UL)
#define CFG_LVGL_ASSET_SHESHIDU_W           (10U)
#define CFG_LVGL_ASSET_SHESHIDU_H           (10U)
#define CFG_LVGL_ASSET_SHESHIDU_PX_SIZE     (3U)
#define CFG_LVGL_ASSET_SHESHIDU_SIZE        (CFG_LVGL_ASSET_SHESHIDU_W * \
                                            CFG_LVGL_ASSET_SHESHIDU_H * \
                                            CFG_LVGL_ASSET_SHESHIDU_PX_SIZE)

#define CFG_LVGL_ASSET_WATHER16X16_OFFSET   (0x05C000UL)
#define CFG_LVGL_ASSET_WATHER16X16_W        (16U)
#define CFG_LVGL_ASSET_WATHER16X16_H        (16U)
#define CFG_LVGL_ASSET_WATHER16X16_PX_SIZE  (3U)
#define CFG_LVGL_ASSET_WATHER16X16_SIZE     (CFG_LVGL_ASSET_WATHER16X16_W * \
                                            CFG_LVGL_ASSET_WATHER16X16_H * \
                                            CFG_LVGL_ASSET_WATHER16X16_PX_SIZE)

#define CFG_LVGL_ASSET_HEART16X16_OFFSET    (0x05D000UL)
#define CFG_LVGL_ASSET_HEART16X16_W         (16U)
#define CFG_LVGL_ASSET_HEART16X16_H         (16U)
#define CFG_LVGL_ASSET_HEART16X16_PX_SIZE   (3U)
#define CFG_LVGL_ASSET_HEART16X16_SIZE      (CFG_LVGL_ASSET_HEART16X16_W * \
                                            CFG_LVGL_ASSET_HEART16X16_H * \
                                            CFG_LVGL_ASSET_HEART16X16_PX_SIZE)

#define CFG_LVGL_ASSET_KLL16X16_OFFSET      (0x05E000UL)
#define CFG_LVGL_ASSET_KLL16X16_W           (16U)
#define CFG_LVGL_ASSET_KLL16X16_H           (16U)
#define CFG_LVGL_ASSET_KLL16X16_PX_SIZE     (3U)
#define CFG_LVGL_ASSET_KLL16X16_SIZE        (CFG_LVGL_ASSET_KLL16X16_W * \
                                            CFG_LVGL_ASSET_KLL16X16_H * \
                                            CFG_LVGL_ASSET_KLL16X16_PX_SIZE)

#define CFG_LVGL_ASSET_FOOT16X16_OFFSET     (0x05F000UL)
#define CFG_LVGL_ASSET_FOOT16X16_W          (16U)
#define CFG_LVGL_ASSET_FOOT16X16_H          (16U)
#define CFG_LVGL_ASSET_FOOT16X16_PX_SIZE    (3U)
#define CFG_LVGL_ASSET_FOOT16X16_SIZE       (CFG_LVGL_ASSET_FOOT16X16_W * \
                                            CFG_LVGL_ASSET_FOOT16X16_H * \
                                            CFG_LVGL_ASSET_FOOT16X16_PX_SIZE)

#define CFG_LVGL_ASSET_BT32_OFFSET          (0x060000UL)
#define CFG_LVGL_ASSET_BT32_W               (32U)
#define CFG_LVGL_ASSET_BT32_H               (32U)
#define CFG_LVGL_ASSET_BT32_PX_SIZE         (3U)
#define CFG_LVGL_ASSET_BT32_SIZE            (CFG_LVGL_ASSET_BT32_W * \
                                            CFG_LVGL_ASSET_BT32_H * \
                                            CFG_LVGL_ASSET_BT32_PX_SIZE)

#define CFG_LVGL_ASSET_MIANTI_0_OFFSET      (0x061000UL)
#define CFG_LVGL_ASSET_MIANTI_0_W           (32U)
#define CFG_LVGL_ASSET_MIANTI_0_H           (32U)
#define CFG_LVGL_ASSET_MIANTI_0_PX_SIZE     (3U)
#define CFG_LVGL_ASSET_MIANTI_0_SIZE        (CFG_LVGL_ASSET_MIANTI_0_W * \
                                            CFG_LVGL_ASSET_MIANTI_0_H * \
                                            CFG_LVGL_ASSET_MIANTI_0_PX_SIZE)

#define CFG_LVGL_ASSET_ZHENGDONG_0_OFFSET   (0x062000UL)
#define CFG_LVGL_ASSET_ZHENGDONG_0_W        (32U)
#define CFG_LVGL_ASSET_ZHENGDONG_0_H        (32U)
#define CFG_LVGL_ASSET_ZHENGDONG_0_PX_SIZE  (3U)
#define CFG_LVGL_ASSET_ZHENGDONG_0_SIZE     (CFG_LVGL_ASSET_ZHENGDONG_0_W * \
                                            CFG_LVGL_ASSET_ZHENGDONG_0_H * \
                                            CFG_LVGL_ASSET_ZHENGDONG_0_PX_SIZE)

#define CFG_LVGL_ASSET_COPESSS_OFFSET       (0x063000UL)
#define CFG_LVGL_ASSET_COPESSS_W            (32U)
#define CFG_LVGL_ASSET_COPESSS_H            (32U)
#define CFG_LVGL_ASSET_COPESSS_PX_SIZE      (3U)
#define CFG_LVGL_ASSET_COPESSS_SIZE         (CFG_LVGL_ASSET_COPESSS_W * \
                                            CFG_LVGL_ASSET_COPESSS_H * \
                                            CFG_LVGL_ASSET_COPESSS_PX_SIZE)

#define CFG_LVGL_ASSET_WEATER32X32_OFFSET   (0x064000UL)
#define CFG_LVGL_ASSET_WEATER32X32_W        (32U)
#define CFG_LVGL_ASSET_WEATER32X32_H        (32U)
#define CFG_LVGL_ASSET_WEATER32X32_PX_SIZE  (3U)
#define CFG_LVGL_ASSET_WEATER32X32_SIZE     (CFG_LVGL_ASSET_WEATER32X32_W * \
                                            CFG_LVGL_ASSET_WEATER32X32_H * \
                                            CFG_LVGL_ASSET_WEATER32X32_PX_SIZE)

#define CFG_LVGL_ASSET_ELLIPSE_OFFSET       (0x065000UL)
#define CFG_LVGL_ASSET_ELLIPSE_W            (40U)
#define CFG_LVGL_ASSET_ELLIPSE_H            (40U)
#define CFG_LVGL_ASSET_ELLIPSE_PX_SIZE      (3U)
#define CFG_LVGL_ASSET_ELLIPSE_SIZE         (CFG_LVGL_ASSET_ELLIPSE_W * \
                                            CFG_LVGL_ASSET_ELLIPSE_H * \
                                            CFG_LVGL_ASSET_ELLIPSE_PX_SIZE)

#define CFG_LVGL_ASSET_STIME_OFFSET         (0x067000UL)
#define CFG_LVGL_ASSET_STIME_W              (16U)
#define CFG_LVGL_ASSET_STIME_H              (8U)
#define CFG_LVGL_ASSET_STIME_PX_SIZE        (3U)
#define CFG_LVGL_ASSET_STIME_SIZE           (CFG_LVGL_ASSET_STIME_W * \
                                            CFG_LVGL_ASSET_STIME_H * \
                                            CFG_LVGL_ASSET_STIME_PX_SIZE)

#define CFG_LVGL_ASSET_SFEN_OFFSET          (0x068000UL)
#define CFG_LVGL_ASSET_SFEN_W               (21U)
#define CFG_LVGL_ASSET_SFEN_H               (6U)
#define CFG_LVGL_ASSET_SFEN_PX_SIZE         (3U)
#define CFG_LVGL_ASSET_SFEN_SIZE            (CFG_LVGL_ASSET_SFEN_W * \
                                            CFG_LVGL_ASSET_SFEN_H * \
                                            CFG_LVGL_ASSET_SFEN_PX_SIZE)

#define CFG_LVGL_ASSET_POWER_HIGHT_OFFSET   (0x069000UL)
#define CFG_LVGL_ASSET_POWER_HIGHT_W        (32U)
#define CFG_LVGL_ASSET_POWER_HIGHT_H        (32U)
#define CFG_LVGL_ASSET_POWER_HIGHT_PX_SIZE  (3U)
#define CFG_LVGL_ASSET_POWER_HIGHT_SIZE     (CFG_LVGL_ASSET_POWER_HIGHT_W * \
                                            CFG_LVGL_ASSET_POWER_HIGHT_H * \
                                            CFG_LVGL_ASSET_POWER_HIGHT_PX_SIZE)

#define CFG_LVGL_ASSET_LOCATION_OFFSET      (0x06A000UL)
#define CFG_LVGL_ASSET_LOCATION_W           (32U)
#define CFG_LVGL_ASSET_LOCATION_H           (32U)
#define CFG_LVGL_ASSET_LOCATION_PX_SIZE     (3U)
#define CFG_LVGL_ASSET_LOCATION_SIZE        (CFG_LVGL_ASSET_LOCATION_W * \
                                            CFG_LVGL_ASSET_LOCATION_H * \
                                            CFG_LVGL_ASSET_LOCATION_PX_SIZE)

#define CFG_LVGL_ASSET_TAIWAN_OFFSET        (0x06B000UL)
#define CFG_LVGL_ASSET_TAIWAN_W             (32U)
#define CFG_LVGL_ASSET_TAIWAN_H             (32U)
#define CFG_LVGL_ASSET_TAIWAN_PX_SIZE       (3U)
#define CFG_LVGL_ASSET_TAIWAN_SIZE          (CFG_LVGL_ASSET_TAIWAN_W * \
                                            CFG_LVGL_ASSET_TAIWAN_H * \
                                            CFG_LVGL_ASSET_TAIWAN_PX_SIZE)

#define CFG_LVGL_ASSET_NFC_OFFSET           (0x06C000UL)
#define CFG_LVGL_ASSET_NFC_W                (32U)
#define CFG_LVGL_ASSET_NFC_H                (32U)
#define CFG_LVGL_ASSET_NFC_PX_SIZE          (3U)
#define CFG_LVGL_ASSET_NFC_SIZE             (CFG_LVGL_ASSET_NFC_W * \
                                            CFG_LVGL_ASSET_NFC_H * \
                                            CFG_LVGL_ASSET_NFC_PX_SIZE)

#define CFG_LVGL_ASSET_LIANGDU_OFFSET       (0x06D000UL)
#define CFG_LVGL_ASSET_LIANGDU_W            (47U)
#define CFG_LVGL_ASSET_LIANGDU_H            (47U)
#define CFG_LVGL_ASSET_LIANGDU_PX_SIZE      (3U)
#define CFG_LVGL_ASSET_LIANGDU_SIZE         (CFG_LVGL_ASSET_LIANGDU_W * \
                                            CFG_LVGL_ASSET_LIANGDU_H * \
                                            CFG_LVGL_ASSET_LIANGDU_PX_SIZE)

#define CFG_LVGL_ASSET_ZNZBG_OFFSET         (0x06F000UL)
#define CFG_LVGL_ASSET_ZNZBG_W              (100U)
#define CFG_LVGL_ASSET_ZNZBG_H              (100U)
#define CFG_LVGL_ASSET_ZNZBG_PX_SIZE        (3U)
#define CFG_LVGL_ASSET_ZNZBG_SIZE           (CFG_LVGL_ASSET_ZNZBG_W * \
                                            CFG_LVGL_ASSET_ZNZBG_H * \
                                            CFG_LVGL_ASSET_ZNZBG_PX_SIZE)

#define CFG_LVGL_ASSET_ARW_OFFSET           (0x077000UL)
#define CFG_LVGL_ASSET_ARW_W                (50U)
#define CFG_LVGL_ASSET_ARW_H                (40U)
#define CFG_LVGL_ASSET_ARW_PX_SIZE          (3U)
#define CFG_LVGL_ASSET_ARW_SIZE             (CFG_LVGL_ASSET_ARW_W * \
                                            CFG_LVGL_ASSET_ARW_H * \
                                            CFG_LVGL_ASSET_ARW_PX_SIZE)

#define CFG_LVGL_ASSET_ZNZ_OFFSET           (0x079000UL)
#define CFG_LVGL_ASSET_ZNZ_W                (50U)
#define CFG_LVGL_ASSET_ZNZ_H                (50U)
#define CFG_LVGL_ASSET_ZNZ_PX_SIZE          (3U)
#define CFG_LVGL_ASSET_ZNZ_SIZE             (CFG_LVGL_ASSET_ZNZ_W * \
                                            CFG_LVGL_ASSET_ZNZ_H * \
                                            CFG_LVGL_ASSET_ZNZ_PX_SIZE)

#define CFG_LVGL_ASSET_HEART32X32_OFFSET    (0x07B000UL)
#define CFG_LVGL_ASSET_HEART32X32_W         (32U)
#define CFG_LVGL_ASSET_HEART32X32_H         (32U)
#define CFG_LVGL_ASSET_HEART32X32_PX_SIZE   (3U)
#define CFG_LVGL_ASSET_HEART32X32_SIZE      (CFG_LVGL_ASSET_HEART32X32_W * \
                                            CFG_LVGL_ASSET_HEART32X32_H * \
                                            CFG_LVGL_ASSET_HEART32X32_PX_SIZE)

#define CFG_LVGL_ASSET_TIWEN_OFFSET         (0x07C000UL)
#define CFG_LVGL_ASSET_TIWEN_W              (32U)
#define CFG_LVGL_ASSET_TIWEN_H              (32U)
#define CFG_LVGL_ASSET_TIWEN_PX_SIZE        (3U)
#define CFG_LVGL_ASSET_TIWEN_SIZE           (CFG_LVGL_ASSET_TIWEN_W * \
                                            CFG_LVGL_ASSET_TIWEN_H * \
                                            CFG_LVGL_ASSET_TIWEN_PX_SIZE)

#define CFG_LVGL_ASSET_PA_OFFSET            (0x07D000UL)
#define CFG_LVGL_ASSET_PA_W                 (32U)
#define CFG_LVGL_ASSET_PA_H                 (32U)
#define CFG_LVGL_ASSET_PA_PX_SIZE           (3U)
#define CFG_LVGL_ASSET_PA_SIZE              (CFG_LVGL_ASSET_PA_W * \
                                            CFG_LVGL_ASSET_PA_H * \
                                            CFG_LVGL_ASSET_PA_PX_SIZE)

#define CFG_LVGL_ASSET_LOCATION32X32_OFFSET (0x07E000UL)
#define CFG_LVGL_ASSET_LOCATION32X32_W      (32U)
#define CFG_LVGL_ASSET_LOCATION32X32_H      (32U)
#define CFG_LVGL_ASSET_LOCATION32X32_PX_SIZE (3U)
#define CFG_LVGL_ASSET_LOCATION32X32_SIZE   (CFG_LVGL_ASSET_LOCATION32X32_W * \
                                            CFG_LVGL_ASSET_LOCATION32X32_H * \
                                            CFG_LVGL_ASSET_LOCATION32X32_PX_SIZE)

#define CFG_LVGL_FONT_INTERTTF_24_BITMAP_OFFSET (0x07F000UL)
#define CFG_LVGL_FONT_INTERTTF_24_BITMAP_SIZE   (25753U)

#define CFG_LVGL_FONT_INTERTTF_10_BITMAP_OFFSET (0x086000UL)
#define CFG_LVGL_FONT_INTERTTF_10_BITMAP_SIZE   (5051U)

#define CFG_LVGL_FONT_INTERTTF_82_BITMAP_OFFSET (0x088000UL)
#define CFG_LVGL_FONT_INTERTTF_82_BITMAP_SIZE   (288357U)

#define CFG_LVGL_FONT_ALIMAMA_16_BITMAP_OFFSET  (0x0CF000UL)
#define CFG_LVGL_FONT_ALIMAMA_16_BITMAP_SIZE    (11848U)

#define CFG_LVGL_FONT_ALIMAMA_36_BITMAP_OFFSET  (0x0D2000UL)
#define CFG_LVGL_FONT_ALIMAMA_36_BITMAP_SIZE    (53612U)

#define CFG_LVGL_FONT_DIGITALDREAMFATNARROW_36_BITMAP_OFFSET (0x0E0000UL)
#define CFG_LVGL_FONT_DIGITALDREAMFATNARROW_36_BITMAP_SIZE   (57756U)

#define CFG_LVGL_FONT_ALIMAMA_12_BITMAP_OFFSET  (0x0EF000UL)
#define CFG_LVGL_FONT_ALIMAMA_12_BITMAP_SIZE    (6637U)

#define CFG_LVGL_FONT_ALIMAMA_10_BITMAP_OFFSET  (0x0F1000UL)
#define CFG_LVGL_FONT_ALIMAMA_10_BITMAP_SIZE    (5152U)

/**
 * @brief One-past-last byte used by LVGL assets.  Bootstrap erases sectors
 *        up to here on a magic mismatch.
 */
#define CFG_LVGL_ASSET_FOOTPRINT        (CFG_LVGL_FONT_ALIMAMA_10_BITMAP_OFFSET + \
                                         CFG_LVGL_FONT_ALIMAMA_10_BITMAP_SIZE)
//******************************** Defines **********************************//

#endif /* __CFG_STORAGE_H__ */
