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

#include "lvgl.h"

typedef struct
{
  
	lv_obj_t *Clock_1;
	bool Clock_1_del;
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
	lv_obj_t *Clock_2;
	bool Clock_2_del;
	lv_obj_t *Clock_2_img_1;
	lv_obj_t *Clock_2_label_1;
	lv_obj_t *Clock_2_label_2;
	lv_obj_t *Clock_3;
	bool Clock_3_del;
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
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, bool new_scr_del, bool * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_scr_load_anim_t anim_type, uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void * var, int32_t duration, int32_t delay, int32_t start_value, int32_t end_value, lv_anim_path_cb_t path_cb,
                       uint16_t repeat_cnt, uint32_t repeat_delay, uint32_t playback_time, uint32_t playback_delay,
                       lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_ready_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_ui(lv_ui *ui);

void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;


void setup_scr_Clock_1(lv_ui *ui);
void setup_scr_Clock_2(lv_ui *ui);
void setup_scr_Clock_3(lv_ui *ui);
LV_IMG_DECLARE(_sheshidu_alpha_10x10);
LV_IMG_DECLARE(_wather16x16_alpha_16x16);
LV_IMG_DECLARE(_heart16x16_alpha_16x16);
LV_IMG_DECLARE(_KLL16x16_alpha_16x16);
LV_IMG_DECLARE(_foot16x16_alpha_16x16);
LV_IMG_DECLARE(_MDLBG_alpha_240x280);       /* declaration only -- pixel data lives on W25Q64, not in firmware */
LV_IMG_DECLARE(_MDLBG_alpha_240x280_ext);   /* W25Q64-streamed mirror, defined in storage_assets.c */

LV_IMG_DECLARE(_biaopan1_200x200);          /* declaration only -- pixel data on W25Q64 */
LV_IMG_DECLARE(_biaopan1_200x200_ext);      /* W25Q64-streamed mirror */
LV_IMG_DECLARE(_time_alpha_50x8);
LV_IMG_DECLARE(_time_alpha_50x8_ext);       /* RAM mirror seeded from W25Q64 */
LV_IMG_DECLARE(_fen_alpha_80x8);
LV_IMG_DECLARE(_fen_alpha_80x8_ext);        /* RAM mirror seeded from W25Q64 */
LV_IMG_DECLARE(_miao_alpha_70x5);
LV_IMG_DECLARE(_miao_alpha_70x5_ext);       /* RAM mirror seeded from W25Q64 (currently unused by UI) */
LV_IMG_DECLARE(_watchdight1_alpha_60x60);       /* declaration only -- pixel data on W25Q64 */
LV_IMG_DECLARE(_watchdight1_alpha_60x60_ext);   /* W25Q64-streamed mirror */
LV_IMG_DECLARE(_watchdight2_alpha_60x60);
LV_IMG_DECLARE(_watchdight2_alpha_60x60_ext);
LV_IMG_DECLARE(_watchdight3_alpha_60x60);
LV_IMG_DECLARE(_watchdight3_alpha_60x60_ext);
LV_IMG_DECLARE(_Ellipse_alpha_40x40);
LV_IMG_DECLARE(_Stime_alpha_16x8);
LV_IMG_DECLARE(_Sfen_alpha_21x6);

LV_FONT_DECLARE(lv_font_interttf_24)
LV_FONT_DECLARE(lv_font_interttf_10)
LV_FONT_DECLARE(lv_font_alimama_12)
LV_FONT_DECLARE(lv_font_alimama_16)
/* Dropped to fit Flash budget (would add ~404 KB total):
 *   lv_font_interttf_82, lv_font_alimama_36, lv_font_digitaldreamfatnarrow_36.
 * setup_scr_Clock_1/2.c references downgraded to surviving fonts; re-add via
 * SquareLine with a glyph-subset slim font when more Flash is available. */


#ifdef __cplusplus
}
#endif
#endif
