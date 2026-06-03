/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "events_init.h"
#include <stdio.h>
#include "lvgl.h"

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
#include "freemaster_client.h"
#endif

bool Clock_btn_1_is_click=0;
bool Clock_btn_2_is_click=0;
bool Clock_btn_3_is_click=0;
bool Clock1_btn_1_is_click=0;
bool Clock1_btn_2_is_click=0;
bool Clock1_btn_3_is_click=0;
bool Clock2_btn_1_is_click=0;
bool Clock2_btn_2_is_click=0;
bool Clock2_btn_3_is_click=0;
uint8_t screen_index=0;

static void Clock_1_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_LONG_PRESSED:
    {
        ui_animation(guider_ui.Clock_1_cont_2, 200, 0, lv_obj_get_x(guider_ui.Clock_1_cont_2), 120, &lv_anim_path_overshoot, 1, 0, 0, 0, (lv_anim_exec_xcb_t)lv_obj_set_x, NULL, NULL, NULL);
        lv_obj_clear_flag(guider_ui.Clock_1_cont_2, LV_OBJ_FLAG_HIDDEN);
        ui_animation(guider_ui.Clock_1_cont_1, 200, 0, lv_obj_get_x(guider_ui.Clock_1_cont_1), 0, &lv_anim_path_overshoot, 1, 0, 0, 0, (lv_anim_exec_xcb_t)lv_obj_set_x, NULL, NULL, NULL);
        break;
    }
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
        case LV_DIR_RIGHT:
        {
            lv_indev_wait_release(lv_indev_get_act());
            ui_load_scr_animation(&guider_ui, &guider_ui.Clock_2, guider_ui.Clock_2_del, &guider_ui.Clock_1_del, setup_scr_Clock_2, LV_SCR_LOAD_ANIM_OVER_RIGHT, 200, 0, true, true);
            screen_index=1;
            break;
        }
        case LV_DIR_BOTTOM:
        {
            lv_indev_wait_release(lv_indev_get_act());
            ui_load_scr_animation(&guider_ui, &guider_ui.top_lap, guider_ui.top_lap_del, &guider_ui.Clock_1_del, setup_scr_top_lap, LV_SCR_LOAD_ANIM_OVER_BOTTOM, 200, 200, true, true);
            break;
        }
        case LV_DIR_TOP:
        {
            lv_indev_wait_release(lv_indev_get_act());
            ui_load_scr_animation(&guider_ui, &guider_ui.under_up, guider_ui.under_up_del, &guider_ui.Clock_1_del, setup_scr_under_up, LV_SCR_LOAD_ANIM_OVER_TOP, 200, 200, true, true);
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void Clock_1_btn_1_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock_btn_1_is_click) {
            Clock_btn_1_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_6, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_1, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock_btn_1_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_6, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_1, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_1_btn_2_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock_btn_2_is_click) {
            Clock_btn_2_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_7, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_2, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock_btn_2_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_7, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_7, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_2, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_1_btn_3_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock_btn_3_is_click) {
            Clock_btn_3_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_8, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_3, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock_btn_3_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_8, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_8, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_3, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_1_img_6_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock_btn_1_is_click) {
            Clock_btn_1_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_6, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_1, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock_btn_1_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_6, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_1, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_1_img_7_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock_btn_2_is_click) {
            Clock_btn_2_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_7, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_2, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock_btn_2_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_7, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_7, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_2, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_1_img_8_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock_btn_3_is_click) {
            Clock_btn_3_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_8, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_3, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock_btn_3_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_1_img_8, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_1_img_8, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_1_btn_3, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_1_cont_2_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_LONG_PRESSED:
    {
        lv_obj_add_flag(guider_ui.Clock_1_cont_2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(guider_ui.Clock_1_cont_2, 0);
        ui_animation(guider_ui.Clock_1_cont_1, 200, 0, lv_obj_get_x(guider_ui.Clock_1_cont_1), -140, &lv_anim_path_overshoot, 1, 0, 0, 0, (lv_anim_exec_xcb_t)lv_obj_set_x, NULL, NULL, NULL);
        break;
    }
    default:
        break;
    }
}

