#ifndef APP_UI_V3_TYPES_H
#define APP_UI_V3_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

typedef enum {
    UI3_SCR_HOME = 0,
    UI3_SCR_UNLOCK,
    UI3_SCR_ADMIN,
    UI3_SCR_MENU,
    UI3_SCR_ADD_USER,
    UI3_SCR_DEL_USER,
    UI3_SCR_SEARCH,
    UI3_SCR_EDIT_USER,
    UI3_SCR_USER_LIST,
    UI3_SCR_WIFI,
    UI3_SCR_PAIR,
    UI3_SCR_NFC_MGMT,
    UI3_SCR_FP_MGMT,
    UI3_SCR_COUNT
} ui3_scr_id_t;

typedef struct {
    const char *acc;
    uint8_t role_admin;
    uint8_t fp;
    uint8_t nfc;
} ui3_user_t;

#define UI3_LIST_ROW_H     52
#define UI3_LIST_VISIBLE   5
#define UI3_ADD_SCROLL_STEP 38
#define UI3_ADD_SCROLL_MAX  2
#define UI3_TABLE_ROW_H    36
#define UI3_NAV_STACK_MAX  8
#define UI3_HOME_NAV_NONE  2u

typedef struct {
    ui3_scr_id_t scr;
    ui3_scr_id_t stack[UI3_NAV_STACK_MAX];
    uint8_t stack_len;

    uint8_t menu_index;
    uint8_t menu_scroll;
    uint8_t table_scroll;
    uint8_t edit_scroll;
    uint8_t wifi_scroll;
    uint8_t wifi_index;
    uint8_t add_scroll;
    uint8_t add_field;
    uint8_t add_admin;
    uint8_t unlock_field;
    uint8_t admin_field;
    uint8_t home_nav_sel;
    uint8_t home_sensing;
    uint8_t unlock_errors;
    uint8_t unlock_show_err;
    uint8_t admin_show_err;
    uint32_t lockout_until_ms;

    lv_obj_t *lbl_top_time;
    lv_obj_t *lbl_clock_big;
    lv_obj_t *lbl_date;
    lv_obj_t *btn_home_menu;
    lv_obj_t *btn_home_unlock;
    lv_obj_t *menu_track;
    lv_obj_t *inp_wrap[2];
    lv_obj_t *form_err_wrap;
    uint8_t inp_count;

    char pair_code[8];
    char unlock_acc[16];
    char unlock_pwd[16];
    char admin_acc[16];
    char admin_pwd[16];
    char search_acc[16];
    char del_acc[16];
    char add_acc[16];
    char add_pwd[16];

    uint8_t mqtt_online;
    uint8_t wifi_scanning;
    char edit_acc[16];
    uint8_t edit_is_admin;
    uint8_t edit_has_fp;
    uint8_t edit_has_nfc;

    /* runtime refs for gesture scroll */
    int16_t scroll_px;
    int16_t scroll_target;
    uint8_t scroll_row_h;
    uint8_t scroll_visible;
    uint8_t scroll_max;
} ui3_state_t;

#endif
