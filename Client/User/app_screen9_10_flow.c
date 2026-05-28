#include "app_screen9_10_flow.h"

#include <stdio.h>

#include "gui_guider.h"
#include "ui.h"
#include "ui_common_utils.h"
#include "ui_screen8_utils.h"
#include "app_screen.h"
#include "app_user_ops.h"
#include "app_screen8_enroll.h"

extern lv_ui guider_ui;
extern app_scr_t g_app_scr;
extern uint8_t g_screen9_focus;
extern uint8_t g_screen10_focus;
extern lv_obj_t *g_screen9_popup;
extern lv_obj_t *g_screen9_popup_btn_yes;
extern lv_obj_t *g_screen9_popup_btn_no;
extern uint8_t g_screen9_choice_yes;
extern uint8_t g_screen9_msgbox_state;
extern char g_screen5_found_acc[13];
extern int g_nfc_op;
extern lv_obj_t *g_screen8_popup;

void screen9_set_focus(uint8_t focus)
{
    ui_screen9_set_focus_style(focus, &g_screen9_focus, guider_ui.screen_9_btn_5, guider_ui.screen_9_btn_6);
}

void screen9_hide_all_msgbox(void)
{
    ui_common_hide_msgbox(&g_screen9_popup, &g_screen9_popup_btn_yes, &g_screen9_popup_btn_no, &g_screen9_msgbox_state);
}

void screen9_popup_btn_style(lv_obj_t *btn, lv_obj_t *lbl, uint8_t selected)
{
    if(!lv_obj_is_valid(btn)) return;
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, selected ? lv_color_hex(0x006bb3) : lv_color_hex(0xe6e6e6), LV_PART_MAIN | LV_STATE_DEFAULT);
    if(lv_obj_is_valid(lbl)) {
        lv_obj_set_style_text_color(lbl, selected ? lv_color_hex(0xffffff) : lv_color_hex(0x4e4e4e), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void screen9_set_msgbox_choice(uint8_t yes_selected)
{
    ui_common_set_yes_no_choice(g_screen9_popup_btn_yes, g_screen9_popup_btn_no, yes_selected, &g_screen9_choice_yes);
}

void screen9_show_delete_confirm_popup(void)
{
    screen9_hide_all_msgbox();
    if(!lv_obj_is_valid(guider_ui.screen_9)) return;
    ui_common_show_yes_no_popup(guider_ui.screen_9, &g_screen9_popup, &g_screen9_popup_btn_yes, &g_screen9_popup_btn_no,
                                "\xE7\xA1\xAE\xE5\xAE\x9A\xE5\x88\xA0\xE9\x99\xA4NFC", &g_screen9_msgbox_state, 1u);
    screen9_set_msgbox_choice(1u);
}

void screen10_show_delete_confirm_popup(void)
{
    screen9_hide_all_msgbox();
    if(!lv_obj_is_valid(guider_ui.screen_10)) return;
    ui_common_show_yes_no_popup(guider_ui.screen_10, &g_screen9_popup, &g_screen9_popup_btn_yes, &g_screen9_popup_btn_no,
                                "\xE7\xA1\xAE\xE5\xAE\x9A\xE5\x88\xA0\xE9\x99\xA4\x46\x50", &g_screen9_msgbox_state, 1u);
    screen9_set_msgbox_choice(1u);
}

void screen9_show_delete_result_popup(uint8_t success)
{
    screen9_hide_all_msgbox();
    if(!lv_obj_is_valid(guider_ui.screen_9)) return;
    ui_common_show_result_popup(guider_ui.screen_9, &g_screen9_popup, success ? "删除成功" : "删除失败",
                                &g_screen9_msgbox_state, success ? 2u : 3u);
}

void screen10_show_delete_result_popup(uint8_t success)
{
    screen9_hide_all_msgbox();
    if(!lv_obj_is_valid(guider_ui.screen_10)) return;
    ui_common_show_result_popup(guider_ui.screen_10, &g_screen9_popup, success ? "删除成功" : "删除失败",
                                &g_screen9_msgbox_state, success ? 2u : 3u);
}

uint8_t screen10_try_delete_fp(void)
{
    uint16_t page = 0xFFFFu;
    if(g_screen5_found_acc[0] == '\0') return 0u;
    if(!users_has_fp_by_acc(g_screen5_found_acc, &page)) {
        return 0u;
    }
    users_clear_fp_by_acc(g_screen5_found_acc, 1u);
    return 1u;
}

void screen9_start_nfc_replace(void)
{
    g_nfc_op = 2;
    screen8_show_nfc_enroll_popup();
}

void enter_screen_9(void)
{
    ui_load_scr_animation(&guider_ui, &guider_ui.screen_9, guider_ui.screen_9_del, &guider_ui.screen_6_del,
                          setup_scr_screen_9, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_9;
    screen9_set_focus(0u);
    g_screen9_popup = NULL;
    g_screen9_popup_btn_yes = NULL;
    g_screen9_popup_btn_no = NULL;
    g_screen9_choice_yes = 1u;
    g_screen9_msgbox_state = 0u;
}

void screen10_set_focus(uint8_t focus)
{
    ui_screen10_set_focus_style(focus, &g_screen10_focus,
                                guider_ui.screen_10_btn_2, guider_ui.screen_10_btn_1, guider_ui.screen_10_btn_3);
}

void enter_screen_10(void)
{
    ui_load_scr_animation(&guider_ui, &guider_ui.screen_10, guider_ui.screen_10_del, &guider_ui.screen_6_del,
                          setup_scr_screen_10, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_10;
    g_screen10_focus = 0u;
    g_screen8_popup = NULL;
    screen10_set_focus(0u);
}
