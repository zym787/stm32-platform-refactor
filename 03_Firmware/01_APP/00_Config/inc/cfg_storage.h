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
 *          - User-tunable runtime sizes (LVGL transient buffer cap)
 *          - Layout of the LVGL sub-region of W25Q64 (asset offsets / sizes)
 *          - Bootstrap magic used to detect a freshly-erased chip
 *
 *        Addresses here are byte offsets RELATIVE to the start of the LVGL
 *        sub-region (MEMORY_LVGL_START_ADDRESS); callers add the absolute
 *        sub-region base when invoking driver-level APIs.
 *
 * @version V1.1 2026-05-24  Switch to new SquareLine watch UI assets.
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
#define CFG_LVGL_ASSET_MAGIC         (0xA55A5AA7UL)

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

/**
 * @brief One-past-last byte used by LVGL assets.  Bootstrap erases sectors
 *        up to here on a magic mismatch.
 */
#define CFG_LVGL_ASSET_FOOTPRINT        (CFG_LVGL_ASSET_WATCHDIGHT3_OFFSET + \
                                         CFG_LVGL_ASSET_WATCHDIGHT3_SIZE)
//******************************** Defines **********************************//

#endif /* __CFG_STORAGE_H__ */
