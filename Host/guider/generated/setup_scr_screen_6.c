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



void setup_scr_screen_6(lv_ui *ui)
{
    //Write codes screen_6
    ui->screen_6 = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_6, 240, 320);
    lv_obj_set_scrollbar_mode(ui->screen_6, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_cont_1
    ui->screen_6_cont_1 = lv_obj_create(ui->screen_6);
    lv_obj_set_pos(ui->screen_6_cont_1, 0, 0);
    lv_obj_set_size(ui->screen_6_cont_1, 240, 320);
    lv_obj_set_scrollbar_mode(ui->screen_6_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_6_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_6_cont_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_6_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_6_cont_1, lv_color_hex(0xfbb1bd), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_6_cont_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_6_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_6_cont_1, lv_color_hex(0xb1e8fb), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_6_cont_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_6_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_6_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_6_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_6_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_btn_2
    ui->screen_6_btn_2 = lv_btn_create(ui->screen_6_cont_1);
    ui->screen_6_btn_2_label = lv_label_create(ui->screen_6_btn_2);
    lv_label_set_text(ui->screen_6_btn_2_label, "ESC");
    lv_label_set_long_mode(ui->screen_6_btn_2_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_6_btn_2_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_6_btn_2, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_6_btn_2_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_6_btn_2, 82, 282);
    lv_obj_set_size(ui->screen_6_btn_2, 62, 30);

    //Write style for screen_6_btn_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_6_btn_2, 165, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_6_btn_2, lv_color_hex(0x009bff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_6_btn_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_6_btn_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_btn_2, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_btn_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_6_btn_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_btn_2, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_btn_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_6_btn_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_label_1
    ui->screen_6_label_1 = lv_label_create(ui->screen_6_cont_1);
    lv_label_set_text(ui->screen_6_label_1, "查找(更改)用户");
    lv_label_set_long_mode(ui->screen_6_label_1, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_6_label_1, 5, 12);
    lv_obj_set_size(ui->screen_6_label_1, 219, 30);

    //Write style for screen_6_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_6_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_6_label_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_label_1, &lv_font_SourceHanSerifSC_Regular_25, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_6_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_6_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_6_label_1, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_6_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_6_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_6_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_6_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_6_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_label_3
    ui->screen_6_label_3 = lv_label_create(ui->screen_6_cont_1);
    lv_label_set_text(ui->screen_6_label_3, "密码：");
    lv_label_set_long_mode(ui->screen_6_label_3, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_6_label_3, 6, 111);
    lv_obj_set_size(ui->screen_6_label_3, 92, 24);

    //Write style for screen_6_label_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_6_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_6_label_3, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_label_3, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_label_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_6_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_6_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_6_label_3, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_6_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_6_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_6_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_6_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_6_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_label_4
    ui->screen_6_label_4 = lv_label_create(ui->screen_6_cont_1);
    lv_label_set_text(ui->screen_6_label_4, "指纹：");
    lv_label_set_long_mode(ui->screen_6_label_4, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_6_label_4, 7, 196);
    lv_obj_set_size(ui->screen_6_label_4, 90, 23);

    //Write style for screen_6_label_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_6_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_6_label_4, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_label_4, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_label_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_6_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_6_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_6_label_4, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_6_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_6_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_6_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_6_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_6_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_label_5
    ui->screen_6_label_5 = lv_label_create(ui->screen_6_cont_1);
    lv_label_set_text(ui->screen_6_label_5, "身份：");
    lv_label_set_long_mode(ui->screen_6_label_5, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_6_label_5, 6, 154);
    lv_obj_set_size(ui->screen_6_label_5, 92, 25);

    //Write style for screen_6_label_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_6_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_6_label_5, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_label_5, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_label_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_6_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_6_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_6_label_5, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_6_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_6_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_6_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_6_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_6_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_btn_3
    ui->screen_6_btn_3 = lv_btn_create(ui->screen_6_cont_1);
    ui->screen_6_btn_3_label = lv_label_create(ui->screen_6_btn_3);
    lv_label_set_text(ui->screen_6_btn_3_label, "更改");
    lv_label_set_long_mode(ui->screen_6_btn_3_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_6_btn_3_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_6_btn_3, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_6_btn_3_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_6_btn_3, 177, 71);
    lv_obj_set_size(ui->screen_6_btn_3, 52, 25);
    /* "账号更改" 改用查找页(screen_5/screen_7)流程，本页不再暴露此按钮。
     * 在创建时就隐藏，避免从 screen_9/10 ESC 回 screen_6 重建后又冒出来。 */
    lv_obj_add_flag(ui->screen_6_btn_3, LV_OBJ_FLAG_HIDDEN);

    //Write style for screen_6_btn_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_6_btn_3, 165, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_6_btn_3, lv_color_hex(0x009bff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_6_btn_3, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_6_btn_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_btn_3, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_btn_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_6_btn_3, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_btn_3, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_btn_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_6_btn_3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_btn_4
    ui->screen_6_btn_4 = lv_btn_create(ui->screen_6_cont_1);
    ui->screen_6_btn_4_label = lv_label_create(ui->screen_6_btn_4);
    lv_label_set_text(ui->screen_6_btn_4_label, "更改");
    lv_label_set_long_mode(ui->screen_6_btn_4_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_6_btn_4_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_6_btn_4, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_6_btn_4_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_6_btn_4, 178, 111);
    lv_obj_set_size(ui->screen_6_btn_4, 51, 24);

    //Write style for screen_6_btn_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_6_btn_4, 165, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_6_btn_4, lv_color_hex(0x009bff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_6_btn_4, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_6_btn_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_btn_4, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_btn_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_6_btn_4, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_btn_4, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_btn_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_6_btn_4, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_btn_6
    ui->screen_6_btn_6 = lv_btn_create(ui->screen_6_cont_1);
    ui->screen_6_btn_6_label = lv_label_create(ui->screen_6_btn_6);
    lv_label_set_text(ui->screen_6_btn_6_label, "更改");
    lv_label_set_long_mode(ui->screen_6_btn_6_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_6_btn_6_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_6_btn_6, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_6_btn_6_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_6_btn_6, 177, 193);
    lv_obj_set_size(ui->screen_6_btn_6, 51, 24);

    //Write style for screen_6_btn_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_6_btn_6, 165, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_6_btn_6, lv_color_hex(0x009bff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_6_btn_6, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_6_btn_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_btn_6, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_btn_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_6_btn_6, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_btn_6, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_btn_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_6_btn_6, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_label_6
    ui->screen_6_label_6 = lv_label_create(ui->screen_6_cont_1);
    lv_label_set_text(ui->screen_6_label_6, "NFC:");
    lv_label_set_long_mode(ui->screen_6_label_6, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_6_label_6, 8, 235);
    lv_obj_set_size(ui->screen_6_label_6, 90, 23);

    //Write style for screen_6_label_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_6_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_6_label_6, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_label_6, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_label_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_6_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_6_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_6_label_6, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_6_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_6_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_6_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_6_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_6_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_btn_7
    ui->screen_6_btn_7 = lv_btn_create(ui->screen_6_cont_1);
    ui->screen_6_btn_7_label = lv_label_create(ui->screen_6_btn_7);
    lv_label_set_text(ui->screen_6_btn_7_label, "更改");
    lv_label_set_long_mode(ui->screen_6_btn_7_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_6_btn_7_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_6_btn_7, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_6_btn_7_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_6_btn_7, 177, 234);
    lv_obj_set_size(ui->screen_6_btn_7, 51, 24);

    //Write style for screen_6_btn_7, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_6_btn_7, 165, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_6_btn_7, lv_color_hex(0x009bff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_6_btn_7, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_6_btn_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_btn_7, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_btn_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_6_btn_7, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_btn_7, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_btn_7, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_6_btn_7, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_label_2
    ui->screen_6_label_2 = lv_label_create(ui->screen_6);
    lv_label_set_text(ui->screen_6_label_2, "账号：");
    lv_label_set_long_mode(ui->screen_6_label_2, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_6_label_2, 8, 71);
    lv_obj_set_size(ui->screen_6_label_2, 114, 24);

    //Write style for screen_6_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_6_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_6_label_2, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_label_2, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_6_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_6_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_6_label_2, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_6_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_6_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_6_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_6_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_6_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_6_ddlist_1
    ui->screen_6_ddlist_1 = lv_dropdown_create(ui->screen_6);
    lv_dropdown_set_options(ui->screen_6_ddlist_1, "用户\n管理员");
    lv_obj_set_pos(ui->screen_6_ddlist_1, 151, 154);
    lv_obj_set_size(ui->screen_6_ddlist_1, 80, 28);

    //Write style for screen_6_ddlist_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->screen_6_ddlist_1, lv_color_hex(0x0D3055), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_6_ddlist_1, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_6_ddlist_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_6_ddlist_1, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_6_ddlist_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_6_ddlist_1, lv_color_hex(0xe1e6ee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_6_ddlist_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_6_ddlist_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_6_ddlist_1, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_6_ddlist_1, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_6_ddlist_1, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_6_ddlist_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_6_ddlist_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_6_ddlist_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_6_ddlist_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_CHECKED for &style_screen_6_ddlist_1_extra_list_selected_checked
    static lv_style_t style_screen_6_ddlist_1_extra_list_selected_checked;
    ui_init_style(&style_screen_6_ddlist_1_extra_list_selected_checked);

    lv_style_set_border_width(&style_screen_6_ddlist_1_extra_list_selected_checked, 1);
    lv_style_set_border_opa(&style_screen_6_ddlist_1_extra_list_selected_checked, 255);
    lv_style_set_border_color(&style_screen_6_ddlist_1_extra_list_selected_checked, lv_color_hex(0xe1e6ee));
    lv_style_set_border_side(&style_screen_6_ddlist_1_extra_list_selected_checked, LV_BORDER_SIDE_FULL);
    lv_style_set_radius(&style_screen_6_ddlist_1_extra_list_selected_checked, 3);
    lv_style_set_bg_opa(&style_screen_6_ddlist_1_extra_list_selected_checked, 255);
    lv_style_set_bg_color(&style_screen_6_ddlist_1_extra_list_selected_checked, lv_color_hex(0x00a1b5));
    lv_style_set_bg_grad_dir(&style_screen_6_ddlist_1_extra_list_selected_checked, LV_GRAD_DIR_NONE);
    lv_style_set_text_color(&style_screen_6_ddlist_1_extra_list_selected_checked, lv_color_hex(0xffffff));
    lv_style_set_text_font(&style_screen_6_ddlist_1_extra_list_selected_checked, &lv_font_SourceHanSerifSC_Regular_20);
    lv_style_set_text_opa(&style_screen_6_ddlist_1_extra_list_selected_checked, 255);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_6_ddlist_1), &style_screen_6_ddlist_1_extra_list_selected_checked, LV_PART_SELECTED|LV_STATE_CHECKED);

    //Write style state: LV_STATE_DEFAULT for &style_screen_6_ddlist_1_extra_list_main_default
    static lv_style_t style_screen_6_ddlist_1_extra_list_main_default;
    ui_init_style(&style_screen_6_ddlist_1_extra_list_main_default);

    lv_style_set_max_height(&style_screen_6_ddlist_1_extra_list_main_default, 90);
    lv_style_set_text_color(&style_screen_6_ddlist_1_extra_list_main_default, lv_color_hex(0x0D3055));
    /* 下拉选项含中文，使用中文字体避免“用户/管理员”不显示 */
    lv_style_set_text_font(&style_screen_6_ddlist_1_extra_list_main_default, &lv_font_SourceHanSerifSC_Regular_20);
    lv_style_set_text_opa(&style_screen_6_ddlist_1_extra_list_main_default, 255);
    lv_style_set_border_width(&style_screen_6_ddlist_1_extra_list_main_default, 1);
    lv_style_set_border_opa(&style_screen_6_ddlist_1_extra_list_main_default, 255);
    lv_style_set_border_color(&style_screen_6_ddlist_1_extra_list_main_default, lv_color_hex(0xe1e6ee));
    lv_style_set_border_side(&style_screen_6_ddlist_1_extra_list_main_default, LV_BORDER_SIDE_FULL);
    lv_style_set_radius(&style_screen_6_ddlist_1_extra_list_main_default, 3);
    lv_style_set_bg_opa(&style_screen_6_ddlist_1_extra_list_main_default, 255);
    lv_style_set_bg_color(&style_screen_6_ddlist_1_extra_list_main_default, lv_color_hex(0xffffff));
    lv_style_set_bg_grad_dir(&style_screen_6_ddlist_1_extra_list_main_default, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_6_ddlist_1), &style_screen_6_ddlist_1_extra_list_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_6_ddlist_1_extra_list_scrollbar_default
    static lv_style_t style_screen_6_ddlist_1_extra_list_scrollbar_default;
    ui_init_style(&style_screen_6_ddlist_1_extra_list_scrollbar_default);

    lv_style_set_radius(&style_screen_6_ddlist_1_extra_list_scrollbar_default, 3);
    lv_style_set_bg_opa(&style_screen_6_ddlist_1_extra_list_scrollbar_default, 255);
    lv_style_set_bg_color(&style_screen_6_ddlist_1_extra_list_scrollbar_default, lv_color_hex(0x00ff00));
    lv_style_set_bg_grad_dir(&style_screen_6_ddlist_1_extra_list_scrollbar_default, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_6_ddlist_1), &style_screen_6_ddlist_1_extra_list_scrollbar_default, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);

    //The custom code of screen_6.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_6);

}
