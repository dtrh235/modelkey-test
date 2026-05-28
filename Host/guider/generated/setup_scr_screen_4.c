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



void setup_scr_screen_4(lv_ui *ui)
{
    //Write codes screen_4
    ui->screen_4 = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_4, 240, 320);
    lv_obj_set_scrollbar_mode(ui->screen_4, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_4_cont_1
    ui->screen_4_cont_1 = lv_obj_create(ui->screen_4);
    lv_obj_set_pos(ui->screen_4_cont_1, 0, 0);
    lv_obj_set_size(ui->screen_4_cont_1, 240, 320);
    lv_obj_set_scrollbar_mode(ui->screen_4_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_4_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_4_cont_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_4_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_4_cont_1, lv_color_hex(0xfbb1bd), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_4_cont_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_4_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_4_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_4_cont_1, lv_color_hex(0xb1e8fb), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_4_cont_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_4_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_4_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_4_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_4_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_4_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_4_btn_4
    ui->screen_4_btn_4 = lv_btn_create(ui->screen_4_cont_1);
    ui->screen_4_btn_4_label = lv_label_create(ui->screen_4_btn_4);
    lv_label_set_text(ui->screen_4_btn_4_label, "ESC");
    lv_label_set_long_mode(ui->screen_4_btn_4_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_4_btn_4_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_4_btn_4, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_4_btn_4_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_4_btn_4, 82, 279);
    lv_obj_set_size(ui->screen_4_btn_4, 62, 30);

    //Write style for screen_4_btn_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_4_btn_4, 165, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_4_btn_4, lv_color_hex(0x009bff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_4_btn_4, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_4_btn_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_4_btn_4, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_4_btn_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_4_btn_4, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_4_btn_4, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_4_btn_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_4_btn_4, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_4_label_1
    ui->screen_4_label_1 = lv_label_create(ui->screen_4_cont_1);
    lv_label_set_text(ui->screen_4_label_1, "显示所有用户");
    lv_label_set_long_mode(ui->screen_4_label_1, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_4_label_1, 5, 12);
    lv_obj_set_size(ui->screen_4_label_1, 219, 30);

    //Write style for screen_4_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_4_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_4_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_4_label_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_4_label_1, &lv_font_SourceHanSerifSC_Regular_25, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_4_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_4_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_4_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_4_label_1, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_4_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_4_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_4_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_4_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_4_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_4_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_4_table_1
    ui->screen_4_table_1 = lv_table_create(ui->screen_4);
    lv_table_set_col_cnt(ui->screen_4_table_1,2);
    lv_table_set_row_cnt(ui->screen_4_table_1,33);
    /* 不在此预填演示数据，避免切入本屏时先闪一帧占位表；数据由 main.c 的 screen4_refresh_table() 写入 */
    for(uint16_t r = 0u; r < 33u; r++) {
        lv_table_set_cell_value(ui->screen_4_table_1, r, 0u, "");
        lv_table_set_cell_value(ui->screen_4_table_1, r, 1u, "");
    }
    lv_obj_set_pos(ui->screen_4_table_1, 0, 48);
    lv_obj_set_size(ui->screen_4_table_1, 240, 220);
    lv_obj_set_scrollbar_mode(ui->screen_4_table_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_4_table_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_pad_top(ui->screen_4_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_4_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_4_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_4_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_4_table_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_4_table_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_4_table_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_4_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_4_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_4_table_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_4_table_1, Part: LV_PART_ITEMS, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->screen_4_table_1, lv_color_hex(0x393c41), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_4_table_1, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_4_table_1, 255, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_4_table_1, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_4_table_1, 0, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_4_table_1, 3, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_4_table_1, 255, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_4_table_1, lv_color_hex(0xd5dee6), LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_4_table_1, LV_BORDER_SIDE_FULL, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_4_table_1, 6, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_4_table_1, 6, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_4_table_1, 6, LV_PART_ITEMS|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_4_table_1, 6, LV_PART_ITEMS|LV_STATE_DEFAULT);

    //The custom code of screen_4.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_4);

    //Init events for screen.
    events_init_screen_4(ui);
}
