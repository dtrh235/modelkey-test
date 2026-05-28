#include "app_screen_auth.h"

#include "lvgl.h"
#include "gui_guider.h"
#include "app_screen.h"
#include "ui_common_utils.h"
#include "ui_auth_input.h"

extern lv_ui guider_ui;
extern app_scr_t g_app_scr;
extern uint8_t g_screen1_field_index;
extern lv_obj_t *g_screen1_cursor;
extern uint32_t g_cursor_last_toggle_ms;
extern uint8_t g_cursor_visible;
extern uint8_t g_screen2_field_index;
extern lv_obj_t *g_screen2_cursor;
extern uint32_t g_screen2_cursor_last_toggle_ms;
extern uint8_t g_screen2_cursor_visible;

static lv_obj_t *screen1_get_field_by_index(uint8_t idx)
{
    return ui_auth_get_field_by_index(guider_ui.screen_1_ta_1, guider_ui.screen_1_ta_2, idx);
}

static uint16_t screen1_get_field_max_len(uint8_t idx)
{
    return ui_auth_get_field_max_len(idx);
}

static lv_obj_t *screen2_get_field_by_index(uint8_t idx)
{
    return ui_auth_get_field_by_index(guider_ui.screen_2_ta_2, guider_ui.screen_2_ta_1, idx);
}

static uint16_t screen2_get_field_max_len(uint8_t idx)
{
    return ui_auth_get_field_max_len(idx);
}

static void screen1_update_cursor_pos(void)
{
    lv_obj_t *ta = screen1_get_field_by_index(g_screen1_field_index);
    ui_auth_update_cursor_pos(g_screen1_cursor, ta);
}

static void screen2_update_cursor_pos(void)
{
    lv_obj_t *ta = screen2_get_field_by_index(g_screen2_field_index);
    ui_auth_update_cursor_pos(g_screen2_cursor, ta);
}

void screen2_hide_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_2_label_3, 1u);
}

void screen2_show_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_2_label_3, 0u);
}

void screen1_hide_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_1_label_3, 1u);
}

void screen1_show_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_1_label_3, 0u);
}

void screen1_handle_input_key(KeyValue_t key)
{
    lv_obj_t *ta = screen1_get_field_by_index(g_screen1_field_index);
    uint16_t max_len = screen1_get_field_max_len(g_screen1_field_index);
    ui_auth_handle_input_key(ta, key, max_len, screen1_hide_error_label, screen1_update_cursor_pos);
}

void screen1_set_field_selected(uint8_t idx)
{
    lv_obj_t *ta_acc = guider_ui.screen_1_ta_1;
    lv_obj_t *ta_pwd = guider_ui.screen_1_ta_2;
    lv_obj_t *selected;

    if(!lv_obj_is_valid(ta_acc) || !lv_obj_is_valid(ta_pwd)) return;

    ui_textarea_apply_input_style(ta_acc, 0u);
    ui_textarea_apply_input_style(ta_pwd, 0u);

    g_screen1_field_index = (idx == 0u) ? 0u : 1u;
    selected = screen1_get_field_by_index(g_screen1_field_index);
    ui_textarea_apply_input_style(selected, 1u);
    ui_cursor_attach_bar(&g_screen1_cursor, selected);
    screen1_update_cursor_pos();
    ui_cursor_activate(g_screen1_cursor, &g_cursor_visible, &g_cursor_last_toggle_ms);
}

void screen1_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_1) return;
    ui_cursor_blink_step(g_screen1_cursor, &g_cursor_visible, &g_cursor_last_toggle_ms, 500u);
}

void screen2_handle_input_key(KeyValue_t key)
{
    lv_obj_t *ta = screen2_get_field_by_index(g_screen2_field_index);
    uint16_t max_len = screen2_get_field_max_len(g_screen2_field_index);
    ui_auth_handle_input_key(ta, key, max_len, screen2_hide_error_label, screen2_update_cursor_pos);
}

void screen2_set_field_selected(uint8_t idx)
{
    lv_obj_t *ta_acc = guider_ui.screen_2_ta_2;
    lv_obj_t *ta_pwd = guider_ui.screen_2_ta_1;
    lv_obj_t *selected;

    if(!lv_obj_is_valid(ta_acc) || !lv_obj_is_valid(ta_pwd)) return;

    ui_textarea_apply_input_style(ta_acc, 0u);
    ui_textarea_apply_input_style(ta_pwd, 0u);

    g_screen2_field_index = (idx == 0u) ? 0u : 1u;
    selected = screen2_get_field_by_index(g_screen2_field_index);
    ui_textarea_apply_input_style(selected, 1u);
    ui_cursor_attach_bar(&g_screen2_cursor, selected);
    screen2_update_cursor_pos();
    ui_cursor_activate(g_screen2_cursor, &g_screen2_cursor_visible, &g_screen2_cursor_last_toggle_ms);
}

void screen2_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_2) return;
    ui_cursor_blink_step(g_screen2_cursor, &g_screen2_cursor_visible, &g_screen2_cursor_last_toggle_ms, 500u);
}
