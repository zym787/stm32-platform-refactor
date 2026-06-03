/******************************************************************************
 * @file lv_port_extfont.c
 *
 * @par dependencies
 * - lv_port_extfont.h
 * - cfg_storage.h
 * - service_storage_facade.h
 * - lv_font_fmt_txt.h
 *
 * @author Ethan-Hang
 *
 * @brief W25Q64-backed LVGL font bitmap callbacks.
 *
 * @version V1.0 2026-6-3
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

//******************************** Includes *********************************//
#include "lv_port_extfont.h"

#include "cfg_storage.h"
#include "service_storage_facade.h"
#include "Debug.h"

#include "lvgl.h"
#include "src/font/lv_font_fmt_txt.h"

#include <stdint.h>
#include <stddef.h>
//******************************** Includes *********************************//

//******************************* Variables *********************************//
static uint8_t s_au8GlyphBuf[CFG_LVGL_FONT_GLYPH_BUFFER_SIZE];
//******************************* Variables *********************************//

//******************************* Functions *********************************//
/**
 * @brief Locate a value in a sorted uint16_t list.
 *
 * @param[in] : list Sorted uint16_t list.
 * @param[in] : listLen Number of entries in list.
 * @param[in] : value Value to find.
 * @param[out] : indexOut Matching index when the function returns true.
 *
 * @return true when found, otherwise false.
 * */
static bool extfont_find_u16(const uint16_t *list,
                             uint16_t       listLen,
                             uint16_t       value,
                             uint16_t      *indexOut)
{
    uint16_t index;

    if ((NULL == list) || (NULL == indexOut))
    {
        return false;
    }

    for (index = 0U; index < listLen; index++)
    {
        if (value == list[index])
        {
            *indexOut = index;
            return true;
        }
    }

    return false;
}

/**
 * @brief Resolve a Unicode code point to an LVGL glyph descriptor index.
 *
 * @param[in] : font Pointer to the LVGL font descriptor.
 * @param[in] : letter Unicode code point requested by LVGL.
 *
 * @return LVGL glyph ID, or 0 when not found.
 * */
static uint32_t extfont_get_glyph_id(const lv_font_t *font, uint32_t letter)
{
    const lv_font_fmt_txt_dsc_t *fdsc;
    uint16_t cmapIndex;

    if ((NULL == font) || (NULL == font->dsc) || ('\0' == letter))
    {
        return 0U;
    }

    if ('\t' == letter)
    {
        letter = ' ';
    }

    fdsc = (const lv_font_fmt_txt_dsc_t *)font->dsc;
    for (cmapIndex = 0U; cmapIndex < fdsc->cmap_num; cmapIndex++)
    {
        const lv_font_fmt_txt_cmap_t *cmap = &fdsc->cmaps[cmapIndex];
        uint32_t rcp;
        uint32_t glyphId = 0U;

        if (letter < cmap->range_start)
        {
            continue;
        }

        rcp = letter - cmap->range_start;
        if (rcp >= cmap->range_length)
        {
            continue;
        }

        if (LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY == cmap->type)
        {
            glyphId = (uint32_t)cmap->glyph_id_start + rcp;
        }
        else if (LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL == cmap->type)
        {
            const uint8_t *glyphOfs = (const uint8_t *)cmap->glyph_id_ofs_list;

            if (NULL != glyphOfs)
            {
                glyphId = (uint32_t)cmap->glyph_id_start + glyphOfs[rcp];
            }
        }
        else
        {
            uint16_t listIndex = 0U;

            if ((rcp <= UINT16_MAX) &&
                extfont_find_u16(cmap->unicode_list,
                                 cmap->list_length,
                                 (uint16_t)rcp,
                                 &listIndex))
            {
                if (LV_FONT_FMT_TXT_CMAP_SPARSE_TINY == cmap->type)
                {
                    glyphId = (uint32_t)cmap->glyph_id_start + listIndex;
                }
                else if (LV_FONT_FMT_TXT_CMAP_SPARSE_FULL == cmap->type)
                {
                    const uint16_t *glyphOfs =
                        (const uint16_t *)cmap->glyph_id_ofs_list;

                    if (NULL != glyphOfs)
                    {
                        glyphId = (uint32_t)cmap->glyph_id_start +
                                  glyphOfs[listIndex];
                    }
                }
            }
        }

        if ((NULL != fdsc->cache) && (0U != glyphId))
        {
            fdsc->cache->last_letter = letter;
            fdsc->cache->last_glyph_id = glyphId;
        }

        return glyphId;
    }

    return 0U;
}

/**
 * @brief Compute one uncompressed LVGL glyph bitmap byte count.
 *
 * @param[in] : gdsc Pointer to LVGL glyph descriptor.
 * @param[in] : bpp Font bits per pixel.
 *
 * @return Number of bytes needed for this glyph bitmap.
 * */
static uint32_t extfont_get_glyph_size(
    const lv_font_fmt_txt_glyph_dsc_t *gdsc,
    uint8_t                            bpp)
{
    uint32_t bitCount;

    if ((NULL == gdsc) || (0U == gdsc->box_w) || (0U == gdsc->box_h))
    {
        return 0U;
    }

    bitCount = (uint32_t)gdsc->box_w * gdsc->box_h * bpp;
    return (bitCount + 7U) / 8U;
}

