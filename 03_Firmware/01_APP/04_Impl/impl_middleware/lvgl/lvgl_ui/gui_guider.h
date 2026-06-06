/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "board_types.h"
#include "lvgl.h"

typedef struct
{
  
	lv_obj_t *Clock_1;
	BOOL Clock_1_del;
	lv_obj_t *Clock_1_label_1;
	lv_obj_t *Clock_1_arc_1;
	lv_obj_t *Clock_1_arc_2;
	lv_obj_t *Clock_1_arc_3;
	lv_obj_t *Clock_1_arc_4;
	lv_obj_t *Clock_1_img_1;
	lv_obj_t *Clock_1_img_2;
	lv_obj_t *Clock_1_img_3;
	lv_obj_t *Clock_1_img_4;
	lv_obj_t *Clock_1_img_5;
	lv_obj_t *Clock_1_label_2;
	lv_obj_t *Clock_1_label_3;
	lv_obj_t *Clock_1_label_4;
	lv_obj_t *Clock_1_label_5;
	lv_obj_t *Clock_1_label_6;
	lv_obj_t *Clock_1_label_7;
	lv_obj_t *Clock_1_label_8;
	lv_obj_t *Clock_1_label_9;
	lv_obj_t *Clock_1_cont_1;
	lv_obj_t *Clock_1_btn_1;
	lv_obj_t *Clock_1_btn_1_label;
	lv_obj_t *Clock_1_btn_2;
	lv_obj_t *Clock_1_btn_2_label;
	lv_obj_t *Clock_1_btn_3;
	lv_obj_t *Clock_1_btn_3_label;
	lv_obj_t *Clock_1_img_6;
	lv_obj_t *Clock_1_img_7;
	lv_obj_t *Clock_1_label_11;
	lv_obj_t *Clock_1_img_8;
	lv_obj_t *Clock_1_img_9;
	lv_obj_t *Clock_1_img_10;
	lv_obj_t *Clock_1_label_10;
	lv_obj_t *Clock_1_label_12;
	lv_obj_t *Clock_1_cont_2;
	lv_obj_t *Clock_2;
	BOOL Clock_2_del;
	lv_obj_t *Clock_2_img_1;
	lv_obj_t *Clock_2_label_1;
	lv_obj_t *Clock_2_label_2;
	lv_obj_t *Clock_2_cont_1;
	lv_obj_t *Clock_2_btn_3;
	lv_obj_t *Clock_2_btn_3_label;
	lv_obj_t *Clock_2_btn_2;
	lv_obj_t *Clock_2_btn_2_label;
	lv_obj_t *Clock_2_btn_1;
	lv_obj_t *Clock_2_btn_1_label;
	lv_obj_t *Clock_2_img_6;
	lv_obj_t *Clock_2_img_5;
	lv_obj_t *Clock_2_label_5;
	lv_obj_t *Clock_2_img_4;
	lv_obj_t *Clock_2_img_3;
	lv_obj_t *Clock_2_img_2;
	lv_obj_t *Clock_2_label_4;
	lv_obj_t *Clock_2_label_3;
	lv_obj_t *Clock_2_cont_2;
	lv_obj_t *Clock_3;
	BOOL Clock_3_del;
	lv_obj_t *Clock_3_analog_clock_1;
	lv_obj_t *Clock_3_img_1;
	lv_obj_t *Clock_3_img_2;
	lv_obj_t *Clock_3_img_3;
	lv_obj_t *Clock_3_label_1;
	lv_obj_t *Clock_3_label_2;
	lv_obj_t *Clock_3_cont_1;
	lv_obj_t *Clock_3_cont_2;
	lv_obj_t *Clock_3_img_4;
	lv_obj_t *Clock_3_img_5;
	lv_obj_t *Clock_3_img_6;
	lv_obj_t *Clock_3_cont_3;
	lv_obj_t *Clock_3_btn_3;
	lv_obj_t *Clock_3_btn_3_label;
	lv_obj_t *Clock_3_btn_2;
	lv_obj_t *Clock_3_btn_2_label;
	lv_obj_t *Clock_3_btn_1;
	lv_obj_t *Clock_3_btn_1_label;
	lv_obj_t *Clock_3_img_11;
	lv_obj_t *Clock_3_img_10;
	lv_obj_t *Clock_3_label_5;
	lv_obj_t *Clock_3_img_9;
	lv_obj_t *Clock_3_img_8;
	lv_obj_t *Clock_3_img_7;
	lv_obj_t *Clock_3_label_4;
	lv_obj_t *Clock_3_label_3;
	lv_obj_t *Clock_3_cont_4;
	lv_obj_t *top_lap;
	BOOL top_lap_del;
	lv_obj_t *top_lap_label_1;
	lv_obj_t *top_lap_img_1;
	lv_obj_t *top_lap_cont_1;
	lv_obj_t *top_lap_img_2;
	lv_obj_t *top_lap_cont_2;
	lv_obj_t *top_lap_img_3;
	lv_obj_t *top_lap_cont_3;
	lv_obj_t *top_lap_img_4;
	lv_obj_t *top_lap_cont_4;
	lv_obj_t *top_lap_img_5;
	lv_obj_t *top_lap_slider_1;
	lv_obj_t *under_up;
	BOOL under_up_del;
	lv_obj_t *under_up_img_1;
	lv_obj_t *under_up_img_6;
	lv_obj_t *under_up_img_2;
	lv_obj_t *under_up_cont_1;
	lv_obj_t *under_up_cont_2;
	lv_obj_t *under_up_cont_3;
	lv_obj_t *under_up_img_3;
	lv_obj_t *under_up_label_1;
	lv_obj_t *under_up_img_4;
	lv_obj_t *under_up_label_2;
	lv_obj_t *under_up_img_5;
	lv_obj_t *under_up_label_3;
	lv_obj_t *under_up_cont_4;
	lv_obj_t *under_up_cont_5;
	lv_obj_t *under_up_img_8;
	lv_obj_t *under_up_img_7;
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, BOOL new_scr_del, BOOL * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_scr_load_anim_t anim_type, UINT32_t time, UINT32_t delay, BOOL is_clean, BOOL auto_del);

