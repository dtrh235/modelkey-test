#ifndef UI_NAV_FLOW_H
#define UI_NAV_FLOW_H

#include <stdint.h>
#include "lvgl.h"
#include "gui_guider.h"
#include "app_screen.h"

typedef void (*ui_nav_void_cb_t)(void);
typedef void (*ui_nav_u8_cb_t)(uint8_t v);

typedef struct {
    lv_ui *ui;
    app_scr_t *app_scr;

    uint8_t *screen1_field_index;
    uint8_t *cursor_visible;
    lv_obj_t **screen1_cursor;

    uint8_t *screen2_field_index;
    uint8_t *screen2_cursor_visible;
    lv_obj_t **screen2_cursor;

    uint8_t *screen3_menu_index;
    uint8_t *screen3_need_init;
    uint8_t *screen3_pending_index;

    uint8_t *screen4_need_init;
    uint8_t *screen5_need_init;
    lv_obj_t **screen5_cursor;

    lv_obj_t **screen7_cursor;
    uint8_t *screen7_need_init;

    lv_obj_t **screen8_popup;
    lv_obj_t **screen8_cursor;
    uint8_t *screen8_fp_enroll_state;
    lv_timer_t **screen8_result_timer;

    uint8_t *screen5_found_is_admin;
    int *nfc_op;
    uint8_t *nfc_enroll_state;
} ui_nav_ctx_t;

typedef struct {
    ui_nav_void_cb_t lock_btn_set_home_unselected;
    ui_nav_void_cb_t menu_btn_set_home_unselected;
    ui_nav_void_cb_t screen1_cancel_unlock_popup;
    ui_nav_u8_cb_t screen2_set_field_selected;
    ui_nav_void_cb_t screen2_hide_error_label;
    ui_nav_void_cb_t screen2_restore_after_back_from_menu;
    ui_nav_void_cb_t screen4_refresh_table;
    ui_nav_void_cb_t screen6_close_dialog_and_reset;
    ui_nav_void_cb_t screen6_update_labels_from_found;
    ui_nav_u8_cb_t screen6_set_focus;
    ui_nav_void_cb_t screen9_hide_all_msgbox;
    ui_nav_void_cb_t screen8_popup_close_only;
} ui_nav_ops_t;

void ui_nav_enter_screen_1(ui_nav_ctx_t *ctx, const ui_nav_ops_t *ops);
void ui_nav_enter_screen_2(ui_nav_ctx_t *ctx, const ui_nav_ops_t *ops);
void ui_nav_enter_screen_3(ui_nav_ctx_t *ctx, const ui_nav_ops_t *ops);
void ui_nav_enter_screen_4(ui_nav_ctx_t *ctx, const ui_nav_ops_t *ops);
void ui_nav_enter_screen_7(ui_nav_ctx_t *ctx);
void ui_nav_go_back_prev_screen(ui_nav_ctx_t *ctx, const ui_nav_ops_t *ops);

#endif
