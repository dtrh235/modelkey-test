#include "app_screen7_flow.h"

#include "gui_guider.h"
#include "app_screen.h"
#include "ui_common_utils.h"
#include "ui_auth_input.h"
#include "ui_menu_popup_utils.h"

extern lv_ui guider_ui;
extern volatile app_scr_t g_app_scr;
extern uint8_t g_screen7_choice_yes;
extern uint8_t g_screen7_msgbox_state;
extern lv_obj_t *g_screen7_cursor;
extern uint32_t g_screen7_cursor_last_ms;
extern uint8_t g_screen7_cursor_visible;
extern lv_obj_t *g_screen7_popup;
extern lv_obj_t *g_screen7_popup_btn_yes;
extern lv_obj_t *g_screen7_popup_btn_no;

void screen7_hide_all_msgbox(void)
{
    ui_common_hide_msgbox(&g_screen7_popup, &g_screen7_popup_btn_yes, &g_screen7_popup_btn_no, &g_screen7_msgbox_state);
}

void screen7_popup_btn_style(lv_obj_t *btn, lv_obj_t *lbl, uint8_t selected)
{
    ui_popup_btn_style(btn, lbl, selected);
}

void screen7_set_msgbox1_choice(uint8_t yes_selected)
{
    ui_popup_set_yes_no_choice(g_screen7_popup_btn_yes, g_screen7_popup_btn_no, yes_selected, &g_screen7_choice_yes);
}

static void screen7_show_simple_popup(const char *text, uint8_t popup_state, uint8_t with_yes_no)
{
    screen7_hide_all_msgbox();
    if(!lv_obj_is_valid(guider_ui.screen_7)) return;
    if(with_yes_no) {
        ui_common_show_yes_no_popup(guider_ui.screen_7, &g_screen7_popup, &g_screen7_popup_btn_yes, &g_screen7_popup_btn_no,
                                    text, &g_screen7_msgbox_state, popup_state);
        screen7_set_msgbox1_choice(1u);
    } else {
        ui_common_show_result_popup(guider_ui.screen_7, &g_screen7_popup, text, &g_screen7_msgbox_state, popup_state);
    }
}

void screen7_show_msgbox1(void)
{
    screen7_show_simple_popup("确定删除用户", 1u, 1u);
}

void screen7_show_msgbox2(void)
{
    screen7_show_simple_popup("删除成功", 2u, 0u);
}

void screen7_show_msgbox3(void)
{
    screen7_show_simple_popup("删除失败", 3u, 0u);
}

static void screen7_update_cursor_pos(void)
{
    lv_obj_t *ta = guider_ui.screen_7_ta_1;
    ui_auth_update_cursor_pos(g_screen7_cursor, ta);
}

void screen7_set_field_selected(void)
{
    lv_obj_t *ta = guider_ui.screen_7_ta_1;
    if(!lv_obj_is_valid(ta)) return;
    ui_textarea_apply_input_style(ta, 1u);
    ui_cursor_attach_bar(&g_screen7_cursor, ta);
    screen7_update_cursor_pos();
    ui_cursor_activate(g_screen7_cursor, &g_screen7_cursor_visible, &g_screen7_cursor_last_ms);
}

void screen7_handle_input_key(KeyValue_t key)
{
    lv_obj_t *ta = guider_ui.screen_7_ta_1;
    ui_auth_handle_input_key(ta, key, 12u, NULL, screen7_update_cursor_pos);
}

void screen7_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_7 || g_screen7_msgbox_state != 0u) return;
    ui_cursor_blink_step(g_screen7_cursor, &g_screen7_cursor_visible, &g_screen7_cursor_last_ms, 500u);
}
