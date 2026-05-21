/******************************************************************************
 * @file lv_port_disp.c
 *
 * @par dependencies
 * - lvgl.h
 * - bsp_wrapper_display.h
 *
 * @author Ethan-Hang
 *
 * @brief LVGL display port over the bsp_wrapper_display abstraction.
 *
 * Processing flow:
 *   lv_disp_drv_t.flush_cb -> display_draw_image() -> driver-specific impl.
 *
 * A single partial draw buffer (one panel-width x 20 lines of RGB565) sits
 * in .bss.  When LVGL asks for a flush, we forward the rectangle straight
 * into the wrapper API; the underlying driver handles RGB565 byte-swap and
 * DMA, so lv_color_t (host-endian RGB565) can be passed through as a plain
 * uint16_t* without further conversion.
 *
 * @version V1.0 2026-04-24
 * @version V2.0 2026-04-26
 * @upgrade 2.0: Routed flush callback through bsp_wrapper_display so the
 *               LVGL port no longer depends on the ST7789 driver type.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "lv_port_disp.h"

#include <stddef.h>
#include <string.h>

#include "lvgl.h"
#include "bsp_wrapper_display.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define LV_PORT_DISP_HOR_RES        240U
#define LV_PORT_DISP_VER_RES        284U
#define LV_PORT_DISP_BUF_LINES      20U
#define LV_PORT_DISP_BUF_PIXELS     (LV_PORT_DISP_HOR_RES * LV_PORT_DISP_BUF_LINES)
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static lv_disp_draw_buf_t    s_draw_buf;
static lv_disp_drv_t         s_disp_drv;
static lv_color_t            s_buf1[LV_PORT_DISP_BUF_PIXELS];
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief LVGL flush callback.  Forwards the dirty rectangle to the
 *        display wrapper; lv_color_t shares storage layout with the
 *        wrapper's uint16_t RGB565 buffer so the cast is well-defined.
 *
 * @param[in] disp_drv  : LVGL display driver context.
 * @param[in] area      : Dirty rectangle in screen coordinates.
 * @param[in] color_p   : Pixel buffer for that rectangle.
 */
static void lv_port_flush_cb(lv_disp_drv_t *  disp_drv,
                             const lv_area_t *area,
                             lv_color_t *     color_p)
{
    const uint16_t x      = (uint16_t)area->x1;
    const uint16_t y      = (uint16_t)area->y1;
    const uint16_t width  = (uint16_t)(area->x2 - area->x1 + 1);
    const uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);

    /**
     * lv_color_t is a 16-bit RGB565 union whose .full matches the host-endian
     * uint16_t the wrapper expects; the cast is layout-compatible.
     **/
    (void)display_draw_image(x, y, width, height, (uint16_t const *)color_p);

    lv_disp_flush_ready(disp_drv);
}

bool lv_port_disp_init(void)
{
    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, NULL, LV_PORT_DISP_BUF_PIXELS);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res  = LV_PORT_DISP_HOR_RES;
    s_disp_drv.ver_res  = LV_PORT_DISP_VER_RES;
    s_disp_drv.flush_cb = lv_port_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;

    lv_disp_t *disp = lv_disp_drv_register(&s_disp_drv);
    return (NULL != disp);
}
//******************************* Functions *********************************//