void ui_animation(void * var, INT32_t duration, INT32_t delay, INT32_t start_value, INT32_t end_value, lv_anim_path_cb_t path_cb,
                       UINT16_t repeat_cnt, UINT32_t repeat_delay, UINT32_t playback_time, UINT32_t playback_delay,
                       lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_ready_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_ui(lv_ui *ui);

void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;


void setup_scr_Clock_1(lv_ui *ui);
void setup_scr_Clock_2(lv_ui *ui);
void setup_scr_Clock_3(lv_ui *ui);
void setup_scr_top_lap(lv_ui *ui);
void setup_scr_under_up(lv_ui *ui);
LV_IMG_DECLARE(_sheshidu_alpha_10x10);
LV_IMG_DECLARE(_wather16x16_alpha_16x16);
LV_IMG_DECLARE(_heart16x16_alpha_16x16);
LV_IMG_DECLARE(_KLL16x16_alpha_16x16);
LV_IMG_DECLARE(_foot16x16_alpha_16x16);
LV_IMG_DECLARE(_BT32_alpha_32x32);
LV_IMG_DECLARE(_mianti_0_alpha_32x32);
LV_IMG_DECLARE(_zhengdong_0_alpha_32x32);
LV_IMG_DECLARE(_copesss_alpha_32x32);
LV_IMG_DECLARE(_weater32x32_alpha_32x32);
LV_IMG_DECLARE(_MDLBG_alpha_240x280);
LV_IMG_DECLARE(_BT32_alpha_32x32);
LV_IMG_DECLARE(_mianti_0_alpha_32x32);
LV_IMG_DECLARE(_zhengdong_0_alpha_32x32);
LV_IMG_DECLARE(_copesss_alpha_32x32);
LV_IMG_DECLARE(_weater32x32_alpha_32x32);

LV_IMG_DECLARE(_biaopan1_200x200);
LV_IMG_DECLARE(_time_alpha_50x8);
LV_IMG_DECLARE(_fen_alpha_80x8);
LV_IMG_DECLARE(_miao_alpha_70x5);
LV_IMG_DECLARE(_watchdight1_alpha_60x60);
LV_IMG_DECLARE(_watchdight3_alpha_60x60);
LV_IMG_DECLARE(_watchdight2_alpha_60x60);
LV_IMG_DECLARE(_Ellipse_alpha_40x40);
LV_IMG_DECLARE(_Stime_alpha_16x8);
LV_IMG_DECLARE(_Sfen_alpha_21x6);
LV_IMG_DECLARE(_BT32_alpha_32x32);
LV_IMG_DECLARE(_mianti_0_alpha_32x32);
LV_IMG_DECLARE(_zhengdong_0_alpha_32x32);
LV_IMG_DECLARE(_copesss_alpha_32x32);
LV_IMG_DECLARE(_weater32x32_alpha_32x32);
LV_IMG_DECLARE(_power_hight_alpha_32x32);
LV_IMG_DECLARE(_BT32_alpha_32x32);
LV_IMG_DECLARE(_location_alpha_32x32);
LV_IMG_DECLARE(_taiwan_alpha_32x32);
LV_IMG_DECLARE(_nfc_alpha_32x32);

LV_IMG_DECLARE(_liangdu_47x47);
LV_IMG_DECLARE(_ZNZBG_alpha_100x100);
LV_IMG_DECLARE(_arw_alpha_50x40);
LV_IMG_DECLARE(_ZNZ_alpha_50x50);
LV_IMG_DECLARE(_heart32x32_alpha_32x32);
LV_IMG_DECLARE(_tiwen_alpha_32x32);
LV_IMG_DECLARE(_pa_alpha_32x32);
LV_IMG_DECLARE(_location32x32_alpha_32x32);
LV_IMG_DECLARE(_location32x32_alpha_32x32);

LV_IMG_DECLARE(_sheshidu_alpha_10x10_ext);
LV_IMG_DECLARE(_wather16x16_alpha_16x16_ext);
LV_IMG_DECLARE(_heart16x16_alpha_16x16_ext);
LV_IMG_DECLARE(_KLL16x16_alpha_16x16_ext);
LV_IMG_DECLARE(_foot16x16_alpha_16x16_ext);
LV_IMG_DECLARE(_BT32_alpha_32x32_ext);
LV_IMG_DECLARE(_mianti_0_alpha_32x32_ext);
LV_IMG_DECLARE(_zhengdong_0_alpha_32x32_ext);
LV_IMG_DECLARE(_copesss_alpha_32x32_ext);
LV_IMG_DECLARE(_weater32x32_alpha_32x32_ext);
LV_IMG_DECLARE(_MDLBG_alpha_240x280_ext);
LV_IMG_DECLARE(_biaopan1_200x200_ext);
LV_IMG_DECLARE(_time_alpha_50x8_ext);
LV_IMG_DECLARE(_fen_alpha_80x8_ext);
LV_IMG_DECLARE(_miao_alpha_70x5_ext);
LV_IMG_DECLARE(_watchdight1_alpha_60x60_ext);
LV_IMG_DECLARE(_watchdight2_alpha_60x60_ext);
LV_IMG_DECLARE(_watchdight3_alpha_60x60_ext);
LV_IMG_DECLARE(_Ellipse_alpha_40x40_ext);
LV_IMG_DECLARE(_Stime_alpha_16x8_ext);
LV_IMG_DECLARE(_Sfen_alpha_21x6_ext);
LV_IMG_DECLARE(_power_hight_alpha_32x32_ext);
LV_IMG_DECLARE(_location_alpha_32x32_ext);
LV_IMG_DECLARE(_taiwan_alpha_32x32_ext);
LV_IMG_DECLARE(_nfc_alpha_32x32_ext);
LV_IMG_DECLARE(_liangdu_47x47_ext);
LV_IMG_DECLARE(_ZNZBG_alpha_100x100_ext);
LV_IMG_DECLARE(_arw_alpha_50x40_ext);
LV_IMG_DECLARE(_ZNZ_alpha_50x50_ext);
LV_IMG_DECLARE(_heart32x32_alpha_32x32_ext);
LV_IMG_DECLARE(_tiwen_alpha_32x32_ext);
LV_IMG_DECLARE(_pa_alpha_32x32_ext);
LV_IMG_DECLARE(_location32x32_alpha_32x32_ext);

LV_FONT_DECLARE(lv_font_interttf_24)
LV_FONT_DECLARE(lv_font_interttf_10)
LV_FONT_DECLARE(lv_font_interttf_82)
LV_FONT_DECLARE(lv_font_alimama_16)
LV_FONT_DECLARE(lv_font_alimama_36)
LV_FONT_DECLARE(lv_font_digitaldreamfatnarrow_36)
LV_FONT_DECLARE(lv_font_alimama_12)
LV_FONT_DECLARE(lv_font_alimama_10)


#ifdef __cplusplus
}
#endif
#endif
