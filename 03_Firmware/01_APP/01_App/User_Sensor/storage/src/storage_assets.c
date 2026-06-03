/******************************************************************************
 * @file storage_assets.c
 *
 * @par dependencies
 * - service_storage_facade.h
 * - cfg_storage.h
 * - lvgl.h
 *
 * @author Ethan-Hang
 *
 * @brief LVGL assets that live on the external W25Q64 flash.
 *
 *        Two classes of asset:
 *
 *          1. Pointer sprites (fen 80x8, miao 70x5, time 50x8).
 *             Small enough to mirror into MCU RAM at boot.  The mirror
 *             is filled by reading from the external flash, which is
 *             provisioned by Tools/pack_assets.py.
 *
 *          2. Large backgrounds (MDLBG 240x280, biaopan1 200x200).
 *             Too large for RAM (200+ KB) and intentionally kept out of
 *             firmware .rodata.  Served line-by-line straight off W25Q64
 *             by the lv_port_extflash decoder.  The lv_img_dsc_t .data
 *             pointer is overloaded to carry an lv_extflash_meta_t the
 *             decoder recognises by its magic field.  data_size = 0
 *             tells LVGL it must call read_line_cb.
 *
 *        The full external asset pack, including backgrounds, icons and
 *        font bitmaps, must be written by `make flash-assets`.
 *
 * @version V1.1 2026-05-24  Switch to new SquareLine watch UI assets.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "service_storage_facade.h"
#include "cfg_storage.h"
#include "lv_port_extflash.h"

#include "lvgl.h"
#include "Debug.h"

#include <stdint.h>
//******************************** Includes *********************************//

//******************************* Variables *********************************//
/* RAM mirrors filled by Read_LvglData() during bootstrap. */
static uint8_t s_au8_fen_ram [CFG_LVGL_ASSET_FEN_SIZE];
static uint8_t s_au8_miao_ram[CFG_LVGL_ASSET_MIAO_SIZE];
static uint8_t s_au8_time_ram[CFG_LVGL_ASSET_TIME_SIZE];

/**
 * Externally-visible LVGL descriptors that point at the RAM mirrors.
 * Exposed via gui_guider.h's LV_IMG_DECLARE so setup_scr_Clock_*.c can
 * reference them without including this translation unit's header.
 */
const lv_img_dsc_t _fen_alpha_80x8_ext = {
    .header.cf          = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .header.always_zero = 0,
    .header.reserved    = 0,
    .header.w           = CFG_LVGL_ASSET_FEN_W,
    .header.h           = CFG_LVGL_ASSET_FEN_H,
    .data_size          = CFG_LVGL_ASSET_FEN_SIZE,
    .data               = s_au8_fen_ram,
};

const lv_img_dsc_t _miao_alpha_70x5_ext = {
    .header.cf          = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .header.always_zero = 0,
    .header.reserved    = 0,
    .header.w           = CFG_LVGL_ASSET_MIAO_W,
    .header.h           = CFG_LVGL_ASSET_MIAO_H,
    .data_size          = CFG_LVGL_ASSET_MIAO_SIZE,
    .data               = s_au8_miao_ram,
};

const lv_img_dsc_t _time_alpha_50x8_ext = {
    .header.cf          = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .header.always_zero = 0,
    .header.reserved    = 0,
    .header.w           = CFG_LVGL_ASSET_TIME_W,
    .header.h           = CFG_LVGL_ASSET_TIME_H,
    .data_size          = CFG_LVGL_ASSET_TIME_SIZE,
    .data               = s_au8_time_ram,
};

/* MDLBG 240x280 alpha background — too large for a RAM mirror, served
 * line by line directly off W25Q64 by the lv_port_extflash decoder. */
static const lv_extflash_meta_t s_mdlbg_meta = {
    .magic      = LV_EXTFLASH_DECODER_MAGIC,
    .ext_offset = CFG_LVGL_ASSET_MDLBG_OFFSET,
    .width      = CFG_LVGL_ASSET_MDLBG_W,
    .height     = CFG_LVGL_ASSET_MDLBG_H,
    .px_size    = CFG_LVGL_ASSET_MDLBG_PX_SIZE,
};

