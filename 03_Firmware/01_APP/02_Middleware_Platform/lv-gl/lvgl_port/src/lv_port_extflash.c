/******************************************************************************
 * @file lv_port_extflash.c
 *
 * @par dependencies
 * - lv_port_extflash.h
 * - service_storage_facade.h
 * - lvgl.h
 *
 * @author Ethan-Hang
 *
 * @brief Implementation of the W25Q64-backed LVGL image decoder.  Hands
 *        each scanline request through to Read_LvglData() (which routes
 *        the read into the storage_manager_task and blocks on a binary
 *        semaphore until the BSP handler thread completes the SPI2 read).
 *
 * Processing flow (per LVGL render of an image whose data points at an
 * lv_extflash_meta_t):
 *
 *   info_cb       -> magic check, hand back width/height/cf
 *   open_cb       -> stash meta in dsc->user_data, set img_data=NULL so
 *                    LVGL switches to per-line streaming
 *   read_line_cb  -> compute (y * width + x) byte offset, blocking read
 *                    `len * px_size` bytes into `buf`
 *   close_cb      -> drop the user_data pointer
 *
 * @version V1.0 2026-05-08
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "lv_port_extflash.h"

#include "service_storage_facade.h"
#include "Debug.h"

#include "lvgl.h"

#include <stdint.h>
#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines **********************************//

//******************************** Defines **********************************//

//******************************* Functions *********************************//
/**
 * @brief Try to recognise an `lv_img_dsc_t` whose `.data` carries an
 *        `lv_extflash_meta_t`.  Returns LV_RES_INV (not LV_RES_OK) for any
 *        descriptor that doesn't carry our magic so LVGL falls through to
 *        the next decoder in the chain.
 */
static lv_res_t lv_extflash_info_cb(lv_img_decoder_t *decoder,
                                    const void       *src,
                                    lv_img_header_t  *header)
{
    LV_UNUSED(decoder);

    if (LV_IMG_SRC_VARIABLE != lv_img_src_get_type(src))
    {
        return LV_RES_INV;
    }

    const lv_img_dsc_t *dsc = (const lv_img_dsc_t *)src;
    if (NULL == dsc->data)
    {
        return LV_RES_INV;
    }

    const lv_extflash_meta_t *meta = (const lv_extflash_meta_t *)dsc->data;
    if (LV_EXTFLASH_DECODER_MAGIC != meta->magic)
    {
        return LV_RES_INV;
    }

    header->w           = meta->width;
    header->h           = meta->height;
    header->cf          = dsc->header.cf;
    header->always_zero = 0;
    header->reserved    = 0;
    return LV_RES_OK;
}

/**
 * @brief Set up a decode session.  Setting `img_data` to NULL is the
 *        documented LVGL signal for "decoder will provide data line by
 *        line via read_line_cb".
 */
static lv_res_t lv_extflash_open_cb(lv_img_decoder_t      *decoder,
                                    lv_img_decoder_dsc_t  *dsc)
{
    LV_UNUSED(decoder);

    const lv_img_dsc_t       *img_dsc = (const lv_img_dsc_t *)dsc->src;
    const lv_extflash_meta_t *meta    =
        (const lv_extflash_meta_t *)img_dsc->data;
    if (LV_EXTFLASH_DECODER_MAGIC != meta->magic)
    {
        return LV_RES_INV;
    }

    /* Cast away const for storage in user_data; read_line_cb only reads. */
    dsc->user_data = (void *)(uintptr_t)meta;
    dsc->img_data  = NULL;
    return LV_RES_OK;
}

/**
 * @brief Service a per-line read.  All offsets here are LVGL-sub-region
 *        relative; storage_manager_task adds MEMORY_LVGL_START_ADDRESS
 *        when issuing the actual driver call.
 */
static lv_res_t lv_extflash_read_line_cb(lv_img_decoder_t      *decoder,
                                         lv_img_decoder_dsc_t  *dsc,
                                         lv_coord_t             x,
                                         lv_coord_t             y,
                                         lv_coord_t             len,
                                         uint8_t               *buf)
{
    LV_UNUSED(decoder);

    const lv_extflash_meta_t *meta =
        (const lv_extflash_meta_t *)dsc->user_data;
    if (NULL == meta)
    {
        return LV_RES_INV;
    }

    uint32_t bytes_per_line = (uint32_t)meta->width * meta->px_size;
    uint32_t row_offset     = (uint32_t)y * bytes_per_line
                            + (uint32_t)x * meta->px_size;
    uint32_t bytes          = (uint32_t)len * meta->px_size;

    ext_flash_status_t st = Read_LvglData(meta->ext_offset + row_offset,
                                          bytes, buf);
    if (EXT_FLASH_OK != st)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG,
                  "extflash read_line failed st=%d y=%d", (int)st, (int)y);
        return LV_RES_INV;
    }
    return LV_RES_OK;
}

static void lv_extflash_close_cb(lv_img_decoder_t     *decoder,
                                 lv_img_decoder_dsc_t *dsc)
{
    LV_UNUSED(decoder);
    dsc->user_data = NULL;
}

void lv_port_extflash_init(void)
{
    lv_img_decoder_t *d = lv_img_decoder_create();
    if (NULL == d)
    {
        DEBUG_OUT(e, W25Q64_ERR_LOG_TAG, "lv_img_decoder_create failed");
        return;
    }
    lv_img_decoder_set_info_cb     (d, lv_extflash_info_cb);
    lv_img_decoder_set_open_cb     (d, lv_extflash_open_cb);
    lv_img_decoder_set_read_line_cb(d, lv_extflash_read_line_cb);
    lv_img_decoder_set_close_cb    (d, lv_extflash_close_cb);
}
//******************************* Functions *********************************//
