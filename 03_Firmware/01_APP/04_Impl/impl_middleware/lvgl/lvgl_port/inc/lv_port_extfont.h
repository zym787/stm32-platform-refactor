/******************************************************************************
 * @file lv_port_extfont.h
 *
 * @par dependencies
 * - lvgl.h
 * - stdint.h
 *
 * @author Ethan-Hang
 *
 * @brief LVGL font bitmap callbacks backed by the W25Q64 LVGL partition.
 *
 * @version V1.0 2026-6-3
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#pragma once
#ifndef __LV_PORT_EXTFONT_H__
#define __LV_PORT_EXTFONT_H__

//******************************** Includes *********************************//
#include <stdint.h>
#include "lvgl.h"
//******************************** Includes *********************************//

#ifdef __cplusplus
extern "C" {
#endif

//******************************* Functions *********************************//
/**
 * @brief Read one glyph bitmap for lv_font_interttf_24 from W25Q64.
 *
 * @param[in] : font Pointer to the LVGL font descriptor.
 * @param[in] : letter Unicode code point requested by LVGL.
 *
 * @return Pointer to a transient glyph bitmap buffer, or NULL on failure.
 * */
const uint8_t *lv_port_extfont_get_bitmap_lv_font_interttf_24(
    const lv_font_t *font,
    uint32_t letter);

/**
 * @brief Read one glyph bitmap for lv_font_interttf_10 from W25Q64.
 *
 * @param[in] : font Pointer to the LVGL font descriptor.
 * @param[in] : letter Unicode code point requested by LVGL.
 *
 * @return Pointer to a transient glyph bitmap buffer, or NULL on failure.
 * */
const uint8_t *lv_port_extfont_get_bitmap_lv_font_interttf_10(
    const lv_font_t *font,
    uint32_t letter);

/**
 * @brief Read one glyph bitmap for lv_font_interttf_82 from W25Q64.
 *
 * @param[in] : font Pointer to the LVGL font descriptor.
 * @param[in] : letter Unicode code point requested by LVGL.
 *
 * @return Pointer to a transient glyph bitmap buffer, or NULL on failure.
 * */
const uint8_t *lv_port_extfont_get_bitmap_lv_font_interttf_82(
    const lv_font_t *font,
    uint32_t letter);

/**
 * @brief Read one glyph bitmap for lv_font_alimama_16 from W25Q64.
 *
 * @param[in] : font Pointer to the LVGL font descriptor.
 * @param[in] : letter Unicode code point requested by LVGL.
 *
 * @return Pointer to a transient glyph bitmap buffer, or NULL on failure.
 * */
const uint8_t *lv_port_extfont_get_bitmap_lv_font_alimama_16(
    const lv_font_t *font,
    uint32_t letter);

/**
 * @brief Read one glyph bitmap for lv_font_alimama_36 from W25Q64.
 *
 * @param[in] : font Pointer to the LVGL font descriptor.
 * @param[in] : letter Unicode code point requested by LVGL.
 *
 * @return Pointer to a transient glyph bitmap buffer, or NULL on failure.
 * */
const uint8_t *lv_port_extfont_get_bitmap_lv_font_alimama_36(
    const lv_font_t *font,
    uint32_t letter);

/**
 * @brief Read one glyph bitmap for lv_font_digitaldreamfatnarrow_36 from
 *        W25Q64.
 *
 * @param[in] : font Pointer to the LVGL font descriptor.
 * @param[in] : letter Unicode code point requested by LVGL.
 *
 * @return Pointer to a transient glyph bitmap buffer, or NULL on failure.
 * */
const uint8_t *lv_port_extfont_get_bitmap_lv_font_digitaldreamfatnarrow_36(
    const lv_font_t *font,
    uint32_t letter);

/**
 * @brief Read one glyph bitmap for lv_font_alimama_12 from W25Q64.
 *
 * @param[in] : font Pointer to the LVGL font descriptor.
 * @param[in] : letter Unicode code point requested by LVGL.
 *
 * @return Pointer to a transient glyph bitmap buffer, or NULL on failure.
 * */
const uint8_t *lv_port_extfont_get_bitmap_lv_font_alimama_12(
    const lv_font_t *font,
    uint32_t letter);

/**
 * @brief Read one glyph bitmap for lv_font_alimama_10 from W25Q64.
 *
 * @param[in] : font Pointer to the LVGL font descriptor.
 * @param[in] : letter Unicode code point requested by LVGL.
 *
 * @return Pointer to a transient glyph bitmap buffer, or NULL on failure.
 * */
const uint8_t *lv_port_extfont_get_bitmap_lv_font_alimama_10(
    const lv_font_t *font,
    uint32_t letter);
//******************************* Functions *********************************//

#ifdef __cplusplus
}
#endif

#endif /* __LV_PORT_EXTFONT_H__ */
