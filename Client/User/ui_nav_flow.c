#include "ui_nav_flow.h"

#include "ui_common_utils.h"

enum { UI_NAV_NFC_OP_NONE = 0 };

static void set_home_unselected(const ui_nav_ops_t *ops)
{
    if(ops == NULL) return;
    if(ops->lock_btn_set_home_unselected != NULL) ops->lock_btn_set_home_unselected();
    if(ops->menu_btn_set_home_unselected != NULL) ops->menu_btn_set_home_unselected();
}

void ui_nav_enter_screen_1(ui_nav_ctx_t *ctx, const ui_nav_ops_t *ops)
{
    if(ctx == NULL || ctx->ui == NULL || ctx->app_scr == NULL) return;
    if(ops != NULL && ops->screen1_cancel_unlock_popup != NULL) ops->screen1_cancel_unlock_popup();
    set_home_unselected(ops);
    ui_load_scr_animation(ctx->ui, &ctx->ui->screen_1, ctx->ui->screen_1_del, &ctx->ui->screen_del,
                          setup_scr_screen_1, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    *ctx->app_scr = APP_SCR_1;
}

void ui_nav_enter_screen_2(ui_nav_ctx_t *ctx, const ui_nav_ops_t *ops)
{
    if(ctx == NULL || ctx->ui == NULL || ctx->app_scr == NULL) return;
    set_home_unselected(ops);
    ui_load_scr_animation(ctx->ui, &ctx->ui->screen_2, ctx->ui->screen_2_del, &ctx->ui->screen_del,
                          setup_scr_screen_2, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    *ctx->app_scr = APP_SCR_2;
}

void ui_nav_enter_screen_3(ui_nav_ctx_t *ctx, const ui_nav_ops_t *ops)
{
    if(ctx == NULL || ctx->ui == NULL || ctx->app_scr == NULL) return;
    if(ops != NULL && ops->screen2_hide_error_label != NULL) ops->screen2_hide_error_label();
    ui_load_scr_animation(ctx->ui, &ctx->ui->screen_3, ctx->ui->screen_3_del, &ctx->ui->screen_2_del,
                          setup_scr_screen_3, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    *ctx->app_scr = APP_SCR_3;
    if(ctx->screen3_pending_index != NULL) *ctx->screen3_pending_index = 0u;
    if(ctx->screen3_need_init != NULL) *ctx->screen3_need_init = 1u;
}

void ui_nav_enter_screen_4(ui_nav_ctx_t *ctx, const ui_nav_ops_t *ops)
{
    if(ctx == NULL || ctx->ui == NULL || ctx->app_scr == NULL) return;
    ui_load_scr_animation(ctx->ui, &ctx->ui->screen_4, ctx->ui->screen_4_del, &ctx->ui->screen_3_del,
                          setup_scr_screen_4, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    *ctx->app_scr = APP_SCR_4;
    if(ops != NULL && ops->screen4_refresh_table != NULL) ops->screen4_refresh_table();
    if(ctx->screen4_need_init != NULL) *ctx->screen4_need_init = 1u;
}

void ui_nav_enter_screen_7(ui_nav_ctx_t *ctx)
{
    if(ctx == NULL || ctx->ui == NULL || ctx->app_scr == NULL) return;
    ui_load_scr_animation(ctx->ui, &ctx->ui->screen_7, ctx->ui->screen_7_del, &ctx->ui->screen_3_del,
                          setup_scr_screen_7, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    *ctx->app_scr = APP_SCR_7;
    if(ctx->screen7_cursor != NULL) *ctx->screen7_cursor = NULL;
    if(ctx->screen7_need_init != NULL) *ctx->screen7_need_init = 1u;
}

void ui_nav_go_back_prev_screen(ui_nav_ctx_t *ctx, const ui_nav_ops_t *ops)
{
    if(ctx == NULL || ctx->ui == NULL || ctx->app_scr == NULL) return;

    if(*ctx->app_scr == APP_SCR_1) {
        if(ops != NULL && ops->screen1_cancel_unlock_popup != NULL) ops->screen1_cancel_unlock_popup();
        ui_load_scr_animation(ctx->ui, &ctx->ui->screen, ctx->ui->screen_del, &ctx->ui->screen_1_del,
                              setup_scr_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        *ctx->app_scr = APP_SCR_HOME;
        if(ctx->screen1_field_index != NULL) *ctx->screen1_field_index = 0u;
        if(ctx->cursor_visible != NULL) *ctx->cursor_visible = 1u;
        if(ctx->screen1_cursor != NULL) *ctx->screen1_cursor = NULL;
        set_home_unselected(ops);
    } else if(*ctx->app_scr == APP_SCR_2) {
        ui_load_scr_animation(ctx->ui, &ctx->ui->screen, ctx->ui->screen_del, &ctx->ui->screen_2_del,
                              setup_scr_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        *ctx->app_scr = APP_SCR_HOME;
        if(ctx->screen2_field_index != NULL) *ctx->screen2_field_index = 0u;
        if(ctx->screen2_cursor_visible != NULL) *ctx->screen2_cursor_visible = 1u;
        if(ctx->screen2_cursor != NULL) *ctx->screen2_cursor = NULL;
        set_home_unselected(ops);
    } else if(*ctx->app_scr == APP_SCR_3) {
        ui_load_scr_animation(ctx->ui, &ctx->ui->screen_2, ctx->ui->screen_2_del, &ctx->ui->screen_3_del,
                              setup_scr_screen_2, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        *ctx->app_scr = APP_SCR_2;
        if(ctx->screen2_field_index != NULL) *ctx->screen2_field_index = 0u;
        if(ctx->screen2_cursor_visible != NULL) *ctx->screen2_cursor_visible = 1u;
        if(ctx->screen2_cursor != NULL) *ctx->screen2_cursor = NULL;
        if(ops != NULL && ops->screen2_set_field_selected != NULL) ops->screen2_set_field_selected(0u);
        if(ops != NULL && ops->screen2_hide_error_label != NULL) ops->screen2_hide_error_label();
        if(ops != NULL && ops->screen2_restore_after_back_from_menu != NULL) {
            ops->screen2_restore_after_back_from_menu();
        }
    } else if(*ctx->app_scr == APP_SCR_7) {
        ui_load_scr_animation(ctx->ui, &ctx->ui->screen_3, ctx->ui->screen_3_del, &ctx->ui->screen_7_del,
                              setup_scr_screen_3, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        *ctx->app_scr = APP_SCR_3;
        if(ctx->screen7_cursor != NULL) *ctx->screen7_cursor = NULL;
        if(ctx->screen7_need_init != NULL) *ctx->screen7_need_init = 0u;
        if(ctx->screen3_pending_index != NULL) *ctx->screen3_pending_index = 1u;
        if(ctx->screen3_need_init != NULL) *ctx->screen3_need_init = 1u;
    } else if(*ctx->app_scr == APP_SCR_8) {
        ui_load_scr_animation(ctx->ui, &ctx->ui->screen_3, ctx->ui->screen_3_del, &ctx->ui->screen_8_del,
                              setup_scr_screen_3, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        *ctx->app_scr = APP_SCR_3;
        if(ctx->screen8_popup != NULL) *ctx->screen8_popup = NULL;
        if(ctx->screen8_cursor != NULL) *ctx->screen8_cursor = NULL;
        if(ctx->screen3_pending_index != NULL && ctx->screen3_menu_index != NULL) {
            *ctx->screen3_pending_index = *ctx->screen3_menu_index;
        }
        if(ctx->screen3_need_init != NULL) *ctx->screen3_need_init = 1u;
    } else if(*ctx->app_scr == APP_SCR_4) {
        ui_load_scr_animation(ctx->ui, &ctx->ui->screen_3, ctx->ui->screen_3_del, &ctx->ui->screen_4_del,
                              setup_scr_screen_3, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        *ctx->app_scr = APP_SCR_3;
        if(ctx->screen3_pending_index != NULL) *ctx->screen3_pending_index = 3u;
        if(ctx->screen3_need_init != NULL) *ctx->screen3_need_init = 1u;
        if(ctx->screen4_need_init != NULL) *ctx->screen4_need_init = 0u;
    } else if(*ctx->app_scr == APP_SCR_5) {
        ui_load_scr_animation(ctx->ui, &ctx->ui->screen_3, ctx->ui->screen_3_del, &ctx->ui->screen_5_del,
                              setup_scr_screen_3, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        *ctx->app_scr = APP_SCR_3;
        if(ctx->screen3_pending_index != NULL) *ctx->screen3_pending_index = 2u;
        if(ctx->screen3_need_init != NULL) *ctx->screen3_need_init = 1u;
        if(ctx->screen5_cursor != NULL) *ctx->screen5_cursor = NULL;
        if(ctx->screen5_need_init != NULL) *ctx->screen5_need_init = 0u;
    } else if(*ctx->app_scr == APP_SCR_6) {
        if(ops != NULL && ops->screen6_close_dialog_and_reset != NULL) ops->screen6_close_dialog_and_reset();
        ui_load_scr_animation(ctx->ui, &ctx->ui->screen_3, ctx->ui->screen_3_del, &ctx->ui->screen_6_del,
                              setup_scr_screen_3, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        *ctx->app_scr = APP_SCR_3;
        if(ctx->screen3_pending_index != NULL) *ctx->screen3_pending_index = 2u;
        if(ctx->screen3_need_init != NULL) *ctx->screen3_need_init = 1u;
        if(ctx->screen5_cursor != NULL) *ctx->screen5_cursor = NULL;
        if(ctx->screen5_need_init != NULL) *ctx->screen5_need_init = 0u;
    } else if(*ctx->app_scr == APP_SCR_9) {
        if(ops != NULL && ops->screen9_hide_all_msgbox != NULL) ops->screen9_hide_all_msgbox();
        if(ops != NULL && ops->screen8_popup_close_only != NULL) ops->screen8_popup_close_only();
        if(ctx->nfc_op != NULL) *ctx->nfc_op = UI_NAV_NFC_OP_NONE;
        if(ctx->nfc_enroll_state != NULL) *ctx->nfc_enroll_state = 0u;
        ui_load_scr_animation(ctx->ui, &ctx->ui->screen_6, ctx->ui->screen_6_del, &ctx->ui->screen_9_del,
                              setup_scr_screen_6, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        *ctx->app_scr = APP_SCR_6;
        if(ops != NULL && ops->screen6_update_labels_from_found != NULL) ops->screen6_update_labels_from_found();
        if(lv_obj_is_valid(ctx->ui->screen_6_ddlist_1) && ctx->screen5_found_is_admin != NULL) {
            lv_dropdown_set_selected(ctx->ui->screen_6_ddlist_1, (*ctx->screen5_found_is_admin) ? 1u : 0u);
            lv_dropdown_close(ctx->ui->screen_6_ddlist_1);
        }
        if(ops != NULL && ops->screen6_set_focus != NULL) ops->screen6_set_focus(4u);
    } else if(*ctx->app_scr == APP_SCR_10) {
        if(ops != NULL && ops->screen9_hide_all_msgbox != NULL) ops->screen9_hide_all_msgbox();
        if(ops != NULL && ops->screen8_popup_close_only != NULL) ops->screen8_popup_close_only();
        if(ctx->screen8_result_timer != NULL && *ctx->screen8_result_timer != NULL) {
            lv_timer_del(*ctx->screen8_result_timer);
            *ctx->screen8_result_timer = NULL;
        }
        if(ctx->screen8_fp_enroll_state != NULL) *ctx->screen8_fp_enroll_state = 0u;
        ui_load_scr_animation(ctx->ui, &ctx->ui->screen_6, ctx->ui->screen_6_del, &ctx->ui->screen_10_del,
                              setup_scr_screen_6, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
        *ctx->app_scr = APP_SCR_6;
        if(ops != NULL && ops->screen6_update_labels_from_found != NULL) ops->screen6_update_labels_from_found();
        if(lv_obj_is_valid(ctx->ui->screen_6_ddlist_1) && ctx->screen5_found_is_admin != NULL) {
            lv_dropdown_set_selected(ctx->ui->screen_6_ddlist_1, (*ctx->screen5_found_is_admin) ? 1u : 0u);
            lv_dropdown_close(ctx->ui->screen_6_ddlist_1);
        }
        if(ops != NULL && ops->screen6_set_focus != NULL) ops->screen6_set_focus(3u);
    }
}
