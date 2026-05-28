#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>
#include "lvgl.h"
#include "gui_guider.h"
#include "app_screen.h"
#include "app_screen6_types.h"
#include "app_user_ops.h"
#include "user_auth_portable.h"

typedef enum {
    NFC_OP_NONE = 0,
    NFC_OP_ENROLL_SCREEN8,
    NFC_OP_REPLACE_SCREEN9
} nfc_op_t;

extern lv_ui guider_ui;

extern volatile app_scr_t g_app_scr;
/* 1=WiFi 设置页独占：Cloud 仅扫描/连热点，停 MQTT/OTA */
extern volatile uint8_t g_wifi_exclusive;
extern volatile uint8_t g_wifi_scan_pending;
extern volatile uint8_t g_wifi_scan_abort;
extern uint8_t g_lock_btn_selected;
extern uint8_t g_menu_btn_selected;

extern uint8_t g_screen1_field_index;
extern lv_obj_t *g_screen1_cursor;
extern uint32_t g_cursor_last_toggle_ms;
extern uint8_t g_cursor_visible;
extern lv_obj_t *g_screen1_unlock_popup;
extern lv_timer_t *g_screen1_unlock_timer;

extern uint8_t g_screen2_field_index;
extern lv_obj_t *g_screen2_cursor;
extern uint32_t g_screen2_cursor_last_toggle_ms;
extern uint8_t g_screen2_cursor_visible;
extern uint8_t g_screen3_menu_index;

extern user_cred_t g_users[APP_USER_MAX];
extern uint8_t g_user_count;
extern uint8_t g_default_admin_deleted;
extern uint8_t g_default_admin_is_admin_role;

extern uint8_t g_screen7_choice_yes;
extern uint8_t g_screen7_msgbox_state;
extern lv_obj_t *g_screen7_cursor;
extern uint32_t g_screen7_cursor_last_ms;
extern uint8_t g_screen7_cursor_visible;
extern uint8_t g_screen7_need_init;
extern lv_obj_t *g_screen7_popup;
extern lv_obj_t *g_screen7_popup_btn_yes;
extern lv_obj_t *g_screen7_popup_btn_no;

extern uint8_t g_screen3_need_init;
extern uint8_t g_screen3_pending_index;
extern uint8_t g_screen4_need_init;

extern uint8_t g_screen5_need_init;
extern lv_obj_t *g_screen5_cursor;
extern uint32_t g_screen5_cursor_last_ms;
extern uint8_t g_screen5_cursor_visible;
extern uint8_t g_screen5_found_is_admin;
extern char g_screen5_found_acc[13];

extern uint8_t g_screen6_focus;
extern lv_obj_t *g_screen6_acc_val_label;
extern lv_obj_t *g_screen6_pwd_val_label;

extern screen6_dlg_t g_screen6_dlg;
extern uint8_t g_screen6_dlg_saved_focus;
extern lv_obj_t *g_screen6_dlg_root;
extern lv_obj_t *g_screen6_dlg_btn_close;
extern lv_obj_t *g_screen6_dlg_btn_ok;
extern lv_obj_t *g_screen6_dlg_btn_esc;
extern lv_obj_t *g_screen6_dlg_ta;
extern lv_obj_t *g_screen6_dlg_cursor;
extern uint32_t g_screen6_dlg_cursor_ms;
extern uint8_t g_screen6_dlg_cursor_vis;

extern char g_default_admin_account[13];
extern char g_default_admin_password[11];
extern uint8_t g_default_admin_has_nfc;
extern uint8_t g_default_admin_nfc_uid[4];
extern uint8_t g_default_admin_has_fp;
extern uint16_t g_default_admin_fp_page_id_1;
extern uint16_t g_default_admin_fp_page_id_2;

extern uint8_t g_screen8_focus;
extern lv_obj_t *g_screen8_cursor;
extern uint32_t g_screen8_cursor_last_ms;
extern uint8_t g_screen8_cursor_visible;
extern uint8_t g_screen8_chip_read_ok;
extern uint8_t g_screen8_fp_enroll_state;
extern uint8_t g_screen8_fp_enroll_result;
extern uint32_t g_screen8_fp_enroll_start_time;
extern uint8_t g_fp_hw_inited;
extern uint8_t g_fp_enroll_step;
extern uint16_t g_fp_enroll_page_id;
extern uint16_t g_fp_enroll_page_id_2;
extern lv_obj_t *g_screen8_popup;

extern uint8_t g_screen9_focus;
extern uint8_t g_screen10_focus;
extern lv_obj_t *g_screen9_popup;
extern lv_obj_t *g_screen9_popup_btn_yes;
extern lv_obj_t *g_screen9_popup_btn_no;
extern uint8_t g_screen9_choice_yes;
extern uint8_t g_screen9_msgbox_state;

extern uint32_t g_nfc_enroll_start_time;
extern uint8_t g_nfc_enroll_state;
extern uint8_t g_nfc_enroll_result;
extern volatile uint8_t g_nfc_last_detect_result;
extern volatile uint8_t g_nfc_last_uid[4];
extern uint32_t g_home_fp_last_poll_ms;

extern int g_nfc_op;
extern uint8_t g_nfc_pending_uid_valid;
extern uint8_t g_nfc_pending_uid[4];
extern uint8_t g_fp_pending_page_valid;
extern uint16_t g_fp_pending_page_id;
extern uint8_t g_fp_pending_page2_valid;
extern uint16_t g_fp_pending_page_id_2;
extern lv_timer_t *g_screen8_result_timer;

#endif