const lv_img_dsc_t _MDLBG_alpha_240x280_ext = {
    .header.cf          = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .header.always_zero = 0,
    .header.reserved    = 0,
    .header.w           = CFG_LVGL_ASSET_MDLBG_W,
    .header.h           = CFG_LVGL_ASSET_MDLBG_H,
    .data_size          = 0,
    .data               = (const uint8_t *)&s_mdlbg_meta,
};

/* biaopan1 200x200 alpha background — same streaming pattern. */
static const lv_extflash_meta_t s_biaopan1_meta = {
    .magic      = LV_EXTFLASH_DECODER_MAGIC,
    .ext_offset = CFG_LVGL_ASSET_BIAOPAN1_OFFSET,
    .width      = CFG_LVGL_ASSET_BIAOPAN1_W,
    .height     = CFG_LVGL_ASSET_BIAOPAN1_H,
    .px_size    = CFG_LVGL_ASSET_BIAOPAN1_PX_SIZE,
};

const lv_img_dsc_t _biaopan1_200x200_ext = {
    .header.cf          = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .header.always_zero = 0,
    .header.reserved    = 0,
    .header.w           = CFG_LVGL_ASSET_BIAOPAN1_W,
    .header.h           = CFG_LVGL_ASSET_BIAOPAN1_H,
    .data_size          = 0,
    .data               = (const uint8_t *)&s_biaopan1_meta,
};

/* watchdight 1/2/3 walking-step icon frames (60x60 alpha each).  Streamed
 * line-by-line from W25Q64 -- each frame is 10800 B; embedding all three
 * in .rodata would cost 32 KB Flash for icons that are otherwise static.
 * Same lv_extflash_meta_t protocol as the larger backgrounds. */
static const lv_extflash_meta_t s_watchdight1_meta = {
    .magic      = LV_EXTFLASH_DECODER_MAGIC,
    .ext_offset = CFG_LVGL_ASSET_WATCHDIGHT1_OFFSET,
    .width      = CFG_LVGL_ASSET_WATCHDIGHT1_W,
    .height     = CFG_LVGL_ASSET_WATCHDIGHT1_H,
    .px_size    = CFG_LVGL_ASSET_WATCHDIGHT1_PX_SIZE,
};

const lv_img_dsc_t _watchdight1_alpha_60x60_ext = {
    .header.cf          = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .header.always_zero = 0,
    .header.reserved    = 0,
    .header.w           = CFG_LVGL_ASSET_WATCHDIGHT1_W,
    .header.h           = CFG_LVGL_ASSET_WATCHDIGHT1_H,
    .data_size          = 0,
    .data               = (const uint8_t *)&s_watchdight1_meta,
};

static const lv_extflash_meta_t s_watchdight2_meta = {
    .magic      = LV_EXTFLASH_DECODER_MAGIC,
    .ext_offset = CFG_LVGL_ASSET_WATCHDIGHT2_OFFSET,
    .width      = CFG_LVGL_ASSET_WATCHDIGHT2_W,
    .height     = CFG_LVGL_ASSET_WATCHDIGHT2_H,
    .px_size    = CFG_LVGL_ASSET_WATCHDIGHT2_PX_SIZE,
};

const lv_img_dsc_t _watchdight2_alpha_60x60_ext = {
    .header.cf          = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .header.always_zero = 0,
    .header.reserved    = 0,
    .header.w           = CFG_LVGL_ASSET_WATCHDIGHT2_W,
    .header.h           = CFG_LVGL_ASSET_WATCHDIGHT2_H,
    .data_size          = 0,
    .data               = (const uint8_t *)&s_watchdight2_meta,
};

