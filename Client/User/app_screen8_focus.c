#include "app_screen8_focus.h"

#include "lvgl.h"
#include "gui_guider.h"
#include "app_screen.h"
#include "ui_screen8_utils.h"
#include "ui_auth_input.h"

extern lv_ui guider_ui;
extern app_scr_t g_app_scr;
extern uint8_t g_screen8_focus;
extern lv_obj_t *g_screen8_cursor;
extern uint32_t g_screen8_cursor_last_ms;
extern uint8_t g_screen8_cursor_visible;

static lv_obj_t *screen8_focus_get_ta(void)
{
    return ui_screen8_focus_get_ta(g_screen8_focus, guider_ui.screen_8_ta_1, guider_ui.screen_8_ta_2);
}

void screen8_hide_status_labels(void)
{
    ui_screen8_hide_status_labels(guider_ui.screen_8_label_5, guider_ui.screen_8_label_6);
}

void screen8_clear_error_if_any(void)
{
    ui_screen8_clear_error_label(guider_ui.screen_8_label_5);
}

static void screen8_style_ta_idle(lv_obj_t *ta)
{
    ui_screen8_style_ta(ta, 0u);
}

static void screen8_style_ta_selected(lv_obj_t *ta)
{
    ui_screen8_style_ta(ta, 1u);
}

static void screen8_style_btn_idle(lv_obj_t *btn, lv_obj_t *lbl)
{
    ui_screen8_style_btn(btn, lbl, 0u);
}

static void screen8_style_btn_selected(lv_obj_t *btn, lv_obj_t *lbl)
{
    ui_screen8_style_btn(btn, lbl, 1u);
}

static void screen8_update_cursor_pos(void)
{
    lv_obj_t *ta = screen8_focus_get_ta();
    ui_auth_update_cursor_pos(g_screen8_cursor, ta);
}

void screen8_set_focus(uint8_t idx)
{
    lv_obj_t *ta1 = guider_ui.screen_8_ta_1;
    lv_obj_t *ta2 = guider_ui.screen_8_ta_2;
    lv_obj_t *cb = guider_ui.screen_8_cb_1;
    lv_obj_t *b_fp_1 = guider_ui.screen_8_btn_3;
    lv_obj_t *b_fp_1_l = guider_ui.screen_8_btn_3_label;
    lv_obj_t *b_fp_2 = guider_ui.screen_8_btn_4;
    lv_obj_t *b_fp_2_l = guider_ui.screen_8_btn_4_label;
    lv_obj_t *b_ok = guider_ui.screen_8_btn_1;
    lv_obj_t *b_ok_l = guider_ui.screen_8_btn_1_label;

    if(idx >= APP_SCREEN8_N_FOCUS) idx = 0u;
    g_screen8_focus = idx;
    screen8_clear_error_if_any();

    screen8_style_ta_idle(ta1);
    screen8_style_ta_idle(ta2);
    screen8_style_btn_idle(b_fp_1, b_fp_1_l);
    screen8_style_btn_idle(b_fp_2, b_fp_2_l);
    screen8_style_btn_idle(b_ok, b_ok_l);
    if(lv_obj_is_valid(cb)) {
        lv_obj_set_style_outline_width(cb, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    switch(g_screen8_focus) {
        case 0:
            screen8_style_ta_selected(ta1);
            ui_cursor_attach_bar(&g_screen8_cursor, ta1);
            screen8_update_cursor_pos();
            ui_cursor_activate(g_screen8_cursor, &g_screen8_cursor_visible, &g_screen8_cursor_last_ms);
            break;
        case 1:
            screen8_style_ta_selected(ta2);
            ui_cursor_attach_bar(&g_screen8_cursor, ta2);
            screen8_update_cursor_pos();
            ui_cursor_activate(g_screen8_cursor, &g_screen8_cursor_visible, &g_screen8_cursor_last_ms);
            break;
        case 2:
            if(lv_obj_is_valid(cb)) {
                lv_obj_set_style_outline_width(cb, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_outline_color(cb, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_outline_opa(cb, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_outline_pad(cb, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            if(g_screen8_cursor != NULL && lv_obj_is_valid(g_screen8_cursor)) {
                lv_obj_add_flag(g_screen8_cursor, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case 3:
            screen8_style_btn_selected(b_fp_1, b_fp_1_l);
            if(g_screen8_cursor != NULL && lv_obj_is_valid(g_screen8_cursor)) {
                lv_obj_add_flag(g_screen8_cursor, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case 4:
            screen8_style_btn_selected(b_fp_2, b_fp_2_l);
            if(g_screen8_cursor != NULL && lv_obj_is_valid(g_screen8_cursor)) {
                lv_obj_add_flag(g_screen8_cursor, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case 5:
            screen8_style_btn_selected(b_ok, b_ok_l);
            if(g_screen8_cursor != NULL && lv_obj_is_valid(g_screen8_cursor)) {
                lv_obj_add_flag(g_screen8_cursor, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        default:
            break;
    }
}

void screen8_handle_ta_key(KeyValue_t key)
{
    lv_obj_t *ta = screen8_focus_get_ta();
    uint16_t max_len = (g_screen8_focus == 0u) ? 12u : 10u;
    ui_auth_handle_input_key(ta, key, max_len, NULL, screen8_update_cursor_pos);
}

void screen8_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_8 || g_screen8_focus > 1u) return;
    ui_cursor_blink_step(g_screen8_cursor, &g_screen8_cursor_visible, &g_screen8_cursor_last_ms, 500u);
}
