/*
* screen_11: WiFi??????/????? screen_3 ????????? + ??? ...??
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"
#include "app_config.h"

void setup_scr_screen_11(lv_ui *ui)
{
    ui->screen_11 = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_11, 240, 320);
    lv_obj_set_scrollbar_mode(ui->screen_11, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(ui->screen_11, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_11_cont_1 = lv_obj_create(ui->screen_11);
    lv_obj_set_pos(ui->screen_11_cont_1, 0, 0);
    lv_obj_set_size(ui->screen_11_cont_1, 240, 320);
    lv_obj_set_scrollbar_mode(ui->screen_11_cont_1, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_style_border_width(ui->screen_11_cont_1, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_11_cont_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_11_cont_1, lv_color_hex(0xfbb1bd), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_11_cont_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_11_cont_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_11_cont_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_11_cont_1, lv_color_hex(0xb1e8fb), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_11_cont_1, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_11_cont_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_11_cont_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_11_cont_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_11_cont_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_11_cont_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* ?????? screen_3?????????????? */
    ui->screen_11_label_title = lv_label_create(ui->screen_11_cont_1);
    lv_label_set_text(ui->screen_11_label_title, "\xe8\xae\xbe\xe7\xbd\xaeWIFI");
    lv_label_set_long_mode(ui->screen_11_label_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_11_label_title, 6, 13);
    lv_obj_set_size(ui->screen_11_label_title, 219, 30);
    lv_obj_set_style_text_font(ui->screen_11_label_title, &lv_font_SourceHanSerifSC_Regular_25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_11_label_title, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_11_label_title, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_11_row_connected = lv_obj_create(ui->screen_11_cont_1);
    lv_obj_set_pos(ui->screen_11_row_connected, 0, 44);
    lv_obj_set_size(ui->screen_11_row_connected, 240, 34);
    lv_obj_clear_flag(ui->screen_11_row_connected, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui->screen_11_row_connected, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_11_row_connected, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_11_row_connected, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->screen_11_row_connected, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    {
        lv_obj_t *conn_icon = lv_label_create(ui->screen_11_row_connected);
        lv_label_set_text(conn_icon, LV_SYMBOL_WIFI);
        lv_obj_set_pos(conn_icon, 4, 6);
        lv_obj_set_style_text_font(conn_icon, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(conn_icon, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    ui->screen_11_lbl_conn_ssid = lv_label_create(ui->screen_11_row_connected);
    lv_label_set_text(ui->screen_11_lbl_conn_ssid, "");
    lv_label_set_long_mode(ui->screen_11_lbl_conn_ssid, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ui->screen_11_lbl_conn_ssid, 130);
    lv_obj_set_pos(ui->screen_11_lbl_conn_ssid, 28, 6);
    lv_obj_set_style_text_font(ui->screen_11_lbl_conn_ssid, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_11_lbl_conn_sta = lv_label_create(ui->screen_11_row_connected);
    lv_label_set_text(ui->screen_11_lbl_conn_sta, "\xe5\xb7\xb2\xe8\xbf\x9e\xe6\x8e\xa5");
    lv_obj_align(ui->screen_11_lbl_conn_sta, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_text_font(ui->screen_11_lbl_conn_sta, &lv_font_cn_wifi_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_11_lbl_conn_sta, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ui->screen_11_row_connected, LV_OBJ_FLAG_HIDDEN);

    ui->screen_11_label_scan = lv_label_create(ui->screen_11_cont_1);
    lv_label_set_text(ui->screen_11_label_scan, "\xe6\x89\xab\xe6\x8f\x8f\xe4\xb8\xad");
    lv_obj_set_pos(ui->screen_11_label_scan, 6, 78);
    lv_obj_set_size(ui->screen_11_label_scan, 100, 30);
    lv_obj_set_style_text_font(ui->screen_11_label_scan, &lv_font_SourceHanSerifSC_Regular_25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_11_label_scan, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_11_label_scan, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* ??...??????Montserrat???????????? */
    ui->screen_11_label_scan_dots = lv_label_create(ui->screen_11_cont_1);
    lv_label_set_text(ui->screen_11_label_scan_dots, "");
    lv_obj_set_pos(ui->screen_11_label_scan_dots, 90, 78);
    lv_obj_set_size(ui->screen_11_label_scan_dots, 36, 30);
    lv_obj_set_style_text_font(ui->screen_11_label_scan_dots, &lv_font_SourceHanSerifSC_Regular_25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_11_label_scan_dots, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_11_label_scan_dots, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui->screen_11_list_panel = lv_obj_create(ui->screen_11_cont_1);
    lv_obj_set_pos(ui->screen_11_list_panel, 0, 108);
    lv_obj_set_size(ui->screen_11_list_panel, 240, 172);
    lv_obj_set_scrollbar_mode(ui->screen_11_list_panel, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(ui->screen_11_list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_11_list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->screen_11_list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(ui->screen_11_list_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui->screen_11_list_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    ui->screen_11_btn_ok = lv_btn_create(ui->screen_11_cont_1);
    ui->screen_11_btn_ok_label = lv_label_create(ui->screen_11_btn_ok);
    lv_label_set_text(ui->screen_11_btn_ok_label, "OK");
    lv_label_set_long_mode(ui->screen_11_btn_ok_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_11_btn_ok_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_11_btn_ok, 0, LV_STATE_DEFAULT);
    lv_obj_set_pos(ui->screen_11_btn_ok, 1, 286);
    lv_obj_set_size(ui->screen_11_btn_ok, 62, 30);
    lv_obj_set_style_bg_opa(ui->screen_11_btn_ok, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_11_btn_ok, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_11_btn_ok, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_11_btn_ok, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_11_btn_ok, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_11_btn_ok, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_11_btn_ok_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_11_btn_ok_label, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_11_btn_ok_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_11_btn_ok_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_11_btn_ok_label, LV_PCT(100));

    ui->screen_11_btn_esc = lv_btn_create(ui->screen_11_cont_1);
    ui->screen_11_btn_esc_label = lv_label_create(ui->screen_11_btn_esc);
    lv_label_set_text(ui->screen_11_btn_esc_label, "ESC");
    lv_label_set_long_mode(ui->screen_11_btn_esc_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_11_btn_esc_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_11_btn_esc, 0, LV_STATE_DEFAULT);
    lv_obj_set_pos(ui->screen_11_btn_esc, 174, 285);
    lv_obj_set_size(ui->screen_11_btn_esc, 62, 30);
    lv_obj_set_style_bg_opa(ui->screen_11_btn_esc, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_11_btn_esc, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_11_btn_esc, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_11_btn_esc, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_11_btn_esc, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_11_btn_esc, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_11_btn_esc_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_11_btn_esc_label, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_11_btn_esc_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_11_btn_esc_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_11_btn_esc_label, LV_PCT(100));

    lv_obj_update_layout(ui->screen_11);
}
