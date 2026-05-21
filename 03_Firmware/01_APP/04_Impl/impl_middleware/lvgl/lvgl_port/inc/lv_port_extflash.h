/******************************************************************************
 * @file lv_port_extflash.h
 *
 * @par dependencies
 * - lvgl.h
 * - stdint.h
 *
 * @author Ethan-Hang
 *
 * @brief Custom LVGL image decoder that streams pixel data from the W25Q64
 *        external flash one scanline at a time.
 *
 *        Why: a 240x240 alpha background image is 169 KB and does not fit
 *        in MCU RAM, while keeping it in firmware .rodata costs the same
 *        Flash that the W25Q64 storage stack was built to free.  This
 *        decoder is the bridge: an `lv_img_dsc_t` whose `.data` points at
 *        an `lv_extflash_meta_t` (carrying a magic + LVGL sub-region
 *        offset + geometry) is recognised by the decoder, which then
 *        services LVGL's per-line render requests by calling
 *        Read_LvglData() on the storage manager.
 *
 *        Performance note: each line round-trips through OSAL primitives
 *        (storage_manager_task event group + binary semaphore) and an
 *        SPI2 transfer (~720 B at 8 MHz).  Roughly 1 ms / line; full 240x
 *        240 repaint ≈ 240 ms.  Acceptable for the static analog-clock
 *        background since LVGL only paints dirty regions covered by the
 *        moving needles.
 *
 * @version V1.0 2026-05-08
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

#pragma once
#ifndef __LV_PORT_EXTFLASH_H__
#define __LV_PORT_EXTFLASH_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include "lvgl.h"
//******************************** Includes *********************************//

#ifdef __cplusplus
extern "C" {
#endif

//******************************** Defines **********************************//
/**
 * @brief Sentinel placed in the first field of `lv_extflash_meta_t`.  The
 *        decoder uses it to identify "its" `lv_img_dsc_t` instances among
 *        all images LVGL hands it during the info_cb scan.
 */
#define LV_EXTFLASH_DECODER_MAGIC   (0xE73F1A57UL)
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
/**
 * @brief Metadata stashed inside the `.data` pointer of an `lv_img_dsc_t`
 *        whose pixel data lives on the W25Q64 LVGL sub-region.
 *
 *        Cast convention: `meta = (const lv_extflash_meta_t *)dsc->data`.
 *        `magic == LV_EXTFLASH_DECODER_MAGIC` claims the image for this
 *        decoder; any other value lets LVGL fall through to the next
 *        registered decoder (the built-in true-color path).
 */
typedef struct
{
    uint32_t  magic;       /**< Must equal LV_EXTFLASH_DECODER_MAGIC.        */
    uint32_t  ext_offset;  /**< Byte offset within the LVGL sub-region.      */
    uint16_t  width;       /**< Pixel columns.                               */
    uint16_t  height;      /**< Pixel rows.                                  */
    uint8_t   px_size;     /**< Bytes per pixel (3 for RGB565+alpha).        */
    uint8_t   reserved[3]; /**< Padding for natural alignment.               */
} lv_extflash_meta_t;
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief Register the external-flash line-streaming decoder with LVGL.
 *
 *        Must be called AFTER lv_init() and BEFORE the first image whose
 *        descriptor uses an `lv_extflash_meta_t` payload is rendered.  The
 *        gui_guider task does this between lv_init() and setup_ui().
 */
void lv_port_extflash_init(void);
//******************************* Functions *********************************//

#ifdef __cplusplus
}
#endif

#endif /* __LV_PORT_EXTFLASH_H__ */
