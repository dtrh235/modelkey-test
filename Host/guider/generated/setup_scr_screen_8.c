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



void setup_scr_screen_8(lv_ui *ui)
{
    //Write codes screen_8
    ui->screen_8 = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_8, 240, 320);
    lv_obj_set_scrollbar_mode(ui->screen_8, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_8, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_8, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_cont_1
    ui->screen_8_cont_1 = lv_obj_create(ui->screen_8);
    lv_obj_set_pos(ui->screen_8_cont_1, 0, 0);
    lv_obj_set_size(ui->screen_8_cont_1, 240, 320);
    lv_obj_set_scrollbar_mode(ui->screen_8_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_8_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_8_cont_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_8_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_8_cont_1, lv_color_hex(0xfbb1bd), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_8_cont_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_cont_1, lv_color_hex(0xb1e8fb), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_cont_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_8_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_8_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_8_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_8_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_btn_2
    ui->screen_8_btn_2 = lv_btn_create(ui->screen_8_cont_1);
    ui->screen_8_btn_2_label = lv_label_create(ui->screen_8_btn_2);
    lv_label_set_text(ui->screen_8_btn_2_label, "ESC");
    lv_label_set_long_mode(ui->screen_8_btn_2_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_8_btn_2_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_8_btn_2, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_8_btn_2_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_8_btn_2, 174, 285);
    lv_obj_set_size(ui->screen_8_btn_2, 62, 30);

    //Write style for screen_8_btn_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_8_btn_2, 165, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_btn_2, lv_color_hex(0x009bff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_btn_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_8_btn_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_btn_2, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_btn_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_btn_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_btn_2, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_btn_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_btn_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_label_1
    ui->screen_8_label_1 = lv_label_create(ui->screen_8_cont_1);
    lv_label_set_text(ui->screen_8_label_1, "增加用户");
    lv_label_set_long_mode(ui->screen_8_label_1, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_8_label_1, 5, 12);
    lv_obj_set_size(ui->screen_8_label_1, 219, 30);

    //Write style for screen_8_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_8_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_label_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_label_1, &lv_font_SourceHanSerifSC_Regular_25, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_8_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_8_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_label_1, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_8_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_8_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_8_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_8_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_btn_1
    ui->screen_8_btn_1 = lv_btn_create(ui->screen_8_cont_1);
    ui->screen_8_btn_1_label = lv_label_create(ui->screen_8_btn_1);
    lv_label_set_text(ui->screen_8_btn_1_label, "OK");
    lv_label_set_long_mode(ui->screen_8_btn_1_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_8_btn_1_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_8_btn_1, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_8_btn_1_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_8_btn_1, 1, 286);
    lv_obj_set_size(ui->screen_8_btn_1, 62, 30);

    //Write style for screen_8_btn_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_8_btn_1, 165, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_btn_1, lv_color_hex(0x009bff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_btn_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_8_btn_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_btn_1, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_btn_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_btn_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_btn_1, &lv_font_SourceHanSerifSC_Regular_19, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_btn_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_btn_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_ta_1
    ui->screen_8_ta_1 = lv_textarea_create(ui->screen_8_cont_1);
    lv_textarea_set_text(ui->screen_8_ta_1, "");
    lv_textarea_set_placeholder_text(ui->screen_8_ta_1, "");
    lv_textarea_set_password_bullet(ui->screen_8_ta_1, "*");
    lv_textarea_set_password_mode(ui->screen_8_ta_1, false);
    lv_textarea_set_one_line(ui->screen_8_ta_1, true);
    lv_textarea_set_accepted_chars(ui->screen_8_ta_1, "");
    lv_textarea_set_max_length(ui->screen_8_ta_1, 12);
#if LV_USE_KEYBOARD != 0 || LV_USE_ZH_KEYBOARD != 0
    lv_obj_add_event_cb(ui->screen_8_ta_1, ta_event_cb, LV_EVENT_ALL, ui->g_kb_top_layer);
#endif
    lv_obj_set_pos(ui->screen_8_ta_1, 9, 79);
    lv_obj_set_size(ui->screen_8_ta_1, 198, 35);

    //Write style for screen_8_ta_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->screen_8_ta_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_ta_1, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_ta_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_8_ta_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_ta_1, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_ta_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_ta_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_ta_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_8_ta_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_8_ta_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_8_ta_1, lv_color_hex(0xe6e6e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_8_ta_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_ta_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_8_ta_1, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_8_ta_1, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_8_ta_1, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_ta_1, 4, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_8_ta_1, Part: LV_PART_SCROLLBAR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_8_ta_1, 255, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_ta_1, lv_color_hex(0x2195f6), LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_ta_1, LV_GRAD_DIR_NONE, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_ta_1, 0, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);

    //Write codes screen_8_label_3
    ui->screen_8_label_3 = lv_label_create(ui->screen_8_cont_1);
    lv_label_set_text(ui->screen_8_label_3, "密码（4-10位）");
    lv_label_set_long_mode(ui->screen_8_label_3, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_8_label_3, 12, 121);
    lv_obj_set_size(ui->screen_8_label_3, 144, 20);

    //Write style for screen_8_label_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_8_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_label_3, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_label_3, &lv_font_SourceHanSerifSC_Regular_19, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_label_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_8_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_8_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_label_3, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_8_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_8_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_8_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_8_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_ta_2
    ui->screen_8_ta_2 = lv_textarea_create(ui->screen_8_cont_1);
    lv_textarea_set_text(ui->screen_8_ta_2, "");
    lv_textarea_set_placeholder_text(ui->screen_8_ta_2, "");
    lv_textarea_set_password_bullet(ui->screen_8_ta_2, "*");
    lv_textarea_set_password_mode(ui->screen_8_ta_2, false);
    lv_textarea_set_one_line(ui->screen_8_ta_2, true);
    lv_textarea_set_accepted_chars(ui->screen_8_ta_2, "");
    lv_textarea_set_max_length(ui->screen_8_ta_2, 10);
#if LV_USE_KEYBOARD != 0 || LV_USE_ZH_KEYBOARD != 0
    lv_obj_add_event_cb(ui->screen_8_ta_2, ta_event_cb, LV_EVENT_ALL, ui->g_kb_top_layer);
#endif
    lv_obj_set_pos(ui->screen_8_ta_2, 9, 144);
    lv_obj_set_size(ui->screen_8_ta_2, 198, 35);

    //Write style for screen_8_ta_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->screen_8_ta_2, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_ta_2, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_ta_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_8_ta_2, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_ta_2, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_ta_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_ta_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_ta_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_8_ta_2, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_8_ta_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_8_ta_2, lv_color_hex(0xe6e6e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_8_ta_2, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_ta_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_8_ta_2, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_8_ta_2, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_8_ta_2, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_ta_2, 4, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_8_ta_2, Part: LV_PART_SCROLLBAR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_8_ta_2, 255, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_ta_2, lv_color_hex(0x2195f6), LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_ta_2, LV_GRAD_DIR_NONE, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_ta_2, 0, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);

    //Write codes screen_8_label_4
    ui->screen_8_label_4 = lv_label_create(ui->screen_8_cont_1);
    lv_label_set_text(ui->screen_8_label_4, "指纹录入");
    lv_label_set_long_mode(ui->screen_8_label_4, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_8_label_4, 12, 216);
    lv_obj_set_size(ui->screen_8_label_4, 123, 23);

    //Write style for screen_8_label_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_8_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_label_4, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_label_4, &lv_font_SourceHanSerifSC_Regular_19, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_label_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_8_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_8_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_label_4, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_8_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_8_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_8_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_8_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_btn_3
    ui->screen_8_btn_3 = lv_btn_create(ui->screen_8_cont_1);
    ui->screen_8_btn_3_label = lv_label_create(ui->screen_8_btn_3);
    lv_label_set_text(ui->screen_8_btn_3_label, "录入");
    lv_label_set_long_mode(ui->screen_8_btn_3_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_8_btn_3_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_8_btn_3, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_8_btn_3_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_8_btn_3, 154, 212);
    lv_obj_set_size(ui->screen_8_btn_3, 57, 24);

    //Write style for screen_8_btn_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_8_btn_3, 165, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_btn_3, lv_color_hex(0x009bff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_btn_3, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_8_btn_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_btn_3, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_btn_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_btn_3, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_btn_3, &lv_font_SourceHanSerifSC_Regular_19, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_btn_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_btn_3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_label_7
    ui->screen_8_label_7 = lv_label_create(ui->screen_8_cont_1);
    lv_label_set_text(ui->screen_8_label_7, "NFC录入");
    lv_label_set_long_mode(ui->screen_8_label_7, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_8_label_7, 12, 246);
    lv_obj_set_size(ui->screen_8_label_7, 123, 23);

    //Write style for screen_8_label_7, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_8_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_label_7, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_label_7, &lv_font_SourceHanSerifSC_Regular_19, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_label_7, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_8_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_8_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_label_7, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_8_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_8_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_8_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_8_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_btn_4
    ui->screen_8_btn_4 = lv_btn_create(ui->screen_8_cont_1);
    ui->screen_8_btn_4_label = lv_label_create(ui->screen_8_btn_4);
    lv_label_set_text(ui->screen_8_btn_4_label, "录入");
    lv_label_set_long_mode(ui->screen_8_btn_4_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_8_btn_4_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_8_btn_4, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_8_btn_4_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_8_btn_4, 153, 246);
    lv_obj_set_size(ui->screen_8_btn_4, 57, 24);

    //Write style for screen_8_btn_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_8_btn_4, 165, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_btn_4, lv_color_hex(0x009bff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_btn_4, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_8_btn_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_btn_4, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_btn_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_btn_4, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_btn_4, &lv_font_SourceHanSerifSC_Regular_19, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_btn_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_btn_4, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_label_5
    ui->screen_8_label_5 = lv_label_create(ui->screen_8_cont_1);
    lv_label_set_text(ui->screen_8_label_5, "增加失败\n");
    lv_label_set_long_mode(ui->screen_8_label_5, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_8_label_5, 60, 244);
    lv_obj_set_size(ui->screen_8_label_5, 104, 22);
    lv_obj_add_flag(ui->screen_8_label_5, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->screen_8_label_5, LV_OBJ_FLAG_HIDDEN);

    //Write style for screen_8_label_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_8_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_label_5, lv_color_hex(0xff0027), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_label_5, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_label_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_8_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_8_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_label_5, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_8_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_8_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_8_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_8_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_label_6
    ui->screen_8_label_6 = lv_label_create(ui->screen_8_cont_1);
    lv_label_set_text(ui->screen_8_label_6, "增加成功\n");
    lv_label_set_long_mode(ui->screen_8_label_6, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_8_label_6, 60, 244);
    lv_obj_set_size(ui->screen_8_label_6, 104, 22);
    lv_obj_add_flag(ui->screen_8_label_6, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->screen_8_label_6, LV_OBJ_FLAG_HIDDEN);

    //Write style for screen_8_label_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_8_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_label_6, lv_color_hex(0xff0027), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_label_6, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_label_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_8_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_8_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_label_6, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_8_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_8_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_8_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_8_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_label_2
    ui->screen_8_label_2 = lv_label_create(ui->screen_8);
    lv_label_set_text(ui->screen_8_label_2, "学号/工号（1-12位）");
    lv_label_set_long_mode(ui->screen_8_label_2, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_8_label_2, 13, 57);
    lv_obj_set_size(ui->screen_8_label_2, 187, 22);

    //Write style for screen_8_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_8_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_label_2, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_label_2, &lv_font_SourceHanSerifSC_Regular_19, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_8_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_8_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_8_label_2, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_8_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_8_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_8_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_8_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_8_cb_1
    ui->screen_8_cb_1 = lv_checkbox_create(ui->screen_8);
    lv_checkbox_set_text(ui->screen_8_cb_1, "管理员");
    lv_obj_set_pos(ui->screen_8_cb_1, 13, 186);

    //Write style for screen_8_cb_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_pad_top(ui->screen_8_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_8_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_8_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_8_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_8_cb_1, lv_color_hex(0x0D3055), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_8_cb_1, &lv_font_SourceHanSerifSC_Regular_19, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_8_cb_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_8_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_8_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_cb_1, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_cb_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_8_cb_1, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_pad_all(ui->screen_8_cb_1, 3, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_8_cb_1, 2, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_8_cb_1, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_8_cb_1, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_8_cb_1, LV_BORDER_SIDE_FULL, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_cb_1, 6, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_8_cb_1, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_cb_1, lv_color_hex(0xffffff), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_cb_1, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write codes screen_8_msgbox_1
    static const char * screen_8_msgbox_1_btns[] = {""};
    ui->screen_8_msgbox_1 = lv_msgbox_create(ui->screen_8, "", " 增加用户成功", screen_8_msgbox_1_btns, true);
    lv_obj_set_size(lv_msgbox_get_btns(ui->screen_8_msgbox_1), 0, 49);
    lv_obj_set_pos(ui->screen_8_msgbox_1, 13, 128);
    lv_obj_set_size(ui->screen_8_msgbox_1, 208, 84);
    lv_obj_add_flag(ui->screen_8_msgbox_1, LV_OBJ_FLAG_HIDDEN);

    //Write style for screen_8_msgbox_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_8_msgbox_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_msgbox_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_msgbox_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_8_msgbox_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_msgbox_1, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_msgbox_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_8_msgbox_1_extra_title_main_default
    static lv_style_t style_screen_8_msgbox_1_extra_title_main_default;
    ui_init_style(&style_screen_8_msgbox_1_extra_title_main_default);

    lv_style_set_text_color(&style_screen_8_msgbox_1_extra_title_main_default, lv_color_hex(0x4e4e4e));
    lv_style_set_text_font(&style_screen_8_msgbox_1_extra_title_main_default, &lv_font_montserratMedium_12);
    lv_style_set_text_opa(&style_screen_8_msgbox_1_extra_title_main_default, 255);
    lv_style_set_text_letter_space(&style_screen_8_msgbox_1_extra_title_main_default, 0);
    lv_style_set_text_line_space(&style_screen_8_msgbox_1_extra_title_main_default, 15);
    lv_obj_add_style(lv_msgbox_get_title(ui->screen_8_msgbox_1), &style_screen_8_msgbox_1_extra_title_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_8_msgbox_1_extra_content_main_default
    static lv_style_t style_screen_8_msgbox_1_extra_content_main_default;
    ui_init_style(&style_screen_8_msgbox_1_extra_content_main_default);

    lv_style_set_text_color(&style_screen_8_msgbox_1_extra_content_main_default, lv_color_hex(0x4e4e4e));
    lv_style_set_text_font(&style_screen_8_msgbox_1_extra_content_main_default, &lv_font_SourceHanSerifSC_Regular_20);
    lv_style_set_text_opa(&style_screen_8_msgbox_1_extra_content_main_default, 255);
    lv_style_set_text_letter_space(&style_screen_8_msgbox_1_extra_content_main_default, 0);
    lv_style_set_text_line_space(&style_screen_8_msgbox_1_extra_content_main_default, 10);
    lv_obj_add_style(lv_msgbox_get_text(ui->screen_8_msgbox_1), &style_screen_8_msgbox_1_extra_content_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_8_msgbox_1_extra_btns_items_default
    static lv_style_t style_screen_8_msgbox_1_extra_btns_items_default;
    ui_init_style(&style_screen_8_msgbox_1_extra_btns_items_default);

    lv_style_set_bg_opa(&style_screen_8_msgbox_1_extra_btns_items_default, 255);
    lv_style_set_bg_color(&style_screen_8_msgbox_1_extra_btns_items_default, lv_color_hex(0xe6e6e6));
    lv_style_set_bg_grad_dir(&style_screen_8_msgbox_1_extra_btns_items_default, LV_GRAD_DIR_NONE);
    lv_style_set_border_width(&style_screen_8_msgbox_1_extra_btns_items_default, 0);
    lv_style_set_radius(&style_screen_8_msgbox_1_extra_btns_items_default, 10);
    lv_style_set_text_color(&style_screen_8_msgbox_1_extra_btns_items_default, lv_color_hex(0x4e4e4e));
    lv_style_set_text_font(&style_screen_8_msgbox_1_extra_btns_items_default, &lv_font_montserratMedium_12);
    lv_style_set_text_opa(&style_screen_8_msgbox_1_extra_btns_items_default, 255);
    lv_obj_add_style(lv_msgbox_get_btns(ui->screen_8_msgbox_1), &style_screen_8_msgbox_1_extra_btns_items_default, LV_PART_ITEMS|LV_STATE_DEFAULT);

    //Write codes screen_8_msgbox_2
    static const char * screen_8_msgbox_2_btns[] = {""};
    ui->screen_8_msgbox_2 = lv_msgbox_create(ui->screen_8, "", " 增加用户失败", screen_8_msgbox_2_btns, true);
    lv_obj_set_size(lv_msgbox_get_btns(ui->screen_8_msgbox_2), 0, 49);
    lv_obj_set_pos(ui->screen_8_msgbox_2, 13, 128);
    lv_obj_set_size(ui->screen_8_msgbox_2, 208, 84);
    lv_obj_add_flag(ui->screen_8_msgbox_2, LV_OBJ_FLAG_HIDDEN);

    //Write style for screen_8_msgbox_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_8_msgbox_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_8_msgbox_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_8_msgbox_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_8_msgbox_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_8_msgbox_2, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_8_msgbox_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_8_msgbox_2_extra_title_main_default
    static lv_style_t style_screen_8_msgbox_2_extra_title_main_default;
    ui_init_style(&style_screen_8_msgbox_2_extra_title_main_default);

    lv_style_set_text_color(&style_screen_8_msgbox_2_extra_title_main_default, lv_color_hex(0x4e4e4e));
    lv_style_set_text_font(&style_screen_8_msgbox_2_extra_title_main_default, &lv_font_montserratMedium_12);
    lv_style_set_text_opa(&style_screen_8_msgbox_2_extra_title_main_default, 255);
    lv_style_set_text_letter_space(&style_screen_8_msgbox_2_extra_title_main_default, 0);
    lv_style_set_text_line_space(&style_screen_8_msgbox_2_extra_title_main_default, 15);
    lv_obj_add_style(lv_msgbox_get_title(ui->screen_8_msgbox_2), &style_screen_8_msgbox_2_extra_title_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_8_msgbox_2_extra_content_main_default
    static lv_style_t style_screen_8_msgbox_2_extra_content_main_default;
    ui_init_style(&style_screen_8_msgbox_2_extra_content_main_default);

    lv_style_set_text_color(&style_screen_8_msgbox_2_extra_content_main_default, lv_color_hex(0x4e4e4e));
    lv_style_set_text_font(&style_screen_8_msgbox_2_extra_content_main_default, &lv_font_SourceHanSerifSC_Regular_20);
    lv_style_set_text_opa(&style_screen_8_msgbox_2_extra_content_main_default, 255);
    lv_style_set_text_letter_space(&style_screen_8_msgbox_2_extra_content_main_default, 0);
    lv_style_set_text_line_space(&style_screen_8_msgbox_2_extra_content_main_default, 10);
    lv_obj_add_style(lv_msgbox_get_text(ui->screen_8_msgbox_2), &style_screen_8_msgbox_2_extra_content_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_8_msgbox_2_extra_btns_items_default
    static lv_style_t style_screen_8_msgbox_2_extra_btns_items_default;
    ui_init_style(&style_screen_8_msgbox_2_extra_btns_items_default);

    lv_style_set_bg_opa(&style_screen_8_msgbox_2_extra_btns_items_default, 255);
    lv_style_set_bg_color(&style_screen_8_msgbox_2_extra_btns_items_default, lv_color_hex(0xe6e6e6));
    lv_style_set_bg_grad_dir(&style_screen_8_msgbox_2_extra_btns_items_default, LV_GRAD_DIR_NONE);
    lv_style_set_border_width(&style_screen_8_msgbox_2_extra_btns_items_default, 0);
    lv_style_set_radius(&style_screen_8_msgbox_2_extra_btns_items_default, 10);
    lv_style_set_text_color(&style_screen_8_msgbox_2_extra_btns_items_default, lv_color_hex(0x4e4e4e));
    lv_style_set_text_font(&style_screen_8_msgbox_2_extra_btns_items_default, &lv_font_montserratMedium_12);
    lv_style_set_text_opa(&style_screen_8_msgbox_2_extra_btns_items_default, 255);
    lv_obj_add_style(lv_msgbox_get_btns(ui->screen_8_msgbox_2), &style_screen_8_msgbox_2_extra_btns_items_default, LV_PART_ITEMS|LV_STATE_DEFAULT);

    //The custom code of screen_8.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_8);

}
