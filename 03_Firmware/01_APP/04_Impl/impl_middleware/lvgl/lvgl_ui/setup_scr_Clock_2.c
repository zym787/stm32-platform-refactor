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



void setup_scr_Clock_2(lv_ui *ui)
{
    //Write codes Clock_2
    ui->Clock_2 = lv_obj_create(NULL);
    lv_obj_set_size(ui->Clock_2, 240, 280);
    lv_obj_set_scrollbar_mode(ui->Clock_2, LV_SCROLLBAR_MODE_OFF);

    //Write style for Clock_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_2, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_img_1
    ui->Clock_2_img_1 = lv_img_create(ui->Clock_2);
    lv_obj_add_flag(ui->Clock_2_img_1, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_2_img_1, &_MDLBG_alpha_240x280_ext);
    lv_img_set_pivot(ui->Clock_2_img_1, 50,50);
    lv_img_set_angle(ui->Clock_2_img_1, 0);
    lv_obj_set_pos(ui->Clock_2_img_1, 0, 0);
    lv_obj_set_size(ui->Clock_2_img_1, 240, 280);

    //Write style for Clock_2_img_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_2_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_2_img_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_img_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_2_img_1, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_label_1
    ui->Clock_2_label_1 = lv_label_create(ui->Clock_2);
    lv_label_set_text(ui->Clock_2_label_1, "19周日");
    lv_label_set_long_mode(ui->Clock_2_label_1, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_2_label_1, 105, 31);
    lv_obj_set_size(ui->Clock_2_label_1, 117, 36);

    //Write style for Clock_2_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_2_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_2_label_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_2_label_1, &lv_font_alimama_36, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_2_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_2_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_2_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_2_label_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_2_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_2_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_2_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_2_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_2_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_2_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_label_2
    ui->Clock_2_label_2 = lv_label_create(ui->Clock_2);
    lv_label_set_text(ui->Clock_2_label_2, "13:24");
    lv_label_set_long_mode(ui->Clock_2_label_2, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_2_label_2, 102, 84);
    lv_obj_set_size(ui->Clock_2_label_2, 122, 36);

    //Write style for Clock_2_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_2_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_2_label_2, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_2_label_2, &lv_font_digitaldreamfatnarrow_36, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_2_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_2_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_2_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_2_label_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_2_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_2_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_2_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_2_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_2_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_2_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_cont_1
    ui->Clock_2_cont_1 = lv_obj_create(ui->Clock_2);
    lv_obj_set_pos(ui->Clock_2_cont_1, -140, 0);
    lv_obj_set_size(ui->Clock_2_cont_1, 120, 280);
    lv_obj_set_scrollbar_mode(ui->Clock_2_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for Clock_2_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_2_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_2_cont_1, 180, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_2_cont_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_2_cont_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_2_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_2_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_2_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_2_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_2_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_btn_3
    ui->Clock_2_btn_3 = lv_btn_create(ui->Clock_2_cont_1);
    ui->Clock_2_btn_3_label = lv_label_create(ui->Clock_2_btn_3);
    lv_label_set_text(ui->Clock_2_btn_3_label, "");
    lv_label_set_long_mode(ui->Clock_2_btn_3_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->Clock_2_btn_3_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->Clock_2_btn_3, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->Clock_2_btn_3_label, LV_PCT(100));
    lv_obj_set_pos(ui->Clock_2_btn_3, 5, 158);
    lv_obj_set_size(ui->Clock_2_btn_3, 110, 50);

    //Write style for Clock_2_btn_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_2_btn_3, 204, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_2_btn_3, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_2_btn_3, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->Clock_2_btn_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_btn_3, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_2_btn_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_2_btn_3, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_2_btn_3, &lv_font_alimama_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_2_btn_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_2_btn_3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_btn_2
    ui->Clock_2_btn_2 = lv_btn_create(ui->Clock_2_cont_1);
    ui->Clock_2_btn_2_label = lv_label_create(ui->Clock_2_btn_2);
    lv_label_set_text(ui->Clock_2_btn_2_label, "");
    lv_label_set_long_mode(ui->Clock_2_btn_2_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->Clock_2_btn_2_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->Clock_2_btn_2, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->Clock_2_btn_2_label, LV_PCT(100));
    lv_obj_set_pos(ui->Clock_2_btn_2, 5, 215);
    lv_obj_set_size(ui->Clock_2_btn_2, 50, 50);

    //Write style for Clock_2_btn_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_2_btn_2, 204, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_2_btn_2, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_2_btn_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->Clock_2_btn_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_btn_2, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_2_btn_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_2_btn_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_2_btn_2, &lv_font_alimama_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_2_btn_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_2_btn_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_btn_1
    ui->Clock_2_btn_1 = lv_btn_create(ui->Clock_2_cont_1);
    ui->Clock_2_btn_1_label = lv_label_create(ui->Clock_2_btn_1);
    lv_label_set_text(ui->Clock_2_btn_1_label, "");
    lv_label_set_long_mode(ui->Clock_2_btn_1_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->Clock_2_btn_1_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->Clock_2_btn_1, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->Clock_2_btn_1_label, LV_PCT(100));
    lv_obj_set_pos(ui->Clock_2_btn_1, 65, 216);
    lv_obj_set_size(ui->Clock_2_btn_1, 50, 50);

    //Write style for Clock_2_btn_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->Clock_2_btn_1, 204, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_2_btn_1, lv_color_hex(0x5a5a5a), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_2_btn_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->Clock_2_btn_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_btn_1, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_2_btn_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_2_btn_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_2_btn_1, &lv_font_alimama_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_2_btn_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_2_btn_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_img_6
    ui->Clock_2_img_6 = lv_img_create(ui->Clock_2_cont_1);
    lv_obj_add_flag(ui->Clock_2_img_6, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_2_img_6, &_BT32_alpha_32x32_ext);
    lv_img_set_pivot(ui->Clock_2_img_6, 50,50);
    lv_img_set_angle(ui->Clock_2_img_6, 0);
    lv_obj_set_pos(ui->Clock_2_img_6, 42, 166);
    lv_obj_set_size(ui->Clock_2_img_6, 32, 32);

    //Write style for Clock_2_img_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_2_img_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_2_img_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_img_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_2_img_6, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_img_5
    ui->Clock_2_img_5 = lv_img_create(ui->Clock_2_cont_1);
    lv_obj_add_flag(ui->Clock_2_img_5, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_2_img_5, &_mianti_0_alpha_32x32_ext);
    lv_img_set_pivot(ui->Clock_2_img_5, 50,50);
    lv_img_set_angle(ui->Clock_2_img_5, 0);
    lv_obj_set_pos(ui->Clock_2_img_5, 13, 224);
    lv_obj_set_size(ui->Clock_2_img_5, 32, 32);

    //Write style for Clock_2_img_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_2_img_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_2_img_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_img_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_2_img_5, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_label_5
    ui->Clock_2_label_5 = lv_label_create(ui->Clock_2_cont_1);
    lv_label_set_text(ui->Clock_2_label_5, "24~28");
    lv_label_set_long_mode(ui->Clock_2_label_5, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_2_label_5, 53, 45);
    lv_obj_set_size(ui->Clock_2_label_5, 58, 19);

    //Write style for Clock_2_label_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_2_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_2_label_5, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_2_label_5, &lv_font_alimama_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_2_label_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_2_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_2_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_2_label_5, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_2_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_2_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_2_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_2_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_2_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_2_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_img_4
    ui->Clock_2_img_4 = lv_img_create(ui->Clock_2_cont_1);
    lv_obj_add_flag(ui->Clock_2_img_4, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_2_img_4, &_zhengdong_0_alpha_32x32_ext);
    lv_img_set_pivot(ui->Clock_2_img_4, 50,50);
    lv_img_set_angle(ui->Clock_2_img_4, 0);
    lv_obj_set_pos(ui->Clock_2_img_4, 72, 223);
    lv_obj_set_size(ui->Clock_2_img_4, 32, 32);

    //Write style for Clock_2_img_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_2_img_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_2_img_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_img_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_2_img_4, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_img_3
    ui->Clock_2_img_3 = lv_img_create(ui->Clock_2_cont_1);
    lv_obj_add_flag(ui->Clock_2_img_3, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_2_img_3, &_copesss_alpha_32x32_ext);
    lv_img_set_pivot(ui->Clock_2_img_3, 50,50);
    lv_img_set_angle(ui->Clock_2_img_3, 0);
    lv_obj_set_pos(ui->Clock_2_img_3, 15, 95);
    lv_obj_set_size(ui->Clock_2_img_3, 32, 32);

    //Write style for Clock_2_img_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_2_img_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_2_img_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_img_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_2_img_3, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_img_2
    ui->Clock_2_img_2 = lv_img_create(ui->Clock_2_cont_1);
    lv_obj_add_flag(ui->Clock_2_img_2, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->Clock_2_img_2, &_weater32x32_alpha_32x32_ext);
    lv_img_set_pivot(ui->Clock_2_img_2, 50,50);
    lv_img_set_angle(ui->Clock_2_img_2, 0);
    lv_obj_set_pos(ui->Clock_2_img_2, 13, 22);
    lv_obj_set_size(ui->Clock_2_img_2, 32, 32);

    //Write style for Clock_2_img_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->Clock_2_img_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->Clock_2_img_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_img_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->Clock_2_img_2, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_label_4
    ui->Clock_2_label_4 = lv_label_create(ui->Clock_2_cont_1);
    lv_label_set_text(ui->Clock_2_label_4, "小雨");
    lv_label_set_long_mode(ui->Clock_2_label_4, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_2_label_4, 60, 23);
    lv_obj_set_size(ui->Clock_2_label_4, 42, 19);

    //Write style for Clock_2_label_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_2_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_2_label_4, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_2_label_4, &lv_font_alimama_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_2_label_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_2_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_2_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_2_label_4, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_2_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_2_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_2_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_2_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_2_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_2_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_label_3
    ui->Clock_2_label_3 = lv_label_create(ui->Clock_2_cont_1);
    lv_label_set_text(ui->Clock_2_label_3, "北纬\n27.6\n东经\n48.5");
    lv_label_set_long_mode(ui->Clock_2_label_3, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->Clock_2_label_3, 58, 79);
    lv_obj_set_size(ui->Clock_2_label_3, 48, 65);

    //Write style for Clock_2_label_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_2_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->Clock_2_label_3, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->Clock_2_label_3, &lv_font_alimama_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->Clock_2_label_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->Clock_2_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->Clock_2_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->Clock_2_label_3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_2_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_2_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_2_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_2_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_2_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_2_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes Clock_2_cont_2
    ui->Clock_2_cont_2 = lv_obj_create(ui->Clock_2);
    lv_obj_set_pos(ui->Clock_2_cont_2, 0, 0);
    lv_obj_set_size(ui->Clock_2_cont_2, 240, 280);
    lv_obj_set_scrollbar_mode(ui->Clock_2_cont_2, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(ui->Clock_2_cont_2, LV_OBJ_FLAG_HIDDEN);

    //Write style for Clock_2_cont_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->Clock_2_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->Clock_2_cont_2, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->Clock_2_cont_2, 140, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->Clock_2_cont_2, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->Clock_2_cont_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->Clock_2_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->Clock_2_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->Clock_2_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->Clock_2_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->Clock_2_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of Clock_2.


    //Update current screen layout.
    lv_obj_update_layout(ui->Clock_2);

    //Init events for screen.
    events_init_Clock_2(ui);
}
