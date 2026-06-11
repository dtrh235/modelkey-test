#include "app_screen_auth.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "app_screen.h"
#include "ui_common_utils.h"
#include "ui_auth_input.h"
#include "app_user_ops.h"
#include "app_unlock_event.h"
#include "app_pair_bind.h"
#include "app_temp_password.h"
#include "app_cloud_bind_cmd.h"
#include "cloud_ota_service.h"
#include "stm32f4xx_hal.h"

#define SCREEN1_LOCKOUT_FAIL_LIMIT   5u
#define SCREEN1_LOCKOUT_DURATION_MS  60000u
#define SCREEN1_LOCKOUT_DEVICE_ID    1u

static uint8_t s_screen1_fail_streak;
static uint32_t s_screen1_lock_until_ms;
static char s_screen1_last_fail_acc[13];
static uint8_t s_screen1_alert_pending;
static uint32_t s_screen1_lockout_ui_ms;

extern lv_ui guider_ui;
extern volatile app_scr_t g_app_scr;
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
    if(!lv_obj_is_valid(guider_ui.screen_1_label_3)) {
        return;
    }
    lv_label_set_text(guider_ui.screen_1_label_3, "\xE8\xBE\x93\xE5\x85\xA5\xE9\x94\x99\xE8\xAF\xAF");
    ui_label_set_hidden(guider_ui.screen_1_label_3, 0u);
}

void screen1_show_lockout_label(uint32_t remain_sec)
{
    char txt[32];

    if(!lv_obj_is_valid(guider_ui.screen_1_label_3)) {
        return;
    }
    if(remain_sec == 0u) {
        remain_sec = 1u;
    }
    (void)snprintf(txt, sizeof(txt), "%lu:%02lu",
                   (unsigned long)(remain_sec / 60u),
                   (unsigned long)(remain_sec % 60u));
    lv_label_set_text(guider_ui.screen_1_label_3, txt);
    ui_label_set_hidden(guider_ui.screen_1_label_3, 0u);
}

uint8_t screen1_is_lockout_active(void)
{
    uint32_t now = HAL_GetTick();

    if(s_screen1_lock_until_ms != 0u && now >= s_screen1_lock_until_ms) {
        s_screen1_lock_until_ms = 0u;
        s_screen1_fail_streak = 0u;
        screen1_hide_error_label();
    }
    return (s_screen1_lock_until_ms != 0u && now < s_screen1_lock_until_ms) ? 1u : 0u;
}

static void screen1_lockout_fire_alert(void)
{
    if(s_screen1_alert_pending == 0u) {
        return;
    }
    s_screen1_alert_pending = 0u;
    app_pair_publish_unlock_alert(SCREEN1_LOCKOUT_DEVICE_ID, s_screen1_last_fail_acc);
}

void screen1_lockout_poll(void)
{
    uint32_t now;
    uint32_t remain;

    if(g_app_scr != APP_SCR_1) {
        return;
    }
    if(screen1_is_lockout_active() == 0u) {
        screen1_lockout_fire_alert();
        return;
    }
    now = HAL_GetTick();
    remain = (s_screen1_lock_until_ms - now + 999u) / 1000u;
    if((now - s_screen1_lockout_ui_ms) >= 1000u) {
        s_screen1_lockout_ui_ms = now;
        screen1_show_lockout_label(remain);
    }
}

uint8_t screen1_try_password_unlock(void)
{
    const char *acc;
    const char *pwd;
    uint32_t now;

    if(screen1_is_lockout_active() != 0u) {
        return 2u;
    }

    acc = lv_textarea_get_text(guider_ui.screen_1_ta_1);
    pwd = lv_textarea_get_text(guider_ui.screen_1_ta_2);
    if(app_temp_password_try_unlock(acc, pwd) != 0u) {
        s_screen1_fail_streak = 0u;
        s_screen1_lock_until_ms = 0u;
        s_screen1_alert_pending = 0u;
        screen1_hide_error_label();
        cloud_ota_service_set_unlock_guest(acc);
        app_unlock_event_handle_success(APP_UNLOCK_POPUP_SCREEN1, "temporary account", "temporary-password");
        app_cloud_queue_temp_password_used(acc);
        return 1u;
    }
    if(unlock_credentials_match_with_delete(acc, pwd)) {
        s_screen1_fail_streak = 0u;
        s_screen1_lock_until_ms = 0u;
        s_screen1_alert_pending = 0u;
        screen1_hide_error_label();
        app_unlock_event_handle_success(APP_UNLOCK_POPUP_SCREEN1, acc, "password");
        return 1u;
    }

    now = HAL_GetTick();
    if(acc != NULL) {
        strncpy(s_screen1_last_fail_acc, acc, sizeof(s_screen1_last_fail_acc) - 1u);
        s_screen1_last_fail_acc[sizeof(s_screen1_last_fail_acc) - 1u] = '\0';
    }
    s_screen1_fail_streak++;
    if(s_screen1_fail_streak >= SCREEN1_LOCKOUT_FAIL_LIMIT) {
        s_screen1_fail_streak = 0u;
        s_screen1_lock_until_ms = now + SCREEN1_LOCKOUT_DURATION_MS;
        s_screen1_alert_pending = 1u;
        s_screen1_lockout_ui_ms = now;
        screen1_show_lockout_label(SCREEN1_LOCKOUT_DURATION_MS / 1000u);
        screen1_lockout_fire_alert();
        return 2u;
    }
    screen1_show_error_label();
    return 0u;
}

void screen1_handle_input_key(KeyValue_t key)
{
    lv_obj_t *ta;
    uint16_t max_len;

    if(screen1_is_lockout_active() != 0u) {
        return;
    }
    ta = screen1_get_field_by_index(g_screen1_field_index);
    max_len = screen1_get_field_max_len(g_screen1_field_index);
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