static const lv_extflash_meta_t s_watchdight3_meta = {
    .magic      = LV_EXTFLASH_DECODER_MAGIC,
    .ext_offset = CFG_LVGL_ASSET_WATCHDIGHT3_OFFSET,
    .width      = CFG_LVGL_ASSET_WATCHDIGHT3_W,
    .height     = CFG_LVGL_ASSET_WATCHDIGHT3_H,
    .px_size    = CFG_LVGL_ASSET_WATCHDIGHT3_PX_SIZE,
};

const lv_img_dsc_t _watchdight3_alpha_60x60_ext = {
    .header.cf          = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .header.always_zero = 0,
    .header.reserved    = 0,
    .header.w           = CFG_LVGL_ASSET_WATCHDIGHT3_W,
    .header.h           = CFG_LVGL_ASSET_WATCHDIGHT3_H,
    .data_size          = 0,
    .data               = (const uint8_t *)&s_watchdight3_meta,
};

#define DEFINE_EXTFLASH_IMAGE(symbol, cfgPrefix)                         \
    static const lv_extflash_meta_t s##symbol##_meta = {                 \
        .magic      = LV_EXTFLASH_DECODER_MAGIC,                         \
        .ext_offset = cfgPrefix##_OFFSET,                                \
        .width      = cfgPrefix##_W,                                     \
        .height     = cfgPrefix##_H,                                     \
        .px_size    = cfgPrefix##_PX_SIZE,                               \
    };                                                                   \
                                                                         \
    const lv_img_dsc_t symbol##_ext = {                                  \
        .header.cf          = LV_IMG_CF_TRUE_COLOR_ALPHA,                 \
        .header.always_zero = 0,                                         \
        .header.reserved    = 0,                                         \
        .header.w           = cfgPrefix##_W,                             \
        .header.h           = cfgPrefix##_H,                             \
        .data_size          = 0,                                         \
        .data               = (const uint8_t *)&s##symbol##_meta,        \
    }

DEFINE_EXTFLASH_IMAGE(_sheshidu_alpha_10x10, CFG_LVGL_ASSET_SHESHIDU);
DEFINE_EXTFLASH_IMAGE(_wather16x16_alpha_16x16, CFG_LVGL_ASSET_WATHER16X16);
DEFINE_EXTFLASH_IMAGE(_heart16x16_alpha_16x16, CFG_LVGL_ASSET_HEART16X16);
DEFINE_EXTFLASH_IMAGE(_KLL16x16_alpha_16x16, CFG_LVGL_ASSET_KLL16X16);
DEFINE_EXTFLASH_IMAGE(_foot16x16_alpha_16x16, CFG_LVGL_ASSET_FOOT16X16);
DEFINE_EXTFLASH_IMAGE(_BT32_alpha_32x32, CFG_LVGL_ASSET_BT32);
DEFINE_EXTFLASH_IMAGE(_mianti_0_alpha_32x32, CFG_LVGL_ASSET_MIANTI_0);
DEFINE_EXTFLASH_IMAGE(_zhengdong_0_alpha_32x32, CFG_LVGL_ASSET_ZHENGDONG_0);
DEFINE_EXTFLASH_IMAGE(_copesss_alpha_32x32, CFG_LVGL_ASSET_COPESSS);
DEFINE_EXTFLASH_IMAGE(_weater32x32_alpha_32x32, CFG_LVGL_ASSET_WEATER32X32);
DEFINE_EXTFLASH_IMAGE(_Ellipse_alpha_40x40, CFG_LVGL_ASSET_ELLIPSE);
DEFINE_EXTFLASH_IMAGE(_Stime_alpha_16x8, CFG_LVGL_ASSET_STIME);
DEFINE_EXTFLASH_IMAGE(_Sfen_alpha_21x6, CFG_LVGL_ASSET_SFEN);
DEFINE_EXTFLASH_IMAGE(_power_hight_alpha_32x32, CFG_LVGL_ASSET_POWER_HIGHT);
DEFINE_EXTFLASH_IMAGE(_location_alpha_32x32, CFG_LVGL_ASSET_LOCATION);
DEFINE_EXTFLASH_IMAGE(_taiwan_alpha_32x32, CFG_LVGL_ASSET_TAIWAN);
DEFINE_EXTFLASH_IMAGE(_nfc_alpha_32x32, CFG_LVGL_ASSET_NFC);
DEFINE_EXTFLASH_IMAGE(_liangdu_47x47, CFG_LVGL_ASSET_LIANGDU);
DEFINE_EXTFLASH_IMAGE(_ZNZBG_alpha_100x100, CFG_LVGL_ASSET_ZNZBG);
DEFINE_EXTFLASH_IMAGE(_arw_alpha_50x40, CFG_LVGL_ASSET_ARW);
DEFINE_EXTFLASH_IMAGE(_ZNZ_alpha_50x50, CFG_LVGL_ASSET_ZNZ);
DEFINE_EXTFLASH_IMAGE(_heart32x32_alpha_32x32, CFG_LVGL_ASSET_HEART32X32);
DEFINE_EXTFLASH_IMAGE(_tiwen_alpha_32x32, CFG_LVGL_ASSET_TIWEN);
DEFINE_EXTFLASH_IMAGE(_pa_alpha_32x32, CFG_LVGL_ASSET_PA);
DEFINE_EXTFLASH_IMAGE(_location32x32_alpha_32x32,
                      CFG_LVGL_ASSET_LOCATION32X32);
