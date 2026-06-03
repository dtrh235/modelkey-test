#include "app_nav_entries.h"

#include "app_config.h"
#include "app_home_unlock.h"
#include "app_slave_ui_fixup.h"

#include "lvgl.h"
#include "gui_guider.h"
#include "ui_nav_flow.h"
#include "app_screen.h"
#include "app_screen_auth.h"
#include "app_screen4_table.h"
#include "app_screen6_dialog_base.h"
#include "app_screen6_types.h"
#include "app_screen6_info.h"
#include "app_screen7_flow.h"
#include "app_screen8_popup.h"
#include "app_screen1_unlock.h"
#include "app_screen9_10_flow.h"
#include "app_state.h"
#include "app_home_nav_btns.h"

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20);

static void nav_lock_btn_set_home_unselected(void)
{
    lock_btn_set_selected(0u);
}

static void nav_menu_btn_set_home_unselected(void)
{
    menu_btn_set_selected(0u);
}

static void nav_screen6_close_dialog_and_reset(void)
{
    screen6_close_dialog();
    g_screen6_dlg = SCREEN6_DLG_NONE;
    screen8_popup_close_only();
    g_nfc_enroll_state = 0u;
    g_screen8_fp_enroll_state = 0u;
}

static void nav_screen2_restore_after_back_from_menu(void)
{
    if(lv_obj_is_valid(guider_ui.screen_2_ta_1)) {
        lv_textarea_set_password_bullet(guider_ui.screen_2_ta_1, "*");
        lv_textarea_set_password_mode(guider_ui.screen_2_ta_1, true);
    }
    g_screen3_menu_index = 0u;
}

static ui_nav_ctx_t build_nav_ctx(void)
{
    ui_nav_ctx_t ctx;
    ctx.ui = &guider_ui;
    ctx.app_scr = &g_app_scr;
    ctx.screen1_field_index = &g_screen1_field_index;
    ctx.cursor_visible = &g_cursor_visible;
    ctx.screen1_cursor = &g_screen1_cursor;
    ctx.screen2_field_index = &g_screen2_field_index;
    ctx.screen2_cursor_visible = &g_screen2_cursor_visible;
    ctx.screen2_cursor = &g_screen2_cursor;
    ctx.screen3_menu_index = &g_screen3_menu_index;
    ctx.screen3_need_init = &g_screen3_need_init;
    ctx.screen3_pending_index = &g_screen3_pending_index;
    ctx.screen4_need_init = &g_screen4_need_init;
    ctx.screen5_need_init = &g_screen5_need_init;
    ctx.screen5_cursor = &g_screen5_cursor;
    ctx.screen7_cursor = &g_screen7_cursor;
    ctx.screen7_need_init = &g_screen7_need_init;
    ctx.screen8_popup = &g_screen8_popup;
    ctx.screen8_cursor = &g_screen8_cursor;
    ctx.screen8_fp_enroll_state = &g_screen8_fp_enroll_state;
    ctx.screen8_result_timer = &g_screen8_result_timer;
    ctx.screen5_found_is_admin = &g_screen5_found_is_admin;
    ctx.nfc_op = &g_nfc_op;
    ctx.nfc_enroll_state = &g_nfc_enroll_state;
    return ctx;
}

static ui_nav_ops_t build_nav_ops(void)
{
    ui_nav_ops_t ops;
    ops.lock_btn_set_home_unselected = nav_lock_btn_set_home_unselected;
    ops.menu_btn_set_home_unselected = nav_menu_btn_set_home_unselected;
    ops.screen1_cancel_unlock_popup = screen1_cancel_unlock_popup;
    ops.screen2_set_field_selected = screen2_set_field_selected;
    ops.screen2_hide_error_label = screen2_hide_error_label;
    ops.screen2_restore_after_back_from_menu = nav_screen2_restore_after_back_from_menu;
    ops.screen4_refresh_table = screen4_refresh_table;
    ops.screen6_close_dialog_and_reset = nav_screen6_close_dialog_and_reset;
    ops.screen6_update_labels_from_found = screen6_update_labels_from_found;
    ops.screen6_set_focus = screen6_set_focus;
    ops.screen9_hide_all_msgbox = screen9_hide_all_msgbox;
    ops.screen8_popup_close_only = screen8_popup_close_only;
    return ops;
}

