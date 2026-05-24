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
#include "lv_analogclock.h"



int Clock_3_analog_clock_1_hour_value = 3;
int Clock_3_analog_clock_1_min_value = 20;
int Clock_3_analog_clock_1_sec_value = 50;
void setup_scr_Clock_3(lv_ui *ui)
{
    //Write codes Clock_3
    ui->Clock_3 = lv_obj_create(NULL);
    lv_obj_set_size(ui->Clock_3, 240, 280);
    lv_obj_set_scrollbar_mode(ui->Clock_3, LV_SCROLLBAR_MODE_OFF);

    //Write style for Clock_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_3, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_3, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_3_analog_clock_1
    static bool Clock_3_analog_clock_1_timer_enabled = false;
    ui->Clock_3_analog_clock_1 = lv_analogclock_create(ui->Clock_3);
    lv_analogclock_hide_digits(ui->Clock_3_analog_clock_1, true);
    lv_analogclock_set_major_ticks(ui->Clock_3_analog_clock_1, 0, 0, lv_color_hex(0x555555), 10);
    lv_analogclock_set_ticks(ui->Clock_3_analog_clock_1, 0, 0, lv_color_hex(0x333333));
    lv_analogclock_set_hour_needle_img(ui->Clock_3_analog_clock_1, &_time_alpha_50x8_ext, 0, 2);
    lv_analogclock_set_min_needle_img(ui->Clock_3_analog_clock_1, &_fen_alpha_80x8_ext, 0, 2);
    lv_analogclock_set_sec_needle_line(ui->Clock_3_analog_clock_1, 2, lv_color_hex(0xff0027), -10);
    lv_analogclock_set_time(ui->Clock_3_analog_clock_1, Clock_3_analog_clock_1_hour_value, Clock_3_analog_clock_1_min_value,Clock_3_analog_clock_1_sec_value);
    // create timer
    if (!Clock_3_analog_clock_1_timer_enabled) {
        lv_timer_create(Clock_3_analog_clock_1_timer, 1000, NULL);
        Clock_3_analog_clock_1_timer_enabled = true;
    }
    lv_obj_set_pos(ui->Clock_3_analog_clock_1, 20, 40);
    lv_obj_set_size(ui->Clock_3_analog_clock_1, 200, 200);

    //Write style for Clock_3_analog_clock_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_3_analog_clock_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_3_analog_clock_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_3_analog_clock_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->Clock_3_analog_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_3_analog_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui->Clock_3_analog_clock_1, &_biaopan1_200x200_ext, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_opa(ui->Clock_3_analog_clock_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(ui->Clock_3_analog_clock_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for Clock_3_analog_clock_1, Part: LV_PART_TICKS, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->Clock_3_analog_clock_1, lv_color_hex(0xff0000), LV_PART_TICKS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_3_analog_clock_1, &lv_font_alimama_12, LV_PART_TICKS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_3_analog_clock_1, 255, LV_PART_TICKS|LV_STATE_DEFAULT);

    //Write style for Clock_3_analog_clock_1, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_3_analog_clock_1, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_3_analog_clock_1, lv_color_hex(0x414141), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_3_analog_clock_1, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write codes Clock_3_img_1
    ui->Clock_3_img_1 = lv_img_create(ui->Clock_3);
    lv_obj_add_flag(ui->Clock_3_img_1, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_3_img_1, &_watchdight1_alpha_60x60_ext);
    lv_img_set_pivot(ui->Clock_3_img_1, 50,50);
    lv_img_set_angle(ui->Clock_3_img_1, 0);
    lv_obj_set_pos(ui->Clock_3_img_1, 12, 35);
    lv_obj_set_size(ui->Clock_3_img_1, 60, 60);

    //Write style for Clock_3_img_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_3_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_3_img_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_3_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_3_img_1, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_3_img_2
    ui->Clock_3_img_2 = lv_img_create(ui->Clock_3);
    lv_obj_add_flag(ui->Clock_3_img_2, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_3_img_2, &_watchdight3_alpha_60x60_ext);
    lv_img_set_pivot(ui->Clock_3_img_2, 50,50);
    lv_img_set_angle(ui->Clock_3_img_2, 0);
    lv_obj_set_pos(ui->Clock_3_img_2, 169, 35);
    lv_obj_set_size(ui->Clock_3_img_2, 60, 60);

    //Write style for Clock_3_img_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_3_img_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_3_img_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_3_img_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_3_img_2, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_3_img_3
    ui->Clock_3_img_3 = lv_img_create(ui->Clock_3);
    lv_obj_add_flag(ui->Clock_3_img_3, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_3_img_3, &_watchdight2_alpha_60x60_ext);
    lv_img_set_pivot(ui->Clock_3_img_3, 50,50);
    lv_img_set_angle(ui->Clock_3_img_3, 0);
    lv_obj_set_pos(ui->Clock_3_img_3, 12, 203);
    lv_obj_set_size(ui->Clock_3_img_3, 60, 60);

    //Write style for Clock_3_img_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_3_img_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_3_img_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_3_img_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_3_img_3, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_3_label_1
    ui->Clock_3_label_1 = lv_label_create(ui->Clock_3);
    lv_label_set_text(ui->Clock_3_label_1, "宁波");
    lv_label_set_long_mode(ui->Clock_3_label_1, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_3_label_1, 180, 240);
    lv_obj_set_size(ui->Clock_3_label_1, 45, 20);

    //Write style for Clock_3_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_3_label_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_3_label_1, &lv_font_alimama_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_3_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_3_label_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_3_label_2
    ui->Clock_3_label_2 = lv_label_create(ui->Clock_3);
    lv_label_set_text(ui->Clock_3_label_2, "20");
    lv_label_set_long_mode(ui->Clock_3_label_2, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_3_label_2, 192, 18);
    lv_obj_set_size(ui->Clock_3_label_2, 38, 18);

    //Write style for Clock_3_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_3_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_3_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_3_label_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_3_label_2, &lv_font_alimama_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_3_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_3_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_3_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_3_label_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_3_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_3_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_3_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_3_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_3_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_3_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_3_cont_1
    ui->Clock_3_cont_1 = lv_obj_create(ui->Clock_3);
    lv_obj_set_pos(ui->Clock_3_cont_1, 25, 227);
    lv_obj_set_size(ui->Clock_3_cont_1, 10, 10);
    lv_obj_set_scrollbar_mode(ui->Clock_3_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for Clock_3_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_3_cont_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->Clock_3_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->Clock_3_cont_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->Clock_3_cont_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_3_cont_1, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_3_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_3_cont_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_3_cont_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_3_cont_2
    ui->Clock_3_cont_2 = lv_obj_create(ui->Clock_3);
    lv_obj_set_pos(ui->Clock_3_cont_2, 195, 48);
    lv_obj_set_size(ui->Clock_3_cont_2, 10, 10);
    lv_obj_set_scrollbar_mode(ui->Clock_3_cont_2, LV_SCROLLBAR_MODE_OFF);

    //Write style for Clock_3_cont_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_3_cont_2, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->Clock_3_cont_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->Clock_3_cont_2, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->Clock_3_cont_2, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_3_cont_2, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_3_cont_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_3_cont_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_3_cont_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_3_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_3_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_3_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_3_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_3_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_3_img_4
    ui->Clock_3_img_4 = lv_img_create(ui->Clock_3);
    lv_obj_add_flag(ui->Clock_3_img_4, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_3_img_4, &_Ellipse_alpha_40x40);
    lv_img_set_pivot(ui->Clock_3_img_4, 50,50);
    lv_img_set_angle(ui->Clock_3_img_4, 0);
    lv_obj_set_pos(ui->Clock_3_img_4, 5, 11);
    lv_obj_set_size(ui->Clock_3_img_4, 40, 40);

    //Write style for Clock_3_img_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_3_img_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_3_img_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_3_img_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_3_img_4, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_3_img_5
    ui->Clock_3_img_5 = lv_img_create(ui->Clock_3);
    lv_obj_add_flag(ui->Clock_3_img_5, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_3_img_5, &_Stime_alpha_16x8);
    lv_img_set_pivot(ui->Clock_3_img_5, 8,4);
    lv_img_set_angle(ui->Clock_3_img_5, -100);
    lv_obj_set_pos(ui->Clock_3_img_5, 25, 26);
    lv_obj_set_size(ui->Clock_3_img_5, 16, 8);

    //Write style for Clock_3_img_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_3_img_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_3_img_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_3_img_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_3_img_5, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_3_img_6
    ui->Clock_3_img_6 = lv_img_create(ui->Clock_3);
    lv_obj_add_flag(ui->Clock_3_img_6, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_3_img_6, &_Sfen_alpha_21x6);
    lv_img_set_pivot(ui->Clock_3_img_6, 0,0);
    lv_img_set_angle(ui->Clock_3_img_6, 2000);
    lv_obj_set_pos(ui->Clock_3_img_6, 28, 35);
    lv_obj_set_size(ui->Clock_3_img_6, 21, 6);

    //Write style for Clock_3_img_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_3_img_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_3_img_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_3_img_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_3_img_6, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of Clock_3.


    //Update current screen layout.
    lv_obj_update_layout(ui->Clock_3);

    //Init events for screen.
    events_init_Clock_3(ui);
}
