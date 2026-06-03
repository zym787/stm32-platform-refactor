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



void setup_scr_under_up(lv_ui *ui)
{
    //Write codes under_up
    ui->under_up = lv_obj_create(NULL);
    lv_obj_set_size(ui->under_up, 240, 280);
    lv_obj_set_scrollbar_mode(ui->under_up, LV_SCROLLBAR_MODE_OFF);

    //Write style for under_up, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->under_up, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->under_up, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->under_up, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_img_1
    ui->under_up_img_1 = lv_img_create(ui->under_up);
    lv_obj_add_flag(ui->under_up_img_1, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->under_up_img_1, &_ZNZBG_alpha_100x100_ext);
    lv_img_set_pivot(ui->under_up_img_1, 50,50);
    lv_img_set_angle(ui->under_up_img_1, 0);
    lv_obj_set_pos(ui->under_up_img_1, 70, 90);
    lv_obj_set_size(ui->under_up_img_1, 100, 100);

    //Write style for under_up_img_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->under_up_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->under_up_img_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->under_up_img_1, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_img_6
    ui->under_up_img_6 = lv_img_create(ui->under_up);
    lv_obj_add_flag(ui->under_up_img_6, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->under_up_img_6, &_arw_alpha_50x40_ext);
    lv_img_set_pivot(ui->under_up_img_6, 50,50);
    lv_img_set_angle(ui->under_up_img_6, 0);
    lv_obj_set_pos(ui->under_up_img_6, 162, 29);
    lv_obj_set_size(ui->under_up_img_6, 50, 40);

    //Write style for under_up_img_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->under_up_img_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->under_up_img_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_img_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->under_up_img_6, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_img_2
    ui->under_up_img_2 = lv_img_create(ui->under_up);
    lv_obj_add_flag(ui->under_up_img_2, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->under_up_img_2, &_ZNZ_alpha_50x50_ext);
    lv_img_set_pivot(ui->under_up_img_2, 50,50);
    lv_img_set_angle(ui->under_up_img_2, 0);
    lv_obj_set_pos(ui->under_up_img_2, 95, 115);
    lv_obj_set_size(ui->under_up_img_2, 50, 50);

    //Write style for under_up_img_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->under_up_img_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->under_up_img_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_img_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->under_up_img_2, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_cont_1
    ui->under_up_cont_1 = lv_obj_create(ui->under_up);
    lv_obj_set_pos(ui->under_up_cont_1, 29, 32);
    lv_obj_set_size(ui->under_up_cont_1, 50, 50);
    lv_obj_set_scrollbar_mode(ui->under_up_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for under_up_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->under_up_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_cont_1, 50, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->under_up_cont_1, 110, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->under_up_cont_1, lv_color_hex(0xff0000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->under_up_cont_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->under_up_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->under_up_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->under_up_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->under_up_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->under_up_cont_1, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->under_up_cont_1, lv_color_hex(0xff0000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->under_up_cont_1, 104, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->under_up_cont_1, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(ui->under_up_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(ui->under_up_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_cont_2
    ui->under_up_cont_2 = lv_obj_create(ui->under_up);
    lv_obj_set_pos(ui->under_up_cont_2, 29, 193);
    lv_obj_set_size(ui->under_up_cont_2, 50, 50);
    lv_obj_set_scrollbar_mode(ui->under_up_cont_2, LV_SCROLLBAR_MODE_OFF);

    //Write style for under_up_cont_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->under_up_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_cont_2, 50, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->under_up_cont_2, 175, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->under_up_cont_2, lv_color_hex(0xf2ea01), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->under_up_cont_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->under_up_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->under_up_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->under_up_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->under_up_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->under_up_cont_2, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->under_up_cont_2, lv_color_hex(0xe5ff00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->under_up_cont_2, 170, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->under_up_cont_2, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(ui->under_up_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(ui->under_up_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_cont_3
    ui->under_up_cont_3 = lv_obj_create(ui->under_up);
    lv_obj_set_pos(ui->under_up_cont_3, 166, 193);
    lv_obj_set_size(ui->under_up_cont_3, 50, 50);
    lv_obj_set_scrollbar_mode(ui->under_up_cont_3, LV_SCROLLBAR_MODE_OFF);

    //Write style for under_up_cont_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->under_up_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_cont_3, 50, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->under_up_cont_3, 168, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->under_up_cont_3, lv_color_hex(0x1fe625), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->under_up_cont_3, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->under_up_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->under_up_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->under_up_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->under_up_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->under_up_cont_3, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->under_up_cont_3, lv_color_hex(0x00ff08), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->under_up_cont_3, 170, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->under_up_cont_3, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(ui->under_up_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(ui->under_up_cont_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_img_3
    ui->under_up_img_3 = lv_img_create(ui->under_up);
    lv_obj_add_flag(ui->under_up_img_3, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->under_up_img_3, &_heart32x32_alpha_32x32_ext);
    lv_img_set_pivot(ui->under_up_img_3, 50,50);
    lv_img_set_angle(ui->under_up_img_3, 0);
    lv_obj_set_pos(ui->under_up_img_3, 39, 32);
    lv_obj_set_size(ui->under_up_img_3, 32, 32);

    //Write style for under_up_img_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->under_up_img_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->under_up_img_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_img_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->under_up_img_3, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_label_1
    ui->under_up_label_1 = lv_label_create(ui->under_up);
    lv_label_set_text(ui->under_up_label_1, "心率64/分");
    lv_label_set_long_mode(ui->under_up_label_1, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->under_up_label_1, 23, 71);
    lv_obj_set_size(ui->under_up_label_1, 63, 11);

    //Write style for under_up_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->under_up_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->under_up_label_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->under_up_label_1, &lv_font_alimama_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->under_up_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->under_up_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->under_up_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->under_up_label_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->under_up_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->under_up_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->under_up_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->under_up_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->under_up_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->under_up_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_img_4
    ui->under_up_img_4 = lv_img_create(ui->under_up);
    lv_obj_add_flag(ui->under_up_img_4, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->under_up_img_4, &_tiwen_alpha_32x32_ext);
    lv_img_set_pivot(ui->under_up_img_4, 50,50);
    lv_img_set_angle(ui->under_up_img_4, 0);
    lv_obj_set_pos(ui->under_up_img_4, 39, 193);
    lv_obj_set_size(ui->under_up_img_4, 32, 32);

    //Write style for under_up_img_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->under_up_img_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->under_up_img_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_img_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->under_up_img_4, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_label_2
    ui->under_up_label_2 = lv_label_create(ui->under_up);
    lv_label_set_text(ui->under_up_label_2, "体温30.1");
    lv_label_set_long_mode(ui->under_up_label_2, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->under_up_label_2, 23, 231);
    lv_obj_set_size(ui->under_up_label_2, 63, 11);

    //Write style for under_up_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->under_up_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->under_up_label_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->under_up_label_2, &lv_font_alimama_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->under_up_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->under_up_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->under_up_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->under_up_label_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->under_up_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->under_up_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->under_up_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->under_up_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->under_up_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->under_up_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_img_5
    ui->under_up_img_5 = lv_img_create(ui->under_up);
    lv_obj_add_flag(ui->under_up_img_5, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->under_up_img_5, &_pa_alpha_32x32_ext);
    lv_img_set_pivot(ui->under_up_img_5, 50,50);
    lv_img_set_angle(ui->under_up_img_5, 0);
    lv_obj_set_pos(ui->under_up_img_5, 174, 196);
    lv_obj_set_size(ui->under_up_img_5, 32, 32);

    //Write style for under_up_img_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->under_up_img_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->under_up_img_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_img_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->under_up_img_5, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_label_3
    ui->under_up_label_3 = lv_label_create(ui->under_up);
    lv_label_set_text(ui->under_up_label_3, "爬行720ppm");
    lv_label_set_long_mode(ui->under_up_label_3, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->under_up_label_3, 162, 234);
    lv_obj_set_size(ui->under_up_label_3, 63, 11);

    //Write style for under_up_label_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->under_up_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->under_up_label_3, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->under_up_label_3, &lv_font_alimama_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->under_up_label_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->under_up_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->under_up_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->under_up_label_3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->under_up_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->under_up_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->under_up_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->under_up_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->under_up_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->under_up_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_cont_4
    ui->under_up_cont_4 = lv_obj_create(ui->under_up);
    lv_obj_set_pos(ui->under_up_cont_4, 145, 18);
    lv_obj_set_size(ui->under_up_cont_4, 25, 25);
    lv_obj_set_scrollbar_mode(ui->under_up_cont_4, LV_SCROLLBAR_MODE_OFF);

    //Write style for under_up_cont_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->under_up_cont_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_cont_4, 50, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->under_up_cont_4, 145, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->under_up_cont_4, lv_color_hex(0x20d8f2), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->under_up_cont_4, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->under_up_cont_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->under_up_cont_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->under_up_cont_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->under_up_cont_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->under_up_cont_4, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->under_up_cont_4, lv_color_hex(0x00cbff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->under_up_cont_4, 170, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->under_up_cont_4, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(ui->under_up_cont_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(ui->under_up_cont_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_cont_5
    ui->under_up_cont_5 = lv_obj_create(ui->under_up);
    lv_obj_set_pos(ui->under_up_cont_5, 195, 69);
    lv_obj_set_size(ui->under_up_cont_5, 25, 25);
    lv_obj_set_scrollbar_mode(ui->under_up_cont_5, LV_SCROLLBAR_MODE_OFF);

    //Write style for under_up_cont_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->under_up_cont_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_cont_5, 50, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->under_up_cont_5, 145, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->under_up_cont_5, lv_color_hex(0x20d8f2), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->under_up_cont_5, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->under_up_cont_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->under_up_cont_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->under_up_cont_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->under_up_cont_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->under_up_cont_5, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui->under_up_cont_5, lv_color_hex(0x00cbff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui->under_up_cont_5, 170, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui->under_up_cont_5, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(ui->under_up_cont_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(ui->under_up_cont_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_img_8
    ui->under_up_img_8 = lv_img_create(ui->under_up);
    lv_obj_add_flag(ui->under_up_img_8, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->under_up_img_8, &_location32x32_alpha_32x32_ext);
    lv_img_set_pivot(ui->under_up_img_8, 50,50);
    lv_img_set_angle(ui->under_up_img_8, 0);
    lv_obj_set_pos(ui->under_up_img_8, 191, 64);
    lv_obj_set_size(ui->under_up_img_8, 32, 32);

    //Write style for under_up_img_8, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->under_up_img_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->under_up_img_8, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_img_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->under_up_img_8, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes under_up_img_7
    ui->under_up_img_7 = lv_img_create(ui->under_up);
    lv_obj_add_flag(ui->under_up_img_7, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->under_up_img_7, &_location32x32_alpha_32x32_ext);
    lv_img_set_pivot(ui->under_up_img_7, 50,50);
    lv_img_set_angle(ui->under_up_img_7, 0);
    lv_obj_set_pos(ui->under_up_img_7, 141, 14);
    lv_obj_set_size(ui->under_up_img_7, 32, 32);

    //Write style for under_up_img_7, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->under_up_img_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->under_up_img_7, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->under_up_img_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->under_up_img_7, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of under_up.


    //Update current screen layout.
    lv_obj_update_layout(ui->under_up);

    //Init events for screen.
    events_init_under_up(ui);
}
