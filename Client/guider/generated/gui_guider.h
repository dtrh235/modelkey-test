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
#include "src/widgets/lv_dclock.h"

typedef struct
{
  
	lv_obj_t *g_kb_top_layer;
	lv_obj_t *screen;
	bool screen_del;
	lv_obj_t *screen_img_3;
	lv_obj_t *screen_img_2;
	lv_obj_t *screen_btn_2;
	lv_obj_t *screen_btn_2_label;
	lv_obj_t *screen_digital_clock_1;
	lv_obj_t *screen_datetext_1;
	lv_obj_t *screen_btn_3;
	lv_obj_t *screen_btn_3_label;
	lv_obj_t *screen_1;
	bool screen_1_del;
	lv_obj_t *screen_1_cont_1;
	lv_obj_t *screen_1_label_2;
	lv_obj_t *screen_1_ta_2;
	lv_obj_t *screen_1_btn_2;
	lv_obj_t *screen_1_btn_2_label;
	lv_obj_t *screen_1_btn_3;
	lv_obj_t *screen_1_btn_3_label;
	lv_obj_t *screen_1_label_1;
	lv_obj_t *screen_1_ta_1;
	lv_obj_t *screen_1_label_3;
	lv_obj_t *screen_2;
	bool screen_2_del;
	lv_obj_t *screen_2_cont_1;
	lv_obj_t *screen_2_label_1;
	lv_obj_t *screen_2_ta_1;
	lv_obj_t *screen_2_btn_2;
	lv_obj_t *screen_2_btn_2_label;
	lv_obj_t *screen_2_btn_1;
	lv_obj_t *screen_2_btn_1_label;
	lv_obj_t *screen_2_label_2;
	lv_obj_t *screen_2_ta_2;
	lv_obj_t *screen_2_label_3;
	lv_obj_t *screen_3;
	bool screen_3_del;
	lv_obj_t *screen_3_cont_1;
	lv_obj_t *screen_3_btn_1;
	lv_obj_t *screen_3_btn_1_label;
	lv_obj_t *screen_3_label_1;
	lv_obj_t *screen_3_btn_4;
	lv_obj_t *screen_3_btn_4_label;
	lv_obj_t *screen_3_btn_5;
	lv_obj_t *screen_3_btn_5_label;
	lv_obj_t *screen_3_btn_6;
	lv_obj_t *screen_3_btn_6_label;
	lv_obj_t *screen_3_btn_3;
	lv_obj_t *screen_3_btn_3_label;
	lv_obj_t *screen_4;
	bool screen_4_del;
	lv_obj_t *screen_4_cont_1;
	lv_obj_t *screen_4_btn_4;
	lv_obj_t *screen_4_btn_4_label;
	lv_obj_t *screen_4_label_1;
	lv_obj_t *screen_4_table_1;
	lv_obj_t *screen_5;
	bool screen_5_del;
	lv_obj_t *screen_5_cont_1;
	lv_obj_t *screen_5_btn_1;
	lv_obj_t *screen_5_btn_1_label;
	lv_obj_t *screen_5_label_1;
	lv_obj_t *screen_5_btn_2;
	lv_obj_t *screen_5_btn_2_label;
	lv_obj_t *screen_5_ta_1;
	lv_obj_t *screen_5_label_3;
	lv_obj_t *screen_5_label_2;
	lv_obj_t *screen_6;
	bool screen_6_del;
	lv_obj_t *screen_6_cont_1;
	lv_obj_t *screen_6_btn_2;
	lv_obj_t *screen_6_btn_2_label;
	lv_obj_t *screen_6_label_1;
	lv_obj_t *screen_6_label_3;
	lv_obj_t *screen_6_label_4;
	lv_obj_t *screen_6_label_5;
	lv_obj_t *screen_6_btn_3;
	lv_obj_t *screen_6_btn_3_label;
	lv_obj_t *screen_6_btn_4;
	lv_obj_t *screen_6_btn_4_label;
	lv_obj_t *screen_6_btn_6;
	lv_obj_t *screen_6_btn_6_label;
	lv_obj_t *screen_6_label_6;
	lv_obj_t *screen_6_btn_7;
	lv_obj_t *screen_6_btn_7_label;
	lv_obj_t *screen_6_label_2;
	lv_obj_t *screen_6_ddlist_1;
	lv_obj_t *screen_7;
	bool screen_7_del;
	lv_obj_t *screen_7_cont_1;
	lv_obj_t *screen_7_btn_2;
	lv_obj_t *screen_7_btn_2_label;
	lv_obj_t *screen_7_label_1;
	lv_obj_t *screen_7_btn_1;
	lv_obj_t *screen_7_btn_1_label;
	lv_obj_t *screen_7_ta_1;
	lv_obj_t *screen_7_label_3;
	lv_obj_t *screen_7_msgbox_3;
	lv_obj_t *screen_7_msgbox_2;
	lv_obj_t *screen_7_msgbox_1;
	lv_obj_t *screen_8;
	bool screen_8_del;
	lv_obj_t *screen_8_cont_1;
	lv_obj_t *screen_8_btn_2;
	lv_obj_t *screen_8_btn_2_label;
	lv_obj_t *screen_8_label_1;
	lv_obj_t *screen_8_btn_1;
	lv_obj_t *screen_8_btn_1_label;
	lv_obj_t *screen_8_ta_1;
	lv_obj_t *screen_8_label_3;
	lv_obj_t *screen_8_ta_2;
	lv_obj_t *screen_8_label_4;
	lv_obj_t *screen_8_btn_3;
	lv_obj_t *screen_8_btn_3_label;
	lv_obj_t *screen_8_label_5;
	lv_obj_t *screen_8_label_6;
	lv_obj_t *screen_8_label_7;
	lv_obj_t *screen_8_btn_4;
	lv_obj_t *screen_8_btn_4_label;
	lv_obj_t *screen_8_label_2;
	lv_obj_t *screen_8_cb_1;
	lv_obj_t *screen_8_msgbox_1;
	lv_obj_t *screen_8_msgbox_2;
	lv_obj_t *screen_9;
	bool screen_9_del;
	lv_obj_t *screen_9_cont_1;
	lv_obj_t *screen_9_btn_4;
	lv_obj_t *screen_9_btn_4_label;
	lv_obj_t *screen_9_label_4;
	lv_obj_t *screen_9_btn_5;
	lv_obj_t *screen_9_btn_5_label;
	lv_obj_t *screen_9_btn_6;
	lv_obj_t *screen_9_btn_6_label;
	lv_obj_t *screen_10;
	bool screen_10_del;
	lv_obj_t *screen_10_cont_1;
	lv_obj_t *screen_10_btn_3;
	lv_obj_t *screen_10_btn_3_label;
	lv_obj_t *screen_10_label_1;
	lv_obj_t *screen_10_btn_2;
	lv_obj_t *screen_10_btn_2_label;
	lv_obj_t *screen_10_btn_1;
	lv_obj_t *screen_10_btn_1_label;
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


void setup_scr_screen(lv_ui *ui);
void setup_scr_screen_1(lv_ui *ui);
void setup_scr_screen_2(lv_ui *ui);
void setup_scr_screen_3(lv_ui *ui);
void setup_scr_screen_4(lv_ui *ui);
void setup_scr_screen_5(lv_ui *ui);
void setup_scr_screen_6(lv_ui *ui);
void setup_scr_screen_7(lv_ui *ui);
void setup_scr_screen_8(lv_ui *ui);
void setup_scr_screen_9(lv_ui *ui);
void setup_scr_screen_10(lv_ui *ui);
LV_IMG_DECLARE(_3_alpha_240x320);
LV_IMG_DECLARE(_2_alpha_82x77);

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20)
LV_FONT_DECLARE(lv_font_montserratMedium_51)
LV_FONT_DECLARE(lv_font_montserratMedium_19)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_21)
LV_FONT_DECLARE(lv_font_montserratMedium_18)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_16)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_25)
LV_FONT_DECLARE(lv_font_montserratMedium_12)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_19)
LV_FONT_DECLARE(lv_font_montserratMedium_16)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_18)
LV_FONT_DECLARE(lv_font_montserratMedium_17)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_30)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_26)


#ifdef __cplusplus
}
#endif
#endif