/**
 * @brief Common W25Q64 glyph bitmap reader.
 *
 * @param[in] : font Pointer to the LVGL font descriptor.
 * @param[in] : letter Unicode code point requested by LVGL.
 * @param[in] : bitmapOffset Font bitmap base offset within LVGL partition.
 * @param[in] : bitmapSize Total external bitmap payload size.
 *
 * @return Pointer to a transient glyph bitmap buffer, or NULL on failure.
 * */
static const uint8_t *extfont_get_bitmap(const lv_font_t *font,
                                         uint32_t         letter,
                                         uint32_t         bitmapOffset,
                                         uint32_t         bitmapSize)
{
    const lv_font_fmt_txt_dsc_t       *fdsc;
    const lv_font_fmt_txt_glyph_dsc_t *gdsc;
    uint32_t                           glyphId;
    uint32_t                           glyphSize;
    ext_flash_status_t                 st;

    if ((NULL == font) || (NULL == font->dsc))
    {
        return NULL;
    }

    fdsc = (const lv_font_fmt_txt_dsc_t *)font->dsc;
    if (LV_FONT_FMT_TXT_PLAIN != fdsc->bitmap_format)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG, "extfont compressed font unsupported");
        return NULL;
    }

    glyphId = extfont_get_glyph_id(font, letter);
    if (0U == glyphId)
    {
        return NULL;
    }

    gdsc = &fdsc->glyph_dsc[glyphId];
    glyphSize = extfont_get_glyph_size(gdsc, (uint8_t)fdsc->bpp);
    if (0U == glyphSize)
    {
        return NULL;
    }

    if ((glyphSize > CFG_LVGL_FONT_GLYPH_BUFFER_SIZE) ||
        (gdsc->bitmap_index > bitmapSize) ||
        (glyphSize > (bitmapSize - gdsc->bitmap_index)))
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "extfont glyph out of range idx=%lu size=%lu",
                  (unsigned long)gdsc->bitmap_index,
                  (unsigned long)glyphSize);
        return NULL;
    }

    st = Read_LvglData(bitmapOffset + gdsc->bitmap_index,
                       glyphSize,
                       s_au8GlyphBuf);
    if (EXT_FLASH_OK != st)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "extfont read failed st=%d", (int)st);
        return NULL;
    }

    return s_au8GlyphBuf;
}

#define LV_EXTFONT_DEFINE(fontName, offsetMacro, sizeMacro)                  \
    const uint8_t *lv_port_extfont_get_bitmap_##fontName(                   \
        const lv_font_t *font,                                               \
        uint32_t         letter)                                             \
    {                                                                        \
        return extfont_get_bitmap(font, letter, offsetMacro, sizeMacro);     \
    }

LV_EXTFONT_DEFINE(lv_font_interttf_24,
                  CFG_LVGL_FONT_INTERTTF_24_BITMAP_OFFSET,
                  CFG_LVGL_FONT_INTERTTF_24_BITMAP_SIZE)
LV_EXTFONT_DEFINE(lv_font_interttf_10,
                  CFG_LVGL_FONT_INTERTTF_10_BITMAP_OFFSET,
                  CFG_LVGL_FONT_INTERTTF_10_BITMAP_SIZE)
LV_EXTFONT_DEFINE(lv_font_interttf_82,
                  CFG_LVGL_FONT_INTERTTF_82_BITMAP_OFFSET,
                  CFG_LVGL_FONT_INTERTTF_82_BITMAP_SIZE)
LV_EXTFONT_DEFINE(lv_font_alimama_16,
                  CFG_LVGL_FONT_ALIMAMA_16_BITMAP_OFFSET,
                  CFG_LVGL_FONT_ALIMAMA_16_BITMAP_SIZE)
LV_EXTFONT_DEFINE(lv_font_alimama_36,
                  CFG_LVGL_FONT_ALIMAMA_36_BITMAP_OFFSET,
                  CFG_LVGL_FONT_ALIMAMA_36_BITMAP_SIZE)
LV_EXTFONT_DEFINE(lv_font_digitaldreamfatnarrow_36,
                  CFG_LVGL_FONT_DIGITALDREAMFATNARROW_36_BITMAP_OFFSET,
                  CFG_LVGL_FONT_DIGITALDREAMFATNARROW_36_BITMAP_SIZE)
LV_EXTFONT_DEFINE(lv_font_alimama_12,
                  CFG_LVGL_FONT_ALIMAMA_12_BITMAP_OFFSET,
                  CFG_LVGL_FONT_ALIMAMA_12_BITMAP_SIZE)
LV_EXTFONT_DEFINE(lv_font_alimama_10,
                  CFG_LVGL_FONT_ALIMAMA_10_BITMAP_OFFSET,
                  CFG_LVGL_FONT_ALIMAMA_10_BITMAP_SIZE)
//******************************* Functions *********************************//
