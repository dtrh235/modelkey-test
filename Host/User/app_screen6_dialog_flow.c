#include "app_screen6_dialog_flow.h"

#include <stdbool.h>
#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "ui_common_utils.h"
#include "app_screen.h"
#include "app_screen6_types.h"
#include "app_screen6_dialog_base.h"
#include "app_screen6_info.h"
#include "app_screen6_commit.h"

LV_FONT_DECLARE(lv_font_montserratMedium_16);
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20);

extern lv_ui guider_ui;
extern volatile app_scr_t g_app_scr;
extern screen6_dlg_t g_screen6_dlg;
extern uint8_t g_screen6_dlg_saved_focus;
extern uint8_t g_screen6_focus;
extern uint8_t g_screen5_found_is_admin;
extern lv_obj_t *g_screen6_dlg_root;
extern lv_obj_t *g_screen6_dlg_btn_close;
extern lv_obj_t *g_screen6_dlg_btn_ok;
extern lv_obj_t *g_screen6_dlg_btn_esc;
extern lv_obj_t *g_screen6_dlg_ta;
extern lv_obj_t *g_screen6_dlg_cursor;
extern uint32_t g_screen6_dlg_cursor_ms;
extern uint8_t g_screen6_dlg_cursor_vis;

static void screen6_dlg_close_x_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    screen6_dlg_handle_key(KEY_ESC);
}

static void screen6_dlg_ok_btn_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    screen6_dlg_handle_key(KEY_OK);
}

static void screen6_dlg_cancel_btn_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    screen6_dlg_handle_key(KEY_ESC);
}

