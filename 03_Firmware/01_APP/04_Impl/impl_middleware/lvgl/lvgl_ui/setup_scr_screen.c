/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"

/**
 * MANUAL EDIT (2026-05-08):
 * The three analog-clock needle descriptors below were switched from the
 * gui_guider-generated _<name>_alpha_70x5 / _40x5 (data in internal flash
 * .rodata) to the *_ext counterparts defined in storage_assets.c whose
 * .data points at RAM mirrors fed from the external W25Q64 flash.
 * If gui_guider regenerates this file, re-apply the *_ext rename.
 */
LV_IMG_DECLARE(_fen_alpha_70x5_ext);
LV_IMG_DECLARE(_miao_alpha_70x5_ext);
LV_IMG_DECLARE(_time_alpha_40x5_ext);
LV_IMG_DECLARE(_biaopan1_alpha_240x240_ext);



int screen_analog_clock_1_hour_value = 3;
int screen_analog_clock_1_min_value = 20;
int screen_analog_clock_1_sec_value = 50;
void setup_scr_screen(lv_ui *ui)
{
    //Write codes screen
    ui->screen = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen, 240, 280);
    lv_obj_set_scrollbar_mode(ui->screen, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_img_1
    ui->screen_img_1 = lv_img_create(ui->screen);
    lv_obj_add_flag(ui->screen_img_1, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_img_1, &_biaopan1_alpha_240x240_ext);
    lv_img_set_pivot(ui->screen_img_1, 50,50);
    lv_img_set_angle(ui->screen_img_1, 0);
    lv_obj_set_pos(ui->screen_img_1, 0, 20);
    lv_obj_set_size(ui->screen_img_1, 240, 240);

    //Write style for screen_img_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_img_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_img_1, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_analog_clock_1
    static bool screen_analog_clock_1_timer_enabled = false;
    ui->screen_analog_clock_1 = lv_analogclock_create(ui->screen);
    lv_analogclock_hide_digits(ui->screen_analog_clock_1, true);
    lv_analogclock_hide_point(ui->screen_analog_clock_1, true);
    lv_analogclock_set_major_ticks(ui->screen_analog_clock_1, 5, 1, lv_color_hex(0x555555), 10);
    lv_analogclock_set_ticks(ui->screen_analog_clock_1, 2, 0, lv_color_hex(0x333333));
    lv_analogclock_set_hour_needle_img(ui->screen_analog_clock_1, &_time_alpha_40x5_ext, 0, 2);
    lv_analogclock_set_min_needle_img(ui->screen_analog_clock_1, &_fen_alpha_70x5_ext, 0, 2);
    lv_analogclock_set_sec_needle_img(ui->screen_analog_clock_1, &_miao_alpha_70x5_ext, 0, 2);
    lv_analogclock_set_time(ui->screen_analog_clock_1, screen_analog_clock_1_hour_value, screen_analog_clock_1_min_value,screen_analog_clock_1_sec_value);
    // create timer
    if (!screen_analog_clock_1_timer_enabled) {
        lv_timer_create(screen_analog_clock_1_timer, 1000, NULL);
        screen_analog_clock_1_timer_enabled = true;
    }
    lv_obj_set_pos(ui->screen_analog_clock_1, 0, 17);
    lv_obj_set_size(ui->screen_analog_clock_1, 240, 240);

    //Write style for screen_analog_clock_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_analog_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_analog_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_analog_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_analog_clock_1, Part: LV_PART_TICKS, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->screen_analog_clock_1, lv_color_hex(0xffffff), LV_PART_TICKS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_analog_clock_1, &lv_font_alimama_12, LV_PART_TICKS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_analog_clock_1, 255, LV_PART_TICKS|LV_STATE_DEFAULT);

    //Write style for screen_analog_clock_1, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_analog_clock_1, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_analog_clock_1, lv_color_hex(0xffffff), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_analog_clock_1, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //The custom code of screen.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen);

    //Init events for screen.
    events_init_screen(ui);
}