void events_init_Clock_1 (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->Clock_1, Clock_1_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_1_btn_1, Clock_1_btn_1_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_1_btn_2, Clock_1_btn_2_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_1_btn_3, Clock_1_btn_3_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_1_img_6, Clock_1_img_6_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_1_img_7, Clock_1_img_7_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_1_img_8, Clock_1_img_8_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_1_cont_2, Clock_1_cont_2_event_handler, LV_EVENT_ALL, ui);
}

static void Clock_2_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_LONG_PRESSED:
    {
        ui_animation(guider_ui.Clock_2_cont_2, 200, 0, lv_obj_get_x(guider_ui.Clock_2_cont_2), 120, &lv_anim_path_overshoot, 1, 0, 0, 0, (lv_anim_exec_xcb_t)lv_obj_set_x, NULL, NULL, NULL);
        lv_obj_clear_flag(guider_ui.Clock_2_cont_2, LV_OBJ_FLAG_HIDDEN);
        ui_animation(guider_ui.Clock_2_cont_1, 200, 0, lv_obj_get_x(guider_ui.Clock_2_cont_1), 0, &lv_anim_path_overshoot, 1, 0, 0, 0, (lv_anim_exec_xcb_t)lv_obj_set_x, NULL, NULL, NULL);
        break;
    }
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
        case LV_DIR_RIGHT:
        {
            lv_indev_wait_release(lv_indev_get_act());
            ui_load_scr_animation(&guider_ui, &guider_ui.Clock_3, guider_ui.Clock_3_del, &guider_ui.Clock_2_del, setup_scr_Clock_3, LV_SCR_LOAD_ANIM_OVER_RIGHT, 200, 0, true, true);
            screen_index=2;
            break;
        }
        case LV_DIR_TOP:
        {
            lv_indev_wait_release(lv_indev_get_act());
            ui_load_scr_animation(&guider_ui, &guider_ui.under_up, guider_ui.under_up_del, &guider_ui.Clock_2_del, setup_scr_under_up, LV_SCR_LOAD_ANIM_OVER_TOP, 200, 200, true, true);
            break;
        }
        case LV_DIR_BOTTOM:
        {
            lv_indev_wait_release(lv_indev_get_act());
            ui_load_scr_animation(&guider_ui, &guider_ui.top_lap, guider_ui.top_lap_del, &guider_ui.Clock_2_del, setup_scr_top_lap, LV_SCR_LOAD_ANIM_OVER_BOTTOM, 200, 200, true, true);
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void Clock_2_img_1_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_LONG_PRESSED:
    {
        lv_obj_clear_flag(guider_ui.Clock_2_cont_2, LV_OBJ_FLAG_HIDDEN);
        ui_animation(guider_ui.Clock_2_cont_2, 200, 0, lv_obj_get_x(guider_ui.Clock_2_cont_2), 120, &lv_anim_path_overshoot, 1, 0, 0, 0, (lv_anim_exec_xcb_t)lv_obj_set_x, NULL, NULL, NULL);
        ui_animation(guider_ui.Clock_2_cont_1, 200, 0, lv_obj_get_x(guider_ui.Clock_2_cont_1), 0, &lv_anim_path_overshoot, 1, 0, 0, 0, (lv_anim_exec_xcb_t)lv_obj_set_x, NULL, NULL, NULL);
        break;
    }
    default:
        break;
    }
}

static void Clock_2_btn_3_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock1_btn_3_is_click) {
            Clock1_btn_3_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_6, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_3, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock1_btn_3_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_6, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_3, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_2_btn_2_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock1_btn_2_is_click) {
            Clock1_btn_2_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_5, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_2, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock1_btn_2_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_5, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_2, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_2_btn_1_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock1_btn_1_is_click) {
            Clock1_btn_1_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_4, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_1, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock1_btn_1_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_4, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_1, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_2_img_6_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock1_btn_1_is_click) {
            Clock1_btn_1_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_6, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_3, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock1_btn_1_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_6, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_3, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_2_img_5_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock1_btn_2_is_click) {
            Clock1_btn_2_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_5, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_2, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock1_btn_2_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_5, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_2, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_2_img_4_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock1_btn_1_is_click) {
            Clock1_btn_1_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_4, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_1, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock1_btn_1_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_2_img_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_2_img_4, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_2_btn_1, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_2_cont_2_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_LONG_PRESSED:
    {
        lv_obj_add_flag(guider_ui.Clock_2_cont_2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(guider_ui.Clock_2_cont_2, 0);
        ui_animation(guider_ui.Clock_2_cont_1, 200, 0, lv_obj_get_x(guider_ui.Clock_2_cont_1), -140, &lv_anim_path_overshoot, 1, 0, 0, 0, (lv_anim_exec_xcb_t)lv_obj_set_x, NULL, NULL, NULL);
        break;
    }
    default:
        break;
    }
}

