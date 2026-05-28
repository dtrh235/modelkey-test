#include "app_state.h"

lv_ui guider_ui;

volatile app_scr_t g_app_scr = APP_SCR_HOME;
volatile uint8_t g_wifi_exclusive = 0u;
volatile uint8_t g_wifi_scan_pending = 0u;
volatile uint8_t g_wifi_scan_abort = 0u;
uint8_t g_lock_btn_selected = 0;
uint8_t g_menu_btn_selected = 0;

uint8_t g_screen1_field_index = 0;
lv_obj_t *g_screen1_cursor = NULL;
uint32_t g_cursor_last_toggle_ms = 0;
uint8_t g_cursor_visible = 1;
lv_obj_t *g_screen1_unlock_popup = NULL;
lv_timer_t *g_screen1_unlock_timer = NULL;

uint8_t g_screen2_field_index = 0;
lv_obj_t *g_screen2_cursor = NULL;
uint32_t g_screen2_cursor_last_toggle_ms = 0;
uint8_t g_screen2_cursor_visible = 1;
uint8_t g_screen3_menu_index = 0;

user_cred_t g_users[APP_USER_MAX];
uint8_t g_user_count = 0;
uint8_t g_default_admin_deleted = 0u;
uint8_t g_default_admin_is_admin_role = 1u;

uint8_t g_screen7_choice_yes = 1u;
uint8_t g_screen7_msgbox_state = 0u;
lv_obj_t *g_screen7_cursor = NULL;
uint32_t g_screen7_cursor_last_ms = 0;
uint8_t g_screen7_cursor_visible = 1;
uint8_t g_screen7_need_init = 0u;
lv_obj_t *g_screen7_popup = NULL;
lv_obj_t *g_screen7_popup_btn_yes = NULL;
lv_obj_t *g_screen7_popup_btn_no = NULL;

uint8_t g_screen3_need_init = 0u;
uint8_t g_screen3_pending_index = 0u;
uint8_t g_screen4_need_init = 0u;

uint8_t g_screen5_need_init = 0u;
lv_obj_t *g_screen5_cursor = NULL;
uint32_t g_screen5_cursor_last_ms = 0;
uint8_t g_screen5_cursor_visible = 1;
uint8_t g_screen5_found_is_admin = 0u;
char g_screen5_found_acc[13] = {0};

uint8_t g_screen6_focus = 0u;
lv_obj_t *g_screen6_acc_val_label = NULL;
lv_obj_t *g_screen6_pwd_val_label = NULL;

screen6_dlg_t g_screen6_dlg = SCREEN6_DLG_NONE;
uint8_t g_screen6_dlg_saved_focus = 0u;
lv_obj_t *g_screen6_dlg_root = NULL;
lv_obj_t *g_screen6_dlg_btn_close = NULL;
lv_obj_t *g_screen6_dlg_btn_ok = NULL;
lv_obj_t *g_screen6_dlg_btn_esc = NULL;
lv_obj_t *g_screen6_dlg_ta = NULL;
lv_obj_t *g_screen6_dlg_cursor = NULL;
uint32_t g_screen6_dlg_cursor_ms = 0;
uint8_t g_screen6_dlg_cursor_vis = 1u;

char g_default_admin_account[13];
char g_default_admin_password[11];
uint8_t g_default_admin_has_nfc = 0u;
uint8_t g_default_admin_nfc_uid[4] = {0u, 0u, 0u, 0u};
uint8_t g_default_admin_has_fp = 0u;
uint16_t g_default_admin_fp_page_id_1 = 0xFFFFu;
uint16_t g_default_admin_fp_page_id_2 = 0xFFFFu;

uint8_t g_screen8_focus = 0;
lv_obj_t *g_screen8_cursor = NULL;
uint32_t g_screen8_cursor_last_ms = 0;
uint8_t g_screen8_cursor_visible = 1;
uint8_t g_screen8_chip_read_ok = 0;
uint8_t g_screen8_fp_enroll_state = 0u;
uint8_t g_screen8_fp_enroll_result = 0u;
uint32_t g_screen8_fp_enroll_start_time = 0u;
uint8_t g_fp_hw_inited = 0u;
uint8_t g_fp_enroll_step = 0u;
uint16_t g_fp_enroll_page_id = 0u;
uint16_t g_fp_enroll_page_id_2 = 0u;
lv_obj_t *g_screen8_popup = NULL;

uint8_t g_screen9_focus = 0u;
uint8_t g_screen10_focus = 0u;
lv_obj_t *g_screen9_popup = NULL;
lv_obj_t *g_screen9_popup_btn_yes = NULL;
lv_obj_t *g_screen9_popup_btn_no = NULL;
uint8_t g_screen9_choice_yes = 1u;
uint8_t g_screen9_msgbox_state = 0u;

uint32_t g_nfc_enroll_start_time = 0;
uint8_t g_nfc_enroll_state = 0;
uint8_t g_nfc_enroll_result = 0;
volatile uint8_t g_nfc_last_detect_result = 0u;
volatile uint8_t g_nfc_last_uid[4] = {0u, 0u, 0u, 0u};
uint32_t g_home_fp_last_poll_ms = 0u;

int g_nfc_op = NFC_OP_NONE;
uint8_t g_nfc_pending_uid_valid = 0u;
uint8_t g_nfc_pending_uid[4] = {0u, 0u, 0u, 0u};
uint8_t g_fp_pending_page_valid = 0u;
uint16_t g_fp_pending_page_id = 0xFFFFu;
uint8_t g_fp_pending_page2_valid = 0u;
uint16_t g_fp_pending_page_id_2 = 0xFFFFu;
lv_timer_t *g_screen8_result_timer = NULL;
