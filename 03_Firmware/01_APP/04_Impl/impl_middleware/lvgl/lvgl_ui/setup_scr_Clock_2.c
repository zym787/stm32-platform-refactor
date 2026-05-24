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
    lv_obj_set_style_text_font(ui->Clock_2_label_1, &lv_font_alimama_16  /* downgraded from alimama_36 to fit Flash budget */, LV_PART_MAIN|LV_STATE_DEFAULT);
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
    lv_obj_set_style_text_font(ui->Clock_2_label_2, &lv_font_alimama_16  /* downgraded from digitaldreamfatnarrow_36 to fit Flash budget */, LV_PART_MAIN|LV_STATE_DEFAULT);
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

    //The custom code of Clock_2.


    //Update current screen layout.
    lv_obj_update_layout(ui->Clock_2);

    //Init events for screen.
    events_init_Clock_2(ui);
}
