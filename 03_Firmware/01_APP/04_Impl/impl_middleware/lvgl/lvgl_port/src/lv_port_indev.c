/******************************************************************************
 * @file lv_port_indev.c
 *
 * @par dependencies
 * - lvgl.h
 * - bsp_wrapper_touch.h
 *
 * @author Ethan-Hang
 *
 * @brief LVGL pointer indev port over the bsp_wrapper_touch abstraction.
 *
 * Processing flow:
 *   LVGL polls indev_read_cb every LV_INDEV_DEF_READ_PERIOD ms (10 ms by
 *   default) -> we call touch_get_finger_num / touch_get_xy on the wrapper
 *   and translate the result to lv_indev_data_t {state, point}.  Gestures
 *   are not consumed here — the app uses LVGL's own click / long-press /
 *   swipe events on top.
 *
 * @version V1.0 2026-04-25
 * @version V2.0 2026-04-26
 * @upgrade 2.0: Removed the direct bsp_cst816t_driver_t coupling; reads now
 *               flow through bsp_wrapper_touch.
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "lv_port_indev.h"

#include <stddef.h>

#include "lvgl.h"
#include "bsp_wrapper_touch.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define LV_PORT_INDEV_PANEL_WIDTH       240u
#define LV_PORT_INDEV_PANEL_HEIGHT      280u
//******************************** Defines **********************************//

//******************************* Declaring *********************************//
static lv_indev_drv_t         s_indev_drv;
static lv_point_t             s_last_point = {0, 0};
//******************************* Declaring *********************************//

//******************************* Functions *********************************//
/**
 * @brief      LVGL indev read callback.  Polled every 10 ms by lv_timer.
 *
 * @param[in]  drv  : Indev driver pointer (unused).
 *
 * @param[out] data : Output state + coordinates LVGL feeds into widgets.
 *
 * @return     None.
 *
 * @note       Holds the last reported coordinate while the finger is up so
 *             LVGL receives a clean PRESSED -> RELEASED transition at the
 *             same point, which makes click / long-press recognition work.
 * */
static void lv_port_indev_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    /**
     * Default to RELEASED + last point so any wrapper failure produces a
     * clean lift-off rather than a stuck press.
     **/
    data->state = LV_INDEV_STATE_RELEASED;
    data->point = s_last_point;

    uint8_t finger_num = 0u;
    wp_touch_status_t st = touch_get_finger_num(&finger_num);
    if (WP_TOUCH_OK != st)
    {
        return;
    }
    if (0u == finger_num)
    {
        return;
    }

    uint16_t x_pos = 0u;
    uint16_t y_pos = 0u;
    st = touch_get_xy(&x_pos, &y_pos);
    if (WP_TOUCH_OK != st)
    {
        return;
    }

    /**
     * Clamp to panel rectangle so a stray off-edge sample (controllers can
     * occasionally report values just past the active area) does not land
     * inside an invalid widget hit-test.
     **/
    if (x_pos >= LV_PORT_INDEV_PANEL_WIDTH)
    {
        x_pos = (uint16_t)(LV_PORT_INDEV_PANEL_WIDTH - 1u);
    }
    if (y_pos >= LV_PORT_INDEV_PANEL_HEIGHT)
    {
        y_pos = (uint16_t)(LV_PORT_INDEV_PANEL_HEIGHT - 1u);
    }

    s_last_point.x = (lv_coord_t)x_pos;
    s_last_point.y = (lv_coord_t)y_pos;
    data->state    = LV_INDEV_STATE_PRESSED;
    data->point    = s_last_point;
}

bool lv_port_indev_init(void)
{
    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = lv_port_indev_read_cb;

    lv_indev_t *indev = lv_indev_drv_register(&s_indev_drv);
    return (NULL != indev);
}
//******************************* Functions *********************************//