//******************************* Variables *********************************//

//******************************* Functions *********************************//
/**
 * @brief Sequentially read the three pointer sprites from external flash
 *        into their RAM mirrors.
 */
static ext_flash_status_t lvgl_assets_load_to_ram(void)
{
    ext_flash_status_t st;

    st = Read_LvglData(CFG_LVGL_ASSET_FEN_OFFSET,
                       CFG_LVGL_ASSET_FEN_SIZE,
                       s_au8_fen_ram);
    if (EXT_FLASH_OK != st)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG, "load fen failed st=%d", (int)st);
        return st;
    }

    st = Read_LvglData(CFG_LVGL_ASSET_MIAO_OFFSET,
                       CFG_LVGL_ASSET_MIAO_SIZE,
                       s_au8_miao_ram);
    if (EXT_FLASH_OK != st)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG, "load miao failed st=%d", (int)st);
        return st;
    }

    st = Read_LvglData(CFG_LVGL_ASSET_TIME_OFFSET,
                       CFG_LVGL_ASSET_TIME_SIZE,
                       s_au8_time_ram);
    if (EXT_FLASH_OK != st)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG, "load time failed st=%d", (int)st);
        return st;
    }

    return EXT_FLASH_OK;
}

ext_flash_status_t storage_assets_bootstrap(void)
{
    uint32_t            magic = 0U;
    ext_flash_status_t  st;

    DEBUG_OUT(i, W25Q64_LOG_TAG, "assets bootstrap: probing magic");

    st = Read_LvglData(CFG_LVGL_ASSET_MAGIC_OFFSET,
                       CFG_LVGL_ASSET_MAGIC_SIZE,
                       (uint8_t *)&magic);
    if (EXT_FLASH_OK != st)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "assets bootstrap: read magic failed st=%d", (int)st);
        return st;
    }

    if (CFG_LVGL_ASSET_MAGIC == magic)
    {
        DEBUG_OUT(i, W25Q64_LOG_TAG,
                  "assets bootstrap: magic ok (0x%08X), skipping write",
                  (unsigned int)magic);
    }
    else
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "assets bootstrap: magic mismatch (0x%08X), flash assets.bin",
                  (unsigned int)magic);
        return EXT_FLASH_ERRORRESOURCE;
    }

    st = lvgl_assets_load_to_ram();
    if (EXT_FLASH_OK != st)
    {
        return st;
    }

    DEBUG_OUT(i, W25Q64_LOG_TAG, "assets bootstrap: ram mirrors filled");
    return EXT_FLASH_OK;
}
//******************************* Functions *********************************//