void events_init_Clock_2 (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->Clock_2, Clock_2_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_2_img_1, Clock_2_img_1_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_2_btn_3, Clock_2_btn_3_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_2_btn_2, Clock_2_btn_2_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_2_btn_1, Clock_2_btn_1_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_2_img_6, Clock_2_img_6_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_2_img_5, Clock_2_img_5_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_2_img_4, Clock_2_img_4_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_2_cont_2, Clock_2_cont_2_event_handler, LV_EVENT_ALL, ui);
}

static void Clock_3_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_LONG_PRESSED:
    {
        break;
    }
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
        case LV_DIR_RIGHT:
        {
            lv_indev_wait_release(lv_indev_get_act());
            screen_index=0;
            ui_load_scr_animation(&guider_ui, &guider_ui.Clock_1, guider_ui.Clock_1_del, &guider_ui.Clock_3_del, setup_scr_Clock_1, LV_SCR_LOAD_ANIM_OVER_RIGHT, 200, 0, false, false);
            break;
        }
        case LV_DIR_TOP:
        {
            lv_indev_wait_release(lv_indev_get_act());
            ui_load_scr_animation(&guider_ui, &guider_ui.under_up, guider_ui.under_up_del, &guider_ui.Clock_3_del, setup_scr_under_up, LV_SCR_LOAD_ANIM_OVER_TOP, 200, 0, false, false);
            break;
        }
        case LV_DIR_BOTTOM:
        {
            lv_indev_wait_release(lv_indev_get_act());
            ui_load_scr_animation(&guider_ui, &guider_ui.top_lap, guider_ui.top_lap_del, &guider_ui.Clock_3_del, setup_scr_top_lap, LV_SCR_LOAD_ANIM_OVER_BOTTOM, 200, 0, false, false);
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void Clock_3_analog_clock_1_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_LONG_PRESSED:
    {
        lv_obj_clear_flag(guider_ui.Clock_3_cont_4, LV_OBJ_FLAG_HIDDEN);
        ui_animation(guider_ui.Clock_3_cont_4, 200, 0, lv_obj_get_x(guider_ui.Clock_3_cont_4), 120, &lv_anim_path_overshoot, 1, 0, 0, 0, (lv_anim_exec_xcb_t)lv_obj_set_x, NULL, NULL, NULL);
        ui_animation(guider_ui.Clock_3_cont_3, 200, 0, lv_obj_get_x(guider_ui.Clock_3_cont_3), 0, &lv_anim_path_overshoot, 1, 0, 0, 0, (lv_anim_exec_xcb_t)lv_obj_set_x, NULL, NULL, NULL);
        break;
    }
    default:
        break;
    }
}

static void Clock_3_btn_3_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock2_btn_3_is_click) {
            Clock2_btn_3_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_11, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_11, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_3, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock2_btn_3_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_11, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_11, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_3, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }

        break;
    }
    default:
        break;
    }
}

static void Clock_3_btn_2_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock2_btn_2_is_click) {
            Clock2_btn_2_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_10, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_2, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock2_btn_2_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_10, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_10, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_2, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }
        break;
    }
    default:
        break;
    }
}

static void Clock_3_btn_1_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock2_btn_1_is_click) {
            Clock2_btn_1_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_9, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_1, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock2_btn_1_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_9, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_9, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_1, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }
        break;
    }
    default:
        break;
    }
}

static void Clock_3_img_11_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock2_btn_3_is_click) {
            Clock2_btn_3_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_11, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_11, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_3, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock2_btn_3_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_11, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_11, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_3, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }
        break;
    }
    default:
        break;
    }
}

