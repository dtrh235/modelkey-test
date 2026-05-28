#include "app_screen5_flow.h"

#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "app_screen.h"
#include "ui.h"
#include "ui_common_utils.h"
#include "ui_auth_input.h"
#include "app_user_ops.h"
#include "user_auth_portable.h"

extern lv_ui guider_ui;
extern volatile app_scr_t g_app_scr;
extern lv_obj_t *g_screen5_cursor;
extern uint32_t g_screen5_cursor_last_ms;
extern uint8_t g_screen5_cursor_visible;
extern uint8_t g_screen5_need_init;
extern uint8_t g_screen5_found_is_admin;
extern char g_screen5_found_acc[13];

extern user_cred_t g_users[APP_USER_MAX];
extern uint8_t g_default_admin_deleted;
extern uint8_t g_default_admin_is_admin_role;
extern char g_default_admin_account[13];

void screen5_hide_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_5_label_3, 1u);
}

void screen5_show_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_5_label_3, 0u);
}

static void screen5_update_cursor_pos(void)
{
    lv_obj_t *ta = guider_ui.screen_5_ta_1;
    ui_auth_update_cursor_pos(g_screen5_cursor, ta);
}

void screen5_set_field_selected(void)
{
    lv_obj_t *ta = guider_ui.screen_5_ta_1;
    if(!lv_obj_is_valid(ta)) return;
    ui_textarea_apply_input_style(ta, 1u);
    ui_cursor_attach_bar(&g_screen5_cursor, ta);
    screen5_update_cursor_pos();
    ui_cursor_activate(g_screen5_cursor, &g_screen5_cursor_visible, &g_screen5_cursor_last_ms);
}

void screen5_handle_input_key(KeyValue_t key)
{
    lv_obj_t *ta = guider_ui.screen_5_ta_1;
    ui_auth_handle_input_key(ta, key, 12u, screen5_hide_error_label, screen5_update_cursor_pos);
}

bool screen5_try_lookup_acc(void)
{
    const char *acc;
    size_t len;
    int idx;
    bool found = false;
    bool is_admin = false;

    if(!lv_obj_is_valid(guider_ui.screen_5_ta_1)) return false;

    acc = lv_textarea_get_text(guider_ui.screen_5_ta_1);
    len = strlen(acc);
    if(len < 1u || len > 12u) return false;

    if(strcmp(acc, g_default_admin_account) == 0) {
        if(!g_default_admin_deleted) {
            found = true;
            is_admin = g_default_admin_is_admin_role ? true : false;
        }
    } else {
        idx = users_find_index_by_acc(acc);
        if(idx >= 0) {
            found = true;
            is_admin = (g_users[idx].is_admin ? true : false);
        }
    }

    if(!found) return false;

    strncpy(g_screen5_found_acc, acc, sizeof(g_screen5_found_acc) - 1u);
    g_screen5_found_acc[sizeof(g_screen5_found_acc) - 1u] = '\0';
    g_screen5_found_is_admin = is_admin ? 1u : 0u;
    return true;
}

void screen5_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_5) return;
    ui_cursor_blink_step(g_screen5_cursor, &g_screen5_cursor_visible, &g_screen5_cursor_last_ms, 500u);
}

void enter_screen_5(void)
{
    g_screen5_found_acc[0] = '\0';
    g_screen5_found_is_admin = 0u;

    ui_load_scr_animation(&guider_ui, &guider_ui.screen_5, guider_ui.screen_5_del, &guider_ui.screen_3_del,
                          setup_scr_screen_5, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_5;
    g_screen5_cursor = NULL;
    g_screen5_need_init = 1u;
}