void enter_screen_1(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_ops_t ops = build_nav_ops();
    ui_nav_enter_screen_1(&ctx, &ops);
    app_slave_ui_hide_corner_ok_esc();
    app_slave_ui_sanitize_screen1_textareas();
    screen1_set_field_selected(0);
    screen1_hide_error_label();
    if(lv_obj_is_valid(guider_ui.screen_1_ta_2)) {
        lv_textarea_set_password_bullet(guider_ui.screen_1_ta_2, "*");
        lv_textarea_set_password_mode(guider_ui.screen_1_ta_2, true);
    }
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
    app_home_nfc_poll_arm_immediate();
    app_home_fp_poll_arm_immediate();
#endif
}

void enter_screen_2(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_ops_t ops = build_nav_ops();
    ui_nav_enter_screen_2(&ctx, &ops);
    screen2_set_field_selected(0);
    screen2_hide_error_label();
    if(lv_obj_is_valid(guider_ui.screen_2_ta_1)) {
        lv_textarea_set_password_bullet(guider_ui.screen_2_ta_1, "*");
        lv_textarea_set_password_mode(guider_ui.screen_2_ta_1, true);
    }
}

void enter_screen_3(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_ops_t ops = build_nav_ops();
    ui_nav_enter_screen_3(&ctx, &ops);
}

void enter_screen_4(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_ops_t ops = build_nav_ops();
    ui_nav_enter_screen_4(&ctx, &ops);
}

void enter_screen_6(void)
{
    lv_obj_t *dd_list;
    screen8_popup_close_only();
    g_nfc_enroll_state = 0u;
    g_screen8_fp_enroll_state = 0u;
    screen6_close_dialog();
    g_screen6_dlg = SCREEN6_DLG_NONE;

    ui_load_scr_animation(&guider_ui, &guider_ui.screen_6, guider_ui.screen_6_del, &guider_ui.screen_5_del,
                          setup_scr_screen_6, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_6;

    screen6_update_labels_from_found();
    if(lv_obj_is_valid(guider_ui.screen_6_ddlist_1)) {
        lv_dropdown_set_selected(guider_ui.screen_6_ddlist_1, g_screen5_found_is_admin ? 1u : 0u);
        dd_list = lv_dropdown_get_list(guider_ui.screen_6_ddlist_1);
        if(lv_obj_is_valid(dd_list)) {
            lv_obj_set_style_text_font(dd_list, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(dd_list, lv_color_hex(0x0D3055), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(dd_list, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_SELECTED | LV_STATE_CHECKED);
            lv_obj_set_style_text_color(dd_list, lv_color_hex(0xffffff), LV_PART_SELECTED | LV_STATE_CHECKED);
        }
    }
    if(lv_obj_is_valid(guider_ui.screen_6_btn_3)) {
        lv_obj_add_flag(guider_ui.screen_6_btn_3, LV_OBJ_FLAG_HIDDEN);
    }
    g_screen6_focus = 0u;
    screen6_set_focus(g_screen6_focus);
    if(lv_obj_is_valid(guider_ui.screen_6_ddlist_1)) lv_dropdown_close(guider_ui.screen_6_ddlist_1);

    if(lv_obj_is_valid(guider_ui.screen_6_btn_2_label)) {
        lv_label_set_text(guider_ui.screen_6_btn_2_label, "ESC");
        lv_obj_set_style_text_font(guider_ui.screen_6_btn_2_label, &lv_font_SourceHanSerifSC_Regular_20,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void enter_screen_7(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_enter_screen_7(&ctx);
}

void go_back_prev_screen(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_ops_t ops = build_nav_ops();
    ui_nav_go_back_prev_screen(&ctx, &ops);
}