static void Clock_3_img_10_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock2_btn_2_is_click) {
            Clock2_btn_2_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_10, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_2, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock2_btn_2_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_10, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_10, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_2, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }
        break;
    }
    default:
        break;
    }
}

static void Clock_3_img_9_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        if(Clock2_btn_1_is_click) {
            Clock2_btn_1_is_click=0;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_9, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_1, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            Clock2_btn_1_is_click=1;
            lv_obj_set_style_img_recolor_opa(guider_ui.Clock_3_img_9, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor(guider_ui.Clock_3_img_9, lv_color_hex(0x313131), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(guider_ui.Clock_3_btn_1, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
        }
        break;
    }
    default:
        break;
    }
}

static void Clock_3_cont_4_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_LONG_PRESSED:
    {
        lv_obj_add_flag(guider_ui.Clock_3_cont_4, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(guider_ui.Clock_3_cont_4, 0);
        ui_animation(guider_ui.Clock_3_cont_3, 200, 0, lv_obj_get_x(guider_ui.Clock_3_cont_3), -140, &lv_anim_path_overshoot, 1, 0, 0, 0, (lv_anim_exec_xcb_t)lv_obj_set_x, NULL, NULL, NULL);
        break;
    }
    default:
        break;
    }
}

void events_init_Clock_3 (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->Clock_3, Clock_3_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_3_analog_clock_1, Clock_3_analog_clock_1_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_3_btn_3, Clock_3_btn_3_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_3_btn_2, Clock_3_btn_2_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_3_btn_1, Clock_3_btn_1_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_3_img_11, Clock_3_img_11_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_3_img_10, Clock_3_img_10_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_3_img_9, Clock_3_img_9_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->Clock_3_cont_4, Clock_3_cont_4_event_handler, LV_EVENT_ALL, ui);
}

static void top_lap_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
        case LV_DIR_TOP:
        {
            lv_indev_wait_release(lv_indev_get_act());
            switch (screen_index)
            {
            case 0:
                /* code */
                lv_indev_wait_release(lv_indev_get_act());
                ui_load_scr_animation(&guider_ui, &guider_ui.Clock_1, guider_ui.Clock_1_del, &guider_ui.under_up_del, setup_scr_Clock_1, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, true, true);
                break;
            case 1:
                lv_indev_wait_release(lv_indev_get_act());
                ui_load_scr_animation(&guider_ui, &guider_ui.Clock_2, guider_ui.Clock_2_del, &guider_ui.top_lap_del, setup_scr_Clock_2, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, true, true);
                break;
            case 2:
                lv_indev_wait_release(lv_indev_get_act());
                ui_load_scr_animation(&guider_ui, &guider_ui.Clock_3, guider_ui.Clock_3_del, &guider_ui.top_lap_del, setup_scr_Clock_3, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, true, true);
                break;
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

void events_init_top_lap (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->top_lap, top_lap_event_handler, LV_EVENT_ALL, ui);
}

static void under_up_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_GESTURE:
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
        case LV_DIR_BOTTOM:
        {
            lv_indev_wait_release(lv_indev_get_act());
            switch (screen_index)
            {
            case 0:
                /* code */
                lv_indev_wait_release(lv_indev_get_act());
                ui_load_scr_animation(&guider_ui, &guider_ui.Clock_1, guider_ui.Clock_1_del, &guider_ui.under_up_del, setup_scr_Clock_1, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, true, true);
                break;
            case 1:
                lv_indev_wait_release(lv_indev_get_act());
                ui_load_scr_animation(&guider_ui, &guider_ui.Clock_2, guider_ui.Clock_2_del, &guider_ui.top_lap_del, setup_scr_Clock_2, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, true, true);
                break;
            case 2:
                lv_indev_wait_release(lv_indev_get_act());
                ui_load_scr_animation(&guider_ui, &guider_ui.Clock_3, guider_ui.Clock_3_del, &guider_ui.top_lap_del, setup_scr_Clock_3, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, true, true);
                break;
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

void events_init_under_up (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->under_up, under_up_event_handler, LV_EVENT_ALL, ui);
}


void events_init(lv_ui *ui)
{

}
