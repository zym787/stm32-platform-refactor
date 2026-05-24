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



void setup_scr_Clock_1(lv_ui *ui)
{
    //Write codes Clock_1
    ui->Clock_1 = lv_obj_create(NULL);
    lv_obj_set_size(ui->Clock_1, 240, 280);
    lv_obj_set_scrollbar_mode(ui->Clock_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for Clock_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_label_1
    ui->Clock_1_label_1 = lv_label_create(ui->Clock_1);
    lv_label_set_text(ui->Clock_1_label_1, "ETERNALCHIP");
    lv_label_set_long_mode(ui->Clock_1_label_1, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_1_label_1, 33, 19);
    lv_obj_set_size(ui->Clock_1_label_1, 176, 24);

    //Write style for Clock_1_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_1_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_1_label_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_1_label_1, &lv_font_interttf_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_1_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_1_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_1_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_1_label_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_1_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_arc_1
    ui->Clock_1_arc_1 = lv_arc_create(ui->Clock_1);
    lv_arc_set_mode(ui->Clock_1_arc_1, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(ui->Clock_1_arc_1, 0, 100);
    lv_arc_set_bg_angles(ui->Clock_1_arc_1, 135, 45);
    lv_arc_set_value(ui->Clock_1_arc_1, 100);
    lv_arc_set_rotation(ui->Clock_1_arc_1, 240);
    lv_obj_set_pos(ui->Clock_1_arc_1, 27, 70);
    lv_obj_set_size(ui->Clock_1_arc_1, 36, 36);

    //Write style for Clock_1_arc_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_1_arc_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->Clock_1_arc_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui->Clock_1_arc_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->Clock_1_arc_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->Clock_1_arc_1, lv_color_hex(0xe6e6e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_arc_1, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_arc_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_arc_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_arc_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_arc_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_arc_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for Clock_1_arc_1, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->Clock_1_arc_1, 2, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->Clock_1_arc_1, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->Clock_1_arc_1, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for Clock_1_arc_1, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_1_arc_1, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->Clock_1_arc_1, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes Clock_1_arc_2
    ui->Clock_1_arc_2 = lv_arc_create(ui->Clock_1);
    lv_arc_set_mode(ui->Clock_1_arc_2, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(ui->Clock_1_arc_2, 0, 100);
    lv_arc_set_bg_angles(ui->Clock_1_arc_2, 135, 45);
    lv_arc_set_value(ui->Clock_1_arc_2, 100);
    lv_arc_set_rotation(ui->Clock_1_arc_2, 240);
    lv_obj_set_pos(ui->Clock_1_arc_2, 27, 123);
    lv_obj_set_size(ui->Clock_1_arc_2, 36, 36);

    //Write style for Clock_1_arc_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_1_arc_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->Clock_1_arc_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui->Clock_1_arc_2, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->Clock_1_arc_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->Clock_1_arc_2, lv_color_hex(0xe6e6e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_arc_2, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_arc_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_arc_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_arc_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_arc_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_arc_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for Clock_1_arc_2, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->Clock_1_arc_2, 2, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->Clock_1_arc_2, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->Clock_1_arc_2, lv_color_hex(0xff0027), LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for Clock_1_arc_2, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_1_arc_2, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->Clock_1_arc_2, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes Clock_1_arc_3
    ui->Clock_1_arc_3 = lv_arc_create(ui->Clock_1);
    lv_arc_set_mode(ui->Clock_1_arc_3, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(ui->Clock_1_arc_3, 0, 100);
    lv_arc_set_bg_angles(ui->Clock_1_arc_3, 135, 45);
    lv_arc_set_value(ui->Clock_1_arc_3, 100);
    lv_arc_set_rotation(ui->Clock_1_arc_3, 240);
    lv_obj_set_pos(ui->Clock_1_arc_3, 27, 177);
    lv_obj_set_size(ui->Clock_1_arc_3, 36, 36);

    //Write style for Clock_1_arc_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_1_arc_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->Clock_1_arc_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui->Clock_1_arc_3, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->Clock_1_arc_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->Clock_1_arc_3, lv_color_hex(0xe6e6e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_arc_3, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_arc_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_arc_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_arc_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_arc_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_arc_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for Clock_1_arc_3, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->Clock_1_arc_3, 2, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->Clock_1_arc_3, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->Clock_1_arc_3, lv_color_hex(0xff6500), LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for Clock_1_arc_3, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_1_arc_3, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->Clock_1_arc_3, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes Clock_1_arc_4
    ui->Clock_1_arc_4 = lv_arc_create(ui->Clock_1);
    lv_arc_set_mode(ui->Clock_1_arc_4, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(ui->Clock_1_arc_4, 0, 100);
    lv_arc_set_bg_angles(ui->Clock_1_arc_4, 135, 45);
    lv_arc_set_value(ui->Clock_1_arc_4, 100);
    lv_arc_set_rotation(ui->Clock_1_arc_4, 240);
    lv_obj_set_pos(ui->Clock_1_arc_4, 27, 228);
    lv_obj_set_size(ui->Clock_1_arc_4, 36, 36);

    //Write style for Clock_1_arc_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_1_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->Clock_1_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui->Clock_1_arc_4, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->Clock_1_arc_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->Clock_1_arc_4, lv_color_hex(0xe6e6e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_arc_4, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_arc_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for Clock_1_arc_4, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->Clock_1_arc_4, 2, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->Clock_1_arc_4, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->Clock_1_arc_4, lv_color_hex(0x00ff08), LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for Clock_1_arc_4, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_1_arc_4, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->Clock_1_arc_4, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes Clock_1_img_1
    ui->Clock_1_img_1 = lv_img_create(ui->Clock_1);
    lv_obj_add_flag(ui->Clock_1_img_1, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_1_img_1, &_sheshidu_alpha_10x10);
    lv_img_set_pivot(ui->Clock_1_img_1, 50,50);
    lv_img_set_angle(ui->Clock_1_img_1, 0);
    lv_obj_set_pos(ui->Clock_1_img_1, 46, 79);
    lv_obj_set_size(ui->Clock_1_img_1, 10, 10);

    //Write style for Clock_1_img_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_1_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_1_img_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_1_img_1, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_img_2
    ui->Clock_1_img_2 = lv_img_create(ui->Clock_1);
    lv_obj_add_flag(ui->Clock_1_img_2, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_1_img_2, &_wather16x16_alpha_16x16);
    lv_img_set_pivot(ui->Clock_1_img_2, 50,50);
    lv_img_set_angle(ui->Clock_1_img_2, 0);
    lv_obj_set_pos(ui->Clock_1_img_2, 54, 70);
    lv_obj_set_size(ui->Clock_1_img_2, 16, 16);

    //Write style for Clock_1_img_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_1_img_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_1_img_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_img_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_1_img_2, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_img_3
    ui->Clock_1_img_3 = lv_img_create(ui->Clock_1);
    lv_obj_add_flag(ui->Clock_1_img_3, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_1_img_3, &_heart16x16_alpha_16x16);
    lv_img_set_pivot(ui->Clock_1_img_3, 50,50);
    lv_img_set_angle(ui->Clock_1_img_3, 0);
    lv_obj_set_pos(ui->Clock_1_img_3, 54, 123);
    lv_obj_set_size(ui->Clock_1_img_3, 16, 16);

    //Write style for Clock_1_img_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_1_img_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_1_img_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_img_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_1_img_3, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_img_4
    ui->Clock_1_img_4 = lv_img_create(ui->Clock_1);
    lv_obj_add_flag(ui->Clock_1_img_4, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_1_img_4, &_KLL16x16_alpha_16x16);
    lv_img_set_pivot(ui->Clock_1_img_4, 50,50);
    lv_img_set_angle(ui->Clock_1_img_4, 0);
    lv_obj_set_pos(ui->Clock_1_img_4, 54, 177);
    lv_obj_set_size(ui->Clock_1_img_4, 16, 16);

    //Write style for Clock_1_img_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_1_img_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_1_img_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_img_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_1_img_4, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_img_5
    ui->Clock_1_img_5 = lv_img_create(ui->Clock_1);
    lv_obj_add_flag(ui->Clock_1_img_5, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_1_img_5, &_foot16x16_alpha_16x16);
    lv_img_set_pivot(ui->Clock_1_img_5, 50,50);
    lv_img_set_angle(ui->Clock_1_img_5, 0);
    lv_obj_set_pos(ui->Clock_1_img_5, 54, 228);
    lv_obj_set_size(ui->Clock_1_img_5, 16, 16);

    //Write style for Clock_1_img_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_1_img_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_1_img_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_img_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_1_img_5, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_label_2
    ui->Clock_1_label_2 = lv_label_create(ui->Clock_1);
    lv_label_set_text(ui->Clock_1_label_2, "24");
    lv_label_set_long_mode(ui->Clock_1_label_2, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_1_label_2, 33, 86);
    lv_obj_set_size(ui->Clock_1_label_2, 23, 10);

    //Write style for Clock_1_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_1_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_1_label_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_1_label_2, &lv_font_interttf_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_1_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_1_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_1_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_1_label_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_1_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_label_3
    ui->Clock_1_label_3 = lv_label_create(ui->Clock_1);
    lv_label_set_text(ui->Clock_1_label_3, "72");
    lv_label_set_long_mode(ui->Clock_1_label_3, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_1_label_3, 33, 136);
    lv_obj_set_size(ui->Clock_1_label_3, 23, 10);

    //Write style for Clock_1_label_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_1_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_1_label_3, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_1_label_3, &lv_font_interttf_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_1_label_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_1_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_1_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_1_label_3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_1_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_label_4
    ui->Clock_1_label_4 = lv_label_create(ui->Clock_1);
    lv_label_set_text(ui->Clock_1_label_4, "304");
    lv_label_set_long_mode(ui->Clock_1_label_4, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_1_label_4, 33, 189);
    lv_obj_set_size(ui->Clock_1_label_4, 23, 10);

    //Write style for Clock_1_label_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_1_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_1_label_4, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_1_label_4, &lv_font_interttf_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_1_label_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_1_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_1_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_1_label_4, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_1_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_label_5
    ui->Clock_1_label_5 = lv_label_create(ui->Clock_1);
    lv_label_set_text(ui->Clock_1_label_5, "9046");
    lv_label_set_long_mode(ui->Clock_1_label_5, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_1_label_5, 27, 241);
    lv_obj_set_size(ui->Clock_1_label_5, 34, 10);

    //Write style for Clock_1_label_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_1_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_1_label_5, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_1_label_5, &lv_font_interttf_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_1_label_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_1_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_1_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_1_label_5, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_1_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_label_6
    ui->Clock_1_label_6 = lv_label_create(ui->Clock_1);
    lv_label_set_text(ui->Clock_1_label_6, "09");
    lv_label_set_long_mode(ui->Clock_1_label_6, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_1_label_6, 104, 57);
    lv_obj_set_size(ui->Clock_1_label_6, 113, 79);

    //Write style for Clock_1_label_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_1_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_1_label_6, lv_color_hex(0xff0050), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_1_label_6, &lv_font_interttf_24  /* downgraded from interttf_82 to fit Flash budget */, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_1_label_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_1_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_1_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_1_label_6, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_1_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_label_7
    ui->Clock_1_label_7 = lv_label_create(ui->Clock_1);
    lv_label_set_text(ui->Clock_1_label_7, "28");
    lv_label_set_long_mode(ui->Clock_1_label_7, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_1_label_7, 101, 143);
    lv_obj_set_size(ui->Clock_1_label_7, 113, 79);

    //Write style for Clock_1_label_7, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_1_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_1_label_7, lv_color_hex(0xc2ff00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_1_label_7, &lv_font_interttf_24  /* downgraded from interttf_82 to fit Flash budget */, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_1_label_7, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_1_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_1_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_1_label_7, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_1_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_label_8
    ui->Clock_1_label_8 = lv_label_create(ui->Clock_1);
    lv_label_set_text(ui->Clock_1_label_8, "AM");
    lv_label_set_long_mode(ui->Clock_1_label_8, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_1_label_8, 90, 244);
    lv_obj_set_size(ui->Clock_1_label_8, 40, 24);

    //Write style for Clock_1_label_8, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_1_label_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_label_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_1_label_8, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_1_label_8, &lv_font_interttf_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_1_label_8, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_1_label_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_1_label_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_1_label_8, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_1_label_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_label_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_label_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_label_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_label_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_label_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_1_label_9
    ui->Clock_1_label_9 = lv_label_create(ui->Clock_1);
    lv_label_set_text(ui->Clock_1_label_9, "03/18");
    lv_label_set_long_mode(ui->Clock_1_label_9, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_1_label_9, 136, 244);
    lv_obj_set_size(ui->Clock_1_label_9, 78, 24);

    //Write style for Clock_1_label_9, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_1_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_1_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_1_label_9, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_1_label_9, &lv_font_interttf_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_1_label_9, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_1_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_1_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_1_label_9, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_1_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_1_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_1_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_1_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_1_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_1_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of Clock_1.
    lv_obj_clear_flag(guider_ui.Clock_1_arc_1,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.Clock_1_arc_2,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.Clock_1_arc_3,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.Clock_1_arc_4,LV_OBJ_FLAG_CLICKABLE);

    //Update current screen layout.
    lv_obj_update_layout(ui->Clock_1);

    //Init events for screen.
    events_init_Clock_1(ui);
}