static void screen6_dlg_apply_overlay(lv_obj_t *root)
{
    lv_obj_set_size(root, 240, 320);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(root, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void screen6_dlg_apply_popup_panel(lv_obj_t *panel, lv_coord_t w, lv_coord_t h)
{
    lv_obj_set_size(panel, w, h);
    lv_obj_center(panel);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void screen6_dlg_add_close_x(lv_obj_t *panel)
{
    lv_obj_t *btn_x;
    lv_obj_t *lbl_x;

    btn_x = lv_btn_create(panel);
    g_screen6_dlg_btn_close = btn_x;
    lv_obj_set_size(btn_x, 24, 24);
    lv_obj_align(btn_x, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_set_style_radius(btn_x, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn_x, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_x, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_x, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_x, screen6_dlg_close_x_event_cb, LV_EVENT_CLICKED, NULL);

    lbl_x = lv_label_create(btn_x);
    lv_label_set_text(lbl_x, "X");
    lv_obj_set_style_text_font(lbl_x, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_x, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(lbl_x);
}

static void screen6_dlg_add_bottom_ok_cancel(lv_obj_t *panel)
{
    lv_obj_t *btn_ok;
    lv_obj_t *btn_esc;
    lv_obj_t *lbl_ok;
    lv_obj_t *lbl_esc;

    btn_ok = lv_btn_create(panel);
    btn_esc = lv_btn_create(panel);
    g_screen6_dlg_btn_ok = btn_ok;
    g_screen6_dlg_btn_esc = btn_esc;
    lv_obj_set_size(btn_ok, 62, 30);
    lv_obj_set_size(btn_esc, 62, 30);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    lv_obj_align(btn_esc, LV_ALIGN_BOTTOM_RIGHT, -12, -12);

    lv_obj_set_style_radius(btn_ok, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_esc, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_ok, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_esc, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn_ok, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn_esc, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_esc, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(btn_ok, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(btn_esc, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_ok, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_esc, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_ok, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_esc, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(btn_ok, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(btn_esc, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(btn_ok, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(btn_esc, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(btn_ok, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(btn_esc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn_ok, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn_esc, 0, LV_STATE_DEFAULT);

    lbl_ok = lv_label_create(btn_ok);
    lbl_esc = lv_label_create(btn_esc);
    lv_label_set_text(lbl_ok, "OK");
    lv_label_set_text(lbl_esc, "ESC");
    lv_label_set_long_mode(lbl_ok, LV_LABEL_LONG_WRAP);
    lv_label_set_long_mode(lbl_esc, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_ok, LV_PCT(100));
    lv_obj_set_width(lbl_esc, LV_PCT(100));
    lv_obj_set_style_text_font(lbl_ok, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_esc, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_ok, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_esc, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl_ok, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl_esc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_ok, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(lbl_esc, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(btn_ok, screen6_dlg_ok_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_esc, screen6_dlg_cancel_btn_event_cb, LV_EVENT_CLICKED, NULL);
}

static void screen6_show_result_dialog(bool success)
{
    lv_obj_t *scr = guider_ui.screen_6;
    lv_obj_t *panel;
    lv_obj_t *msg;

    if(!lv_obj_is_valid(scr)) return;

    g_screen6_dlg_root = lv_obj_create(scr);
    screen6_dlg_apply_overlay(g_screen6_dlg_root);

    panel = lv_obj_create(g_screen6_dlg_root);
    screen6_dlg_apply_popup_panel(panel, 180, 100);

    screen6_dlg_add_close_x(panel);

    msg = lv_label_create(panel);
    lv_label_set_text(msg, success ? "更改成功" : "更改失败");
    lv_label_set_long_mode(msg, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(msg, 150);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(msg, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(msg, lv_color_hex(0x0D3055), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(msg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(msg);

    g_screen6_dlg_ta = NULL;
    g_screen6_dlg_cursor = NULL;

    lv_obj_move_foreground(g_screen6_dlg_root);
}

void screen6_open_edit_account_dialog(void)
{
    lv_obj_t *scr = guider_ui.screen_6;
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *ta;

    if(!lv_obj_is_valid(scr)) return;

    screen6_close_dialog();
    g_screen6_dlg_saved_focus = g_screen6_focus;
    g_screen6_dlg = SCREEN6_DLG_EDIT_ACC;

    g_screen6_dlg_root = lv_obj_create(scr);
    screen6_dlg_apply_overlay(g_screen6_dlg_root);

    panel = lv_obj_create(g_screen6_dlg_root);
    screen6_dlg_apply_popup_panel(panel, 224, 220);

    screen6_dlg_add_close_x(panel);

    title = lv_label_create(panel);
    lv_label_set_text(title, "更改后的账号：");
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(title, 200);
    lv_obj_set_style_text_font(title, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    ta = lv_textarea_create(panel);
    lv_obj_set_size(ta, 200, 35);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 78);
    screen6_dlg_style_ta_like_screen5(ta, 12u);
    g_screen6_dlg_ta = ta;
    screen6_dlg_setup_cursor_on_ta(ta);

    screen6_dlg_add_bottom_ok_cancel(panel);

    lv_obj_move_foreground(g_screen6_dlg_root);
}

void screen6_open_edit_password_dialog(void)
{
    lv_obj_t *scr = guider_ui.screen_6;
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *ta;

    if(!lv_obj_is_valid(scr)) return;

    screen6_close_dialog();
    g_screen6_dlg_saved_focus = g_screen6_focus;
    g_screen6_dlg = SCREEN6_DLG_EDIT_PWD;

    g_screen6_dlg_root = lv_obj_create(scr);
    screen6_dlg_apply_overlay(g_screen6_dlg_root);

    panel = lv_obj_create(g_screen6_dlg_root);
    screen6_dlg_apply_popup_panel(panel, 224, 220);

    screen6_dlg_add_close_x(panel);

    title = lv_label_create(panel);
    lv_label_set_text(title, "更改后的密码：");
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(title, 200);
    lv_obj_set_style_text_font(title, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    ta = lv_textarea_create(panel);
    lv_obj_set_size(ta, 200, 35);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 78);
    screen6_dlg_style_ta_like_screen5(ta, 10u);
    g_screen6_dlg_ta = ta;
    screen6_dlg_setup_cursor_on_ta(ta);

    screen6_dlg_add_bottom_ok_cancel(panel);

    lv_obj_move_foreground(g_screen6_dlg_root);
}

static void screen6_dlg_handle_ta_key(KeyValue_t key)
{
    lv_obj_t *ta = g_screen6_dlg_ta;
    uint16_t max_len;
    int digit;
    uint16_t cur_len;

    if(!lv_obj_is_valid(ta)) return;

    max_len = (g_screen6_dlg == SCREEN6_DLG_EDIT_ACC) ? 12u : 10u;
    digit = ui_key_to_digit(key);

    if(key == KEY_LEFT) {
        lv_textarea_del_char(ta);
        screen6_dlg_update_cursor_pos();
        return;
    }

    if(digit < 0) return;

    cur_len = (uint16_t)strlen(lv_textarea_get_text(ta));
    if(cur_len >= max_len) return;

    lv_textarea_add_char(ta, (uint32_t)('0' + digit));
    screen6_dlg_update_cursor_pos();
}

void screen6_dlg_handle_key(KeyValue_t key)
{
    bool ok;

    if(g_screen6_dlg == SCREEN6_DLG_RESULT_OK || g_screen6_dlg == SCREEN6_DLG_RESULT_FAIL) {
        if(key == KEY_OK || key == KEY_ESC) {
            screen6_close_dialog();
            g_screen6_dlg = SCREEN6_DLG_NONE;
            screen6_update_labels_from_found();
            if(lv_obj_is_valid(guider_ui.screen_6_ddlist_1)) {
                lv_dropdown_set_selected(guider_ui.screen_6_ddlist_1, g_screen5_found_is_admin ? 1u : 0u);
            }
            screen6_set_focus(g_screen6_dlg_saved_focus);
        }
        return;
    }

    if(g_screen6_dlg == SCREEN6_DLG_EDIT_ACC || g_screen6_dlg == SCREEN6_DLG_EDIT_PWD) {
        if(key == KEY_ESC) {
            screen6_close_dialog();
            g_screen6_dlg = SCREEN6_DLG_NONE;
            screen6_set_focus(g_screen6_dlg_saved_focus);
            return;
        }
        if(key == KEY_OK) {
            const char *txt = lv_textarea_get_text(g_screen6_dlg_ta);
            if(g_screen6_dlg == SCREEN6_DLG_EDIT_ACC) {
                (void)txt;
                ok = false;
            } else {
                ok = screen6_try_commit_password_change(txt);
            }
            screen6_close_dialog();
            g_screen6_dlg = ok ? SCREEN6_DLG_RESULT_OK : SCREEN6_DLG_RESULT_FAIL;
            screen6_show_result_dialog(ok);
            return;
        }
        screen6_dlg_handle_ta_key(key);
    }
}

void screen6_dlg_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_6) return;
    if(g_screen6_dlg != SCREEN6_DLG_EDIT_ACC && g_screen6_dlg != SCREEN6_DLG_EDIT_PWD) return;
    ui_cursor_blink_step(g_screen6_dlg_cursor, &g_screen6_dlg_cursor_vis, &g_screen6_dlg_cursor_ms, 500u);
}
