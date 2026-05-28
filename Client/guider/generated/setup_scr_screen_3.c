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



void setup_scr_screen_3(lv_ui *ui)
{
    //Write codes screen_3
    ui->screen_3 = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_3, 240, 320);
    lv_obj_set_scrollbar_mode(ui->screen_3, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_3_cont_1
    ui->screen_3_cont_1 = lv_obj_create(ui->screen_3);
    lv_obj_set_pos(ui->screen_3_cont_1, 0, 0);
    lv_obj_set_size(ui->screen_3_cont_1, 240, 320);
    lv_obj_set_scrollbar_mode(ui->screen_3_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_3_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_3_cont_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_3_cont_1, lv_color_hex(0xfbb1bd), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_3_cont_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_3_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_3_cont_1, lv_color_hex(0xb1e8fb), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_3_cont_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_3_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_3_btn_1
    ui->screen_3_btn_1 = lv_btn_create(ui->screen_3_cont_1);
    ui->screen_3_btn_1_label = lv_label_create(ui->screen_3_btn_1);
    lv_label_set_text(ui->screen_3_btn_1_label, "ESC");
    lv_label_set_long_mode(ui->screen_3_btn_1_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_3_btn_1_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_3_btn_1, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_3_btn_1_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_3_btn_1, 82, 279);
    lv_obj_set_size(ui->screen_3_btn_1, 62, 30);

    //Write style for screen_3_btn_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_3_btn_1, 165, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_3_btn_1, lv_color_hex(0x009bff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_3_btn_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_3_btn_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_3_btn_1, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_3_btn_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_3_btn_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_3_btn_1, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_3_btn_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_3_btn_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_3_label_1
    ui->screen_3_label_1 = lv_label_create(ui->screen_3_cont_1);
    lv_label_set_text(ui->screen_3_label_1, "账号管理");
    lv_label_set_long_mode(ui->screen_3_label_1, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_3_label_1, 6, 13);
    lv_obj_set_size(ui->screen_3_label_1, 219, 30);

    //Write style for screen_3_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_3_label_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_3_label_1, &lv_font_SourceHanSerifSC_Regular_25, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_3_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_3_label_1, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_3_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_3_btn_4
    ui->screen_3_btn_4 = lv_btn_create(ui->screen_3_cont_1);
    ui->screen_3_btn_4_label = lv_label_create(ui->screen_3_btn_4);
    lv_label_set_text(ui->screen_3_btn_4_label, "2.删除用户");
    lv_label_set_long_mode(ui->screen_3_btn_4_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_3_btn_4_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_3_btn_4, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_3_btn_4_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_3_btn_4, 7, 102);
    lv_obj_set_size(ui->screen_3_btn_4, 115, 28);

    //Write style for screen_3_btn_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_3_btn_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_3_btn_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_3_btn_4, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_3_btn_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_3_btn_4, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_3_btn_4, &lv_font_SourceHanSerifSC_Regular_21, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_3_btn_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_3_btn_4, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_3_btn_5
    ui->screen_3_btn_5 = lv_btn_create(ui->screen_3_cont_1);
    ui->screen_3_btn_5_label = lv_label_create(ui->screen_3_btn_5);
    lv_label_set_text(ui->screen_3_btn_5_label, "3.查找(更改)用户");
    lv_label_set_long_mode(ui->screen_3_btn_5_label, LV_LABEL_LONG_CLIP);
    lv_obj_align(ui->screen_3_btn_5_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_3_btn_5, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_3_btn_5_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_3_btn_5, 7, 146);
    lv_obj_set_size(ui->screen_3_btn_5, 190, 30);

    //Write style for screen_3_btn_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_3_btn_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_3_btn_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_3_btn_5, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_3_btn_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_3_btn_5, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_3_btn_5, &lv_font_SourceHanSerifSC_Regular_21, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_3_btn_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_3_btn_5, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_3_btn_6
    ui->screen_3_btn_6 = lv_btn_create(ui->screen_3_cont_1);
    ui->screen_3_btn_6_label = lv_label_create(ui->screen_3_btn_6);
    lv_label_set_text(ui->screen_3_btn_6_label, "4.显示所有用户");
    lv_label_set_long_mode(ui->screen_3_btn_6_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_3_btn_6_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_3_btn_6, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_3_btn_6_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_3_btn_6, 7, 192);
    lv_obj_set_size(ui->screen_3_btn_6, 154, 27);

    //Write style for screen_3_btn_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_3_btn_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_3_btn_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_3_btn_6, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_3_btn_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_3_btn_6, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_3_btn_6, &lv_font_SourceHanSerifSC_Regular_21, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_3_btn_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_3_btn_6, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_3_btn_3
    ui->screen_3_btn_3 = lv_btn_create(ui->screen_3);
    ui->screen_3_btn_3_label = lv_label_create(ui->screen_3_btn_3);
    lv_label_set_text(ui->screen_3_btn_3_label, "1.增加用户");
    lv_label_set_long_mode(ui->screen_3_btn_3_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_3_btn_3_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_3_btn_3, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_3_btn_3_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_3_btn_3, 8, 60);
    lv_obj_set_size(ui->screen_3_btn_3, 115, 28);

    //Write style for screen_3_btn_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_3_btn_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_3_btn_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_3_btn_3, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_3_btn_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_3_btn_3, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_3_btn_3, &lv_font_SourceHanSerifSC_Regular_21, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_3_btn_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_3_btn_3, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of screen_3.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_3);

}
