/**
 ****************************************************************************************************
 * @file        main.c
 * @author      Mr.Mr.
 * @version     V1.0
 * @date        2023-04-23
 * @brief       触摸屏实验
 * @license     Copyright (c) 2020-2032,  
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:ST-1 STM32F407核心板
 *  
 *  
 * 公司网址:www.genbotter.com
 * 购买地址:makerbase.taobao.com
 * 
 ****************************************************************************************************
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
// #include "./BSP/LED/led.h"
#include "./BSP/KEY/key.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
//#include "./BSP/SRAM/sram.h"
#include "./BSP/TIMER/btim.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "gui_guider.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "events_init.h"
#include "../Drivers/BSP/mfrc522/door.h"
#include "../Drivers/BSP/mfrc522/MFRC522.h"
#include "../Drivers/BSP/AS608/as608.h"
#include "boot_ota_config.h"
#include "cloud_ota_service.h"
#include "cloud_legacy_adapter.h"
#include "user_auth_portable.h"
#include "ui_common_utils.h"
#include "ui_auth_input.h"
#include "ui_menu_popup_utils.h"
#include "ui_screen8_utils.h"
#include "ui_flow_panels.h"
#include "app_screen.h"
#include "ui_nav_flow.h"
LV_FONT_DECLARE(lv_font_simsun_16_cjk);

/* Cloud switch: set to 1 to re-enable cloud/OTA path. */
#define APP_CLOUD_ENABLE 1
#if (APP_CLOUD_ENABLE == 0)
#define cloud_ota_service_report_event(...) ((void)0)
#define cloud_ota_service_ingest_line(...)  ((void)0)
#define tcp_mqtt_init()                     ((void)0)
#define tcp_mqtt_while()                    ((void)0)
#define ota_firmware_get()                  ((void)0)
#endif

/* Keep display path same as old stable project first. */
#define APP_NFC_ENABLE 1
#define APP_BOOT_VTOR_RELOCATE 0

/* 默认管理员（screen_2 登录） */
#define ADMIN_DEFAULT_ACCOUNT "1"
#define ADMIN_DEFAULT_PASSWORD "1111"

#define TA_PWD_MASK_BUF 16

#define SCREEN8_N_FOCUS 6u /* 学号、密码、管理员、btn_3、btn_4(录入)、确定 */
#define MAX_USERS 32

lv_ui guider_ui;
/* 与 lv_scr_act() 解耦：切屏动画/部分状态下活动屏指针可能与 guider_ui.screen_x 不一致，按键逻辑以本状态为准 */
static app_scr_t g_app_scr = APP_SCR_HOME;
static uint8_t g_lock_btn_selected = 0;
static uint8_t g_menu_btn_selected = 0;

static uint8_t g_screen1_field_index = 0; /* 0: account ta_1, 1: password ta_2 */
static lv_obj_t *g_screen1_cursor = NULL;
static uint32_t g_cursor_last_toggle_ms = 0;
static uint8_t g_cursor_visible = 1;
static lv_obj_t *g_screen1_unlock_popup = NULL;
static lv_timer_t *g_screen1_unlock_timer = NULL;

#define SCREEN1_UNLOCK_POPUP_MS 2500u
/* 单标签 UTF-8；思源 20 的 fallback 为 SimSun，字距统一 */
#define SCREEN1_UNLOCK_LETTER_SPACE 4

static uint8_t g_screen2_field_index = 0; /* 0: account ta_2, 1: password ta_1 */
static lv_obj_t *g_screen2_cursor = NULL;
static uint32_t g_screen2_cursor_last_toggle_ms = 0;
static uint8_t g_screen2_cursor_visible = 1;
static uint8_t g_screen3_menu_index = 0; /* 0:add,1:del,2:find/update,3:show all */

static user_cred_t g_users[MAX_USERS];
static uint8_t g_user_count = 0;
static uint8_t g_default_admin_deleted = 0u;
/* 默认管理员账号(1)是否仍具有管理员身份 */
static uint8_t g_default_admin_is_admin_role = 1u;

static uint8_t g_screen7_choice_yes = 1u; /* msgbox_1 默认 YES */
static uint8_t g_screen7_msgbox_state = 0u; /* 0:none,1:confirm,2:success,3:fail */
static lv_obj_t *g_screen7_cursor = NULL;
static uint32_t g_screen7_cursor_last_ms = 0;
static uint8_t g_screen7_cursor_visible = 1;
static uint8_t g_screen7_need_init = 0u;
static lv_obj_t *g_screen7_popup = NULL;
static lv_obj_t *g_screen7_popup_btn_yes = NULL;
static lv_obj_t *g_screen7_popup_btn_no = NULL;
static uint8_t g_screen3_need_init = 0u;
static uint8_t g_screen3_pending_index = 0u;

static uint8_t g_screen4_need_init = 0u;

static uint8_t g_screen5_need_init = 0u;
static lv_obj_t *g_screen5_cursor = NULL;
static uint32_t g_screen5_cursor_last_ms = 0;
static uint8_t g_screen5_cursor_visible = 1;
static uint8_t g_screen5_found_is_admin = 0u;
static char g_screen5_found_acc[13] = {0};

/* screen_6：更改用户身份 */
static uint8_t g_screen6_focus = 0u; /* 0:账号更改, 1:密码更改, 2:身份下拉, 3:指纹更改, 4:NFC更改 */
static lv_obj_t *g_screen6_acc_val_label = NULL; /* 账号数字(冒号后显示) */
static lv_obj_t *g_screen6_pwd_val_label = NULL; /* 密码数字(冒号后显示) */

typedef enum {
    SCREEN6_DLG_NONE = 0,
    SCREEN6_DLG_EDIT_ACC,
    SCREEN6_DLG_EDIT_PWD,
    SCREEN6_DLG_RESULT_OK,
    SCREEN6_DLG_RESULT_FAIL
} screen6_dlg_t;

static screen6_dlg_t g_screen6_dlg = SCREEN6_DLG_NONE;
static uint8_t g_screen6_dlg_saved_focus = 0u;
static lv_obj_t *g_screen6_dlg_root = NULL;
static lv_obj_t *g_screen6_dlg_ta = NULL;
static lv_obj_t *g_screen6_dlg_cursor = NULL;
static uint32_t g_screen6_dlg_cursor_ms = 0;
static uint8_t g_screen6_dlg_cursor_vis = 1u;

/* 默认管理员账号/密码（可与宏初值一致，支持在 screen_6 运行时修改） */
static char g_default_admin_account[13];
static char g_default_admin_password[11];
static uint8_t g_default_admin_has_nfc = 0u;
static uint8_t g_default_admin_nfc_uid[4] = {0u, 0u, 0u, 0u};
static uint8_t g_default_admin_has_fp = 0u;
static uint16_t g_default_admin_fp_page_id_1 = 0xFFFFu;
static uint16_t g_default_admin_fp_page_id_2 = 0xFFFFu;

static uint8_t g_screen8_focus = 0;
static lv_obj_t *g_screen8_cursor = NULL;
static uint32_t g_screen8_cursor_last_ms = 0;
static uint8_t g_screen8_cursor_visible = 1;
static uint8_t g_screen8_chip_read_ok = 0;
static uint8_t g_screen8_fp_enroll_state = 0u;
static uint8_t g_screen8_fp_enroll_result = 0u;
static uint32_t g_screen8_fp_enroll_start_time = 0u;
static uint8_t g_fp_hw_inited = 0u;
static uint8_t g_fp_enroll_step = 0u; /* 0:首按手指 1:等待抬手 2:二次按手指 */
static uint16_t g_fp_enroll_page_id = 0u;
static uint16_t g_fp_enroll_page_id_2 = 0u;
static lv_obj_t *g_screen8_popup = NULL;
static uint8_t g_screen9_focus = 0u; /* 0:NFC更换, 1:NFC删除 */
static uint8_t g_screen10_focus = 0u; /* 0:指纹更改, 1:指纹删除, 2:ESC */
static lv_obj_t *g_screen9_popup = NULL;
static lv_obj_t *g_screen9_popup_btn_yes = NULL;
static lv_obj_t *g_screen9_popup_btn_no = NULL;
static uint8_t g_screen9_choice_yes = 1u;
static uint8_t g_screen9_msgbox_state = 0u; /* 0:none,1:confirm,2:success,3:fail */

// NFC录入状态变量
static uint32_t g_nfc_enroll_start_time = 0;
static uint8_t g_nfc_enroll_state = 0; // 0:空闲, 1:录入中, 2:完成
static uint8_t g_nfc_enroll_result = 0;
static volatile uint8_t g_nfc_last_detect_result = 0u;
static volatile uint8_t g_nfc_last_uid[4] = {0u, 0u, 0u, 0u};
static volatile uint8_t g_nfc_last_reset_ret = 0u;
static uint8_t g_nfc_hw_ready = 0u;
static uint32_t g_home_nfc_last_poll_ms = 0u;
static uint32_t g_home_fp_last_poll_ms = 0u;
static lv_obj_t *g_home_unlock_popup = NULL;
static lv_timer_t *g_home_unlock_timer = NULL;

// NFC录入函数前向声明
static int users_find_index_by_nfc_uid(const uint8_t uid[4]);
static void screen4_refresh_table(void);
static void screen6_update_labels_from_found(void);
static void screen6_set_focus(uint8_t focus);
static void screen6_close_dialog(void);
static void screen8_show_nfc_enroll_popup(void);
static void screen8_handle_nfc_enroll(void);
static void screen8_show_fp_enroll_popup(void);
static void screen8_handle_fp_enroll(void);
static void screen10_set_focus(uint8_t focus);
static void enter_screen_10(void);
static void nfc_hw_wakeup_no_reset(void);

typedef enum {
    NFC_OP_NONE = 0,
    NFC_OP_ENROLL_SCREEN8,
    NFC_OP_REPLACE_SCREEN9
} nfc_op_t;

static int g_nfc_op = NFC_OP_NONE;
static uint8_t g_nfc_pending_uid_valid = 0u;
static uint8_t g_nfc_pending_uid[4] = {0u, 0u, 0u, 0u};
static uint8_t g_fp_pending_page_valid = 0u;
static uint16_t g_fp_pending_page_id = 0xFFFFu;
static uint8_t g_fp_pending_page2_valid = 0u;
static uint16_t g_fp_pending_page_id_2 = 0xFFFFu;
static lv_timer_t *g_screen8_result_timer = NULL;

static void home_unlock_popup_close(void)
{
    if(g_home_unlock_timer != NULL) {
        lv_timer_del(g_home_unlock_timer);
        g_home_unlock_timer = NULL;
    }
    if(g_home_unlock_popup != NULL && lv_obj_is_valid(g_home_unlock_popup)) {
        lv_obj_del(g_home_unlock_popup);
    }
    g_home_unlock_popup = NULL;
}

static uint8_t fp_hw_try_handshake(void)
{
    uint32_t addr = 0u;
    uint8_t hs_try;
    if(!g_fp_hw_inited) {
        GZ_StaGPIO_Init();
        g_fp_hw_inited = 1u;
    }
    for(hs_try = 0u; hs_try < 3u; hs_try++) {
        as608_rx_buffer_clear();
        if(GZ_HandShake(&addr) == 0u) {
            AS608Addr = addr;
            return 1u;
        }
        HAL_Delay(40);
    }
    return 0u;
}

static uint8_t fp_delete_template_by_page(uint16_t page_id)
{
    uint8_t en;
    if(!fp_hw_try_handshake()) return 0u;
    en = GZ_DeletChar(page_id, 1u);
    return (en == 0x00u) ? 1u : 0u;
}

static void home_unlock_timer_cb(lv_timer_t *t)
{
    (void)t;
    g_home_unlock_timer = NULL;
    home_unlock_popup_close();
}

static void home_show_unlock_popup(void)
{
    lv_obj_t *mask;
    lv_obj_t *panel;
    lv_obj_t *lbl;

    if(!lv_obj_is_valid(guider_ui.screen)) return;
    home_unlock_popup_close();

    mask = lv_obj_create(guider_ui.screen);
    g_home_unlock_popup = mask;
    lv_obj_set_size(mask, 240, 320);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_set_scrollbar_mode(mask, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(mask, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(mask, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(mask, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    panel = lv_obj_create(mask);
    lv_obj_set_size(panel, 180, 100);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl = lv_label_create(panel);
    lv_label_set_text(lbl, "\xE5\xBC\x80\xE9\x94\x81\xE6\x88\x90\xE5\x8A\x9F");
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl, 160);
    lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x0D3055), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(lbl, SCREEN1_UNLOCK_LETTER_SPACE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(lbl);
    lv_obj_move_foreground(mask);

    g_home_unlock_timer = lv_timer_create(home_unlock_timer_cb, SCREEN1_UNLOCK_POPUP_MS, NULL);
    if(g_home_unlock_timer != NULL) {
        lv_timer_set_repeat_count(g_home_unlock_timer, 1);
    }
}

static void home_nfc_poll_handle(void)
{
#if (APP_NFC_ENABLE == 0)
    return;
#else
    uint8_t uid[4];
    uint32_t now = HAL_GetTick();
    uint8_t matched = 0u;
    const char *unlock_acc = "";
    int idx;

    if(g_app_scr != APP_SCR_HOME) return;
    if(lv_scr_act() != guider_ui.screen) return;
    if(g_home_unlock_popup != NULL && lv_obj_is_valid(g_home_unlock_popup)) return;
    if((now - g_home_nfc_last_poll_ms) < 200u) return;
    g_home_nfc_last_poll_ms = now;

    if(!g_nfc_hw_ready) {
        nfc_hw_wakeup_no_reset();
    }

    if(NFC_Read_UID(uid) == 1u) {
        idx = users_find_index_by_nfc_uid(uid);
        if(idx >= 0) {
            matched = 1u;
            unlock_acc = g_users[idx].acc;
        } else if(!g_default_admin_deleted && g_default_admin_has_nfc &&
                  memcmp(g_default_admin_nfc_uid, uid, 4) == 0) {
            matched = 1u;
            unlock_acc = g_default_admin_account;
        }
        if(matched) {
            home_show_unlock_popup();
            cloud_ota_service_report_event(CLOUD_EVT_UNLOCK_OK, unlock_acc);
            cloud_ota_service_report_unlock_record(unlock_acc, "nfc", HAL_GetTick());
        }
    }
#endif
}

static uint8_t users_match_fp_page(uint16_t page_id)
{
    return user_auth_match_fp_page(g_users, g_user_count,
                                   page_id,
                                   g_default_admin_deleted,
                                   g_default_admin_has_fp,
                                   g_default_admin_fp_page_id_1,
                                   g_default_admin_fp_page_id_2);
}

static const char *users_get_acc_by_fp_page(uint16_t page_id)
{
    return user_auth_get_acc_by_fp_page(g_users, g_user_count,
                                        page_id,
                                        g_default_admin_deleted,
                                        g_default_admin_has_fp,
                                        g_default_admin_fp_page_id_1,
                                        g_default_admin_fp_page_id_2,
                                        g_default_admin_account);
}

static void home_fp_poll_handle(void)
{
    uint32_t now = HAL_GetTick();
    static uint32_t s_last_hs_try_ms = 0u;
    static uint32_t s_retry_after_ms = 0u;
    uint8_t try_i;
    uint8_t en;
    SearchResult search = {0u, 0u};

    if(g_app_scr != APP_SCR_HOME) return;
    if(lv_scr_act() != guider_ui.screen) return;
    if(g_home_unlock_popup != NULL && lv_obj_is_valid(g_home_unlock_popup)) return;
    if((int32_t)(now - s_retry_after_ms) < 0) return;
    if((now - g_home_fp_last_poll_ms) < 200u) return;
    g_home_fp_last_poll_ms = now;

    /* 仅在手指触发脚有效时才发 AS608 指令，避免空闲时阻塞主循环。 */
    if(GZ_Sta != GPIO_PIN_SET) return;

    /* 握手失败会阻塞较久，失败后做短退避，防止按键“卡死感”。 */
    if((now - s_last_hs_try_ms) >= 2000u) {
        if(!fp_hw_try_handshake()) {
            s_retry_after_ms = now + 200u;
            s_last_hs_try_ms = now;
            return;
        }
        s_last_hs_try_ms = now;
    }

    en = GZ_GetImage();
    if(en == 0x02u) return; /* no finger */
    if(en == 0xFFu) {
        s_retry_after_ms = now + 200u;
        return;
    }
    if(en != 0x00u) return;

    /* 首次特征提取失败时再快速补试1次，提升手指边缘/大拇指场景成功率。 */
    en = 0xFFu;
    for(try_i = 0u; try_i < 2u; try_i++) {
        en = GZ_GenChar(CharBuffer1);
        if(en == 0x00u) break;
        HAL_Delay(20);
    }
    if(en == 0xFFu) {
        s_retry_after_ms = now + 200u;
        return;
    }
    if(en != 0x00u) return;

    /* 搜索偶发通信/图像抖动时再补试1次。 */
    en = 0xFFu;
    for(try_i = 0u; try_i < 2u; try_i++) {
        en = GZ_Search(CharBuffer1, 0u, 300u, &search);
        if(en == 0x00u) break;
        HAL_Delay(20);
    }
    if(en == 0xFFu) {
        s_retry_after_ms = now + 200u;
        return;
    }
    if(en != 0x00u) return;

    if(users_match_fp_page(search.pageID)) {
        home_show_unlock_popup();
        cloud_ota_service_report_event(CLOUD_EVT_UNLOCK_OK, users_get_acc_by_fp_page(search.pageID));
        cloud_ota_service_report_unlock_record(users_get_acc_by_fp_page(search.pageID), "fingerprint", HAL_GetTick());
        return;
    }

    /* 双检索：同一幅图再提取到 B2 并补搜一次，提升边缘按压场景命中率。 */
    en = GZ_GenChar(CharBuffer2);
    if(en != 0x00u) return;
    en = GZ_Search(CharBuffer2, 0u, 300u, &search);
    if(en == 0x00u && users_match_fp_page(search.pageID)) {
        home_show_unlock_popup();
        cloud_ota_service_report_event(CLOUD_EVT_UNLOCK_OK, users_get_acc_by_fp_page(search.pageID));
        cloud_ota_service_report_unlock_record(users_get_acc_by_fp_page(search.pageID), "fingerprint", HAL_GetTick());
    }
}

static void nfc_hw_wakeup_no_reset(void)
{
    /* reset 在当前工程路径上可能阻塞，这里只做轻量唤醒 */
    MFRC522_Init();
    delay_ms(2);
    MFRC522_AntennaOn();
    Flash_ReadID();
    g_nfc_hw_ready = 1u;
}

static void nfc_hw_init_once(void)
{
    uint8_t i;
    char ret = (char)(-1);

    MFRC522_Init();
    for(i = 0u; i < 3u; i++) {
        ret = MFRC522_Reset();
        if(ret == MI_OK) {
            break;
        }
        delay_ms(10);
    }

    g_nfc_last_reset_ret = (uint8_t)ret;
    if(ret == MI_OK) {
        MFRC522_AntennaOn();
        Flash_ReadID();
        g_nfc_hw_ready = 1u;
    } else {
        g_nfc_hw_ready = 0u;
    }
}

static int key_to_digit(KeyValue_t key)
{
    return ui_key_to_digit(key);
}

static void lock_btn_set_selected(uint8_t selected)
{
    ui_lock_btn_set_selected(guider_ui.screen_btn_2, guider_ui.screen_btn_3,
                             &g_lock_btn_selected, &g_menu_btn_selected, selected);
}

static void menu_btn_set_selected(uint8_t selected)
{
    ui_menu_btn_set_selected(guider_ui.screen_btn_2, guider_ui.screen_btn_3,
                             &g_lock_btn_selected, &g_menu_btn_selected, selected);
}

static lv_obj_t *screen1_get_field_by_index(uint8_t idx)
{
    return ui_auth_get_field_by_index(guider_ui.screen_1_ta_1, guider_ui.screen_1_ta_2, idx);
}

static uint16_t screen1_get_field_max_len(uint8_t idx)
{
    return ui_auth_get_field_max_len(idx); /* account 1-12, password 4-10 */
}

static void textarea_measure_text_run_width(lv_obj_t *ta, lv_coord_t *txt_w)
{
    ui_textarea_measure_text_run_width(ta, txt_w, TA_PWD_MASK_BUF);
}

static void screen2_hide_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_2_label_3, 1u);
}

static void screen2_show_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_2_label_3, 0u);
}

static void screen1_hide_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_1_label_3, 1u);
}

static void screen1_show_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_1_label_3, 0u);
}

static void screen1_cancel_unlock_popup(void)
{
    if(g_screen1_unlock_timer != NULL) {
        lv_timer_del(g_screen1_unlock_timer);
        g_screen1_unlock_timer = NULL;
    }
    if(g_screen1_unlock_popup != NULL && lv_obj_is_valid(g_screen1_unlock_popup)) {
        lv_obj_del(g_screen1_unlock_popup);
    }
    g_screen1_unlock_popup = NULL;
}

static void screen1_return_home_after_unlock(void)
{
    if(g_screen1_unlock_popup != NULL && lv_obj_is_valid(g_screen1_unlock_popup)) {
        lv_obj_del(g_screen1_unlock_popup);
        g_screen1_unlock_popup = NULL;
    }
    ui_load_scr_animation(&guider_ui, &guider_ui.screen, guider_ui.screen_del, &guider_ui.screen_1_del,
                          setup_scr_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_HOME;
    g_screen1_field_index = 0;
    g_cursor_visible = 1;
    g_screen1_cursor = NULL;
    lock_btn_set_selected(0);
    menu_btn_set_selected(0);
}

static void screen1_unlock_timer_cb(lv_timer_t *t)
{
    (void)t;
    g_screen1_unlock_timer = NULL;
    screen1_return_home_after_unlock();
}

static void screen1_show_unlock_popup(void)
{
    lv_obj_t *mask;
    lv_obj_t *panel;
    lv_obj_t *lbl;

    if(!lv_obj_is_valid(guider_ui.screen_1)) return;

    screen1_cancel_unlock_popup();

    mask = lv_obj_create(guider_ui.screen_1);
    g_screen1_unlock_popup = mask;
    lv_obj_set_size(mask, 240, 320);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_set_scrollbar_mode(mask, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(mask, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(mask, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(mask, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    panel = lv_obj_create(mask);
    lv_obj_set_size(panel, 180, 100);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl = lv_label_create(panel);
    /* 显式 UTF-8（开锁成功），避免 Keil 源文件编码差异影响中文显示 */
    lv_label_set_text(lbl, "\xE5\xBC\x80\xE9\x94\x81\xE6\x88\x90\xE5\x8A\x9F");
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl, 160);
    lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x0D3055), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(lbl, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(lbl, SCREEN1_UNLOCK_LETTER_SPACE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(lbl);

    lv_obj_move_foreground(mask);
    g_screen1_unlock_timer = lv_timer_create(screen1_unlock_timer_cb, SCREEN1_UNLOCK_POPUP_MS, NULL);
    if(g_screen1_unlock_timer != NULL) {
        lv_timer_set_repeat_count(g_screen1_unlock_timer, 1);
    }
}

static int users_find_index_by_acc(const char *acc)
{
    return user_auth_find_index_by_acc(g_users, g_user_count, acc);
}

static int users_find_index_by_nfc_uid(const uint8_t uid[4])
{
    return user_auth_find_index_by_nfc_uid(g_users, g_user_count, uid);
}

static bool users_bind_nfc_by_acc(const char *acc, const uint8_t uid[4])
{
    user_auth_admin_t admin = {0};
    admin.deleted = g_default_admin_deleted;
    admin.is_admin_role = g_default_admin_is_admin_role;
    strncpy(admin.account, g_default_admin_account, sizeof(admin.account) - 1u);
    strncpy(admin.password, g_default_admin_password, sizeof(admin.password) - 1u);
    admin.has_nfc = g_default_admin_has_nfc;
    memcpy(admin.nfc_uid, g_default_admin_nfc_uid, sizeof(admin.nfc_uid));
    admin.has_fp = g_default_admin_has_fp;
    admin.fp_page_id_1 = g_default_admin_fp_page_id_1;
    admin.fp_page_id_2 = g_default_admin_fp_page_id_2;
    if(!user_auth_bind_nfc_by_acc(g_users, g_user_count, &admin, acc, uid)) return false;
    g_default_admin_has_nfc = admin.has_nfc;
    memcpy(g_default_admin_nfc_uid, admin.nfc_uid, sizeof(g_default_admin_nfc_uid));
    return true;
}

static bool users_fp_page_owned_by_other(const char *acc, uint16_t page_id)
{
    uint8_t i;
    if(!g_default_admin_deleted && g_default_admin_has_fp &&
       strcmp(acc, g_default_admin_account) != 0 &&
       (g_default_admin_fp_page_id_1 == page_id || g_default_admin_fp_page_id_2 == page_id)) {
        return true;
    }
    for(i = 0u; i < g_user_count; i++) {
        if(!g_users[i].active || !g_users[i].has_fp) continue;
        if(strcmp(g_users[i].acc, acc) == 0) continue;
        if(g_users[i].fp_page_id_1 == page_id || g_users[i].fp_page_id_2 == page_id) return true;
    }
    return false;
}

static bool users_bind_fp_by_acc(const char *acc, uint16_t page_id)
{
    user_auth_admin_t admin = {0};
    admin.deleted = g_default_admin_deleted;
    admin.is_admin_role = g_default_admin_is_admin_role;
    strncpy(admin.account, g_default_admin_account, sizeof(admin.account) - 1u);
    strncpy(admin.password, g_default_admin_password, sizeof(admin.password) - 1u);
    admin.has_nfc = g_default_admin_has_nfc;
    memcpy(admin.nfc_uid, g_default_admin_nfc_uid, sizeof(admin.nfc_uid));
    admin.has_fp = g_default_admin_has_fp;
    admin.fp_page_id_1 = g_default_admin_fp_page_id_1;
    admin.fp_page_id_2 = g_default_admin_fp_page_id_2;
    if(!user_auth_bind_fp_by_acc(g_users, g_user_count, &admin, acc, page_id)) return false;
    g_default_admin_has_fp = admin.has_fp;
    g_default_admin_fp_page_id_1 = admin.fp_page_id_1;
    g_default_admin_fp_page_id_2 = admin.fp_page_id_2;
    return true;
}

static uint8_t users_has_fp_by_acc(const char *acc, uint16_t *page_id_out)
{
    user_auth_admin_t admin = {0};
    admin.deleted = g_default_admin_deleted;
    admin.is_admin_role = g_default_admin_is_admin_role;
    strncpy(admin.account, g_default_admin_account, sizeof(admin.account) - 1u);
    strncpy(admin.password, g_default_admin_password, sizeof(admin.password) - 1u);
    admin.has_nfc = g_default_admin_has_nfc;
    memcpy(admin.nfc_uid, g_default_admin_nfc_uid, sizeof(admin.nfc_uid));
    admin.has_fp = g_default_admin_has_fp;
    admin.fp_page_id_1 = g_default_admin_fp_page_id_1;
    admin.fp_page_id_2 = g_default_admin_fp_page_id_2;
    return user_auth_has_fp_by_acc(g_users, g_user_count, &admin, acc, page_id_out);
}

static void users_clear_fp_by_acc(const char *acc, uint8_t clear_hw)
{
    user_auth_admin_t admin = {0};
    admin.deleted = g_default_admin_deleted;
    admin.is_admin_role = g_default_admin_is_admin_role;
    strncpy(admin.account, g_default_admin_account, sizeof(admin.account) - 1u);
    strncpy(admin.password, g_default_admin_password, sizeof(admin.password) - 1u);
    admin.has_nfc = g_default_admin_has_nfc;
    memcpy(admin.nfc_uid, g_default_admin_nfc_uid, sizeof(admin.nfc_uid));
    admin.has_fp = g_default_admin_has_fp;
    admin.fp_page_id_1 = g_default_admin_fp_page_id_1;
    admin.fp_page_id_2 = g_default_admin_fp_page_id_2;
    user_auth_clear_fp_by_acc(g_users, g_user_count, &admin, acc, clear_hw, fp_delete_template_by_page);
    g_default_admin_has_fp = admin.has_fp;
    g_default_admin_fp_page_id_1 = admin.fp_page_id_1;
    g_default_admin_fp_page_id_2 = admin.fp_page_id_2;
}

static void users_clear_nfc_by_acc(const char *acc)
{
    user_auth_admin_t admin = {0};
    admin.deleted = g_default_admin_deleted;
    admin.is_admin_role = g_default_admin_is_admin_role;
    strncpy(admin.account, g_default_admin_account, sizeof(admin.account) - 1u);
    strncpy(admin.password, g_default_admin_password, sizeof(admin.password) - 1u);
    admin.has_nfc = g_default_admin_has_nfc;
    memcpy(admin.nfc_uid, g_default_admin_nfc_uid, sizeof(admin.nfc_uid));
    admin.has_fp = g_default_admin_has_fp;
    admin.fp_page_id_1 = g_default_admin_fp_page_id_1;
    admin.fp_page_id_2 = g_default_admin_fp_page_id_2;
    user_auth_clear_nfc_by_acc(g_users, g_user_count, &admin, acc);
    g_default_admin_has_nfc = admin.has_nfc;
    memcpy(g_default_admin_nfc_uid, admin.nfc_uid, sizeof(g_default_admin_nfc_uid));
}

static bool admin_credentials_match(const char *acc, const char *pwd)
{
    return user_auth_admin_credentials_match(g_users, g_user_count,
                                             g_default_admin_account,
                                             g_default_admin_password,
                                             g_default_admin_is_admin_role,
                                             acc, pwd);
}

static bool unlock_credentials_match(const char *acc, const char *pwd)
{
    return user_auth_unlock_credentials_match(g_users, g_user_count,
                                              g_default_admin_account,
                                              g_default_admin_password,
                                              acc, pwd);
}

static bool users_try_register(const char *acc, const char *pwd, bool is_admin)
{
    if(g_user_count >= MAX_USERS) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_ADD_FAIL, acc);
        return false;
    }
    if(users_find_index_by_acc(acc) >= 0) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_ADD_FAIL, acc);
        return false;
    }
    if(!g_default_admin_deleted && strcmp(acc, g_default_admin_account) == 0) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_ADD_FAIL, acc);
        return false;
    }
    strncpy(g_users[g_user_count].acc, acc, sizeof(g_users[0].acc) - 1);
    g_users[g_user_count].acc[sizeof(g_users[0].acc) - 1] = '\0';
    strncpy(g_users[g_user_count].pwd, pwd, sizeof(g_users[0].pwd) - 1);
    g_users[g_user_count].pwd[sizeof(g_users[0].pwd) - 1] = '\0';
    g_users[g_user_count].is_admin = is_admin ? 1u : 0u;
    g_users[g_user_count].active = 1u;
    g_users[g_user_count].has_nfc = 0u;
    memset(g_users[g_user_count].nfc_uid, 0, sizeof(g_users[g_user_count].nfc_uid));
    g_users[g_user_count].has_fp = 0u;
    g_users[g_user_count].fp_page_id_1 = 0xFFFFu;
    g_users[g_user_count].fp_page_id_2 = 0xFFFFu;
    g_user_count++;
    cloud_ota_service_report_event(CLOUD_EVT_USER_ADD_OK, acc);
    return true;
}

static bool users_try_delete_by_acc(const char *acc)
{
    int idx = users_find_index_by_acc(acc);
    if(idx < 0) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_DELETE_FAIL, acc);
        return false;
    }
    g_users[idx].active = 0u;
    g_users[idx].has_nfc = 0u;
    memset(g_users[idx].nfc_uid, 0, sizeof(g_users[idx].nfc_uid));
    if(g_users[idx].has_fp) {
        if(g_users[idx].fp_page_id_1 != 0xFFFFu) {
            (void)fp_delete_template_by_page(g_users[idx].fp_page_id_1);
        }
        if(g_users[idx].fp_page_id_2 != 0xFFFFu &&
           g_users[idx].fp_page_id_2 != g_users[idx].fp_page_id_1) {
            (void)fp_delete_template_by_page(g_users[idx].fp_page_id_2);
        }
    }
    g_users[idx].has_fp = 0u;
    g_users[idx].fp_page_id_1 = 0xFFFFu;
    g_users[idx].fp_page_id_2 = 0xFFFFu;
    cloud_ota_service_report_event(CLOUD_EVT_USER_DELETE_OK, acc);
    return true;
}

static bool default_admin_deleted(void)
{
    return g_default_admin_deleted ? true : false;
}

static bool admin_credentials_match_with_delete(const char *acc, const char *pwd)
{
    /* 默认管理员 "1/1111" 不在 g_users 表里，所以这里单独处理删除状态 */
    if(strcmp(acc, g_default_admin_account) == 0 && default_admin_deleted()) {
        return false;
    }
    return admin_credentials_match(acc, pwd);
}

static bool unlock_credentials_match_with_delete(const char *acc, const char *pwd)
{
    /* 默认管理员 "1/1111" 不在 g_users 表里，所以这里单独处理删除状态 */
    if(strcmp(acc, g_default_admin_account) == 0 && default_admin_deleted()) {
        return false;
    }
    return unlock_credentials_match(acc, pwd);
}

static void screen1_update_cursor_pos(void)
{
    lv_obj_t *ta = screen1_get_field_by_index(g_screen1_field_index);
    ui_auth_update_cursor_pos(g_screen1_cursor, ta);
}

static void screen1_handle_input_key(KeyValue_t key)
{
    lv_obj_t *ta = screen1_get_field_by_index(g_screen1_field_index);
    uint16_t max_len = screen1_get_field_max_len(g_screen1_field_index);
    ui_auth_handle_input_key(ta, key, max_len, screen1_hide_error_label, screen1_update_cursor_pos);
}

static void screen1_set_field_selected(uint8_t idx)
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

static void screen1_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_1) return;
    ui_cursor_blink_step(g_screen1_cursor, &g_cursor_visible, &g_cursor_last_toggle_ms, 500u);
}

/* screen_2：布局与 screen_1 一致逻辑；ta_2=上方账号，ta_1=下方密码 */
static lv_obj_t *screen2_get_field_by_index(uint8_t idx)
{
    return ui_auth_get_field_by_index(guider_ui.screen_2_ta_2, guider_ui.screen_2_ta_1, idx);
}

static uint16_t screen2_get_field_max_len(uint8_t idx)
{
    return ui_auth_get_field_max_len(idx);
}

static void screen2_update_cursor_pos(void)
{
    lv_obj_t *ta = screen2_get_field_by_index(g_screen2_field_index);
    ui_auth_update_cursor_pos(g_screen2_cursor, ta);
}

static void screen2_handle_input_key(KeyValue_t key)
{
    lv_obj_t *ta = screen2_get_field_by_index(g_screen2_field_index);
    uint16_t max_len = screen2_get_field_max_len(g_screen2_field_index);
    ui_auth_handle_input_key(ta, key, max_len, screen2_hide_error_label, screen2_update_cursor_pos);
}

static void screen2_set_field_selected(uint8_t idx)
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

static void screen2_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_2) return;
    ui_cursor_blink_step(g_screen2_cursor, &g_screen2_cursor_visible, &g_screen2_cursor_last_toggle_ms, 500u);
}

static lv_obj_t *screen3_get_menu_btn_by_index(uint8_t idx)
{
    return ui_menu_btn_by_index(guider_ui.screen_3_btn_3, guider_ui.screen_3_btn_4,
                                guider_ui.screen_3_btn_5, guider_ui.screen_3_btn_6, idx);
}

static lv_obj_t *screen3_get_menu_label_by_index(uint8_t idx)
{
    return ui_menu_label_by_index(guider_ui.screen_3_btn_3_label, guider_ui.screen_3_btn_4_label,
                                  guider_ui.screen_3_btn_5_label, guider_ui.screen_3_btn_6_label, idx);
}

static void screen3_set_menu_selected(uint8_t idx)
{
    ui_screen3_set_menu_selected(idx, &g_screen3_menu_index,
                                 guider_ui.screen_3_btn_3, guider_ui.screen_3_btn_4,
                                 guider_ui.screen_3_btn_5, guider_ui.screen_3_btn_6,
                                 guider_ui.screen_3_btn_3_label, guider_ui.screen_3_btn_4_label,
                                 guider_ui.screen_3_btn_5_label, guider_ui.screen_3_btn_6_label);
}

static void screen7_hide_all_msgbox(void)
{
    ui_common_hide_msgbox(&g_screen7_popup, &g_screen7_popup_btn_yes, &g_screen7_popup_btn_no, &g_screen7_msgbox_state);
}

static void screen7_popup_btn_style(lv_obj_t *btn, lv_obj_t *lbl, uint8_t selected)
{
    ui_popup_btn_style(btn, lbl, selected);
}

static void screen7_set_msgbox1_choice(uint8_t yes_selected)
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

static void screen7_show_msgbox1(void)
{
    screen7_show_simple_popup("确定删除用户", 1u, 1u);
}

static void screen7_show_msgbox2(void)
{
    screen7_show_simple_popup("删除成功", 2u, 0u);
}

static void screen7_show_msgbox3(void)
{
    screen7_show_simple_popup("删除失败", 3u, 0u);
}

static void screen7_update_cursor_pos(void)
{
    lv_obj_t *ta = guider_ui.screen_7_ta_1;
    ui_auth_update_cursor_pos(g_screen7_cursor, ta);
}

static void screen7_set_field_selected(void)
{
    lv_obj_t *ta = guider_ui.screen_7_ta_1;
    if(!lv_obj_is_valid(ta)) return;
    ui_textarea_apply_input_style(ta, 1u);
    ui_cursor_attach_bar(&g_screen7_cursor, ta);
    screen7_update_cursor_pos();
    ui_cursor_activate(g_screen7_cursor, &g_screen7_cursor_visible, &g_screen7_cursor_last_ms);
}

static void screen7_handle_input_key(KeyValue_t key)
{
    lv_obj_t *ta = guider_ui.screen_7_ta_1;
    ui_auth_handle_input_key(ta, key, 12u, NULL, screen7_update_cursor_pos);
}

static void screen5_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_5) return;
    ui_cursor_blink_step(g_screen5_cursor, &g_screen5_cursor_visible, &g_screen5_cursor_last_ms, 500u);
}

static void screen7_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_7 || g_screen7_msgbox_state != 0u) return;
    ui_cursor_blink_step(g_screen7_cursor, &g_screen7_cursor_visible, &g_screen7_cursor_last_ms, 500u);
}

static lv_obj_t *screen8_focus_get_ta(void)
{
    return ui_screen8_focus_get_ta(g_screen8_focus, guider_ui.screen_8_ta_1, guider_ui.screen_8_ta_2);
}

static void screen8_hide_status_labels(void)
{
    ui_screen8_hide_status_labels(guider_ui.screen_8_label_5, guider_ui.screen_8_label_6);
}

static void screen8_clear_error_if_any(void)
{
    ui_screen8_clear_error_label(guider_ui.screen_8_label_5);
}

static void screen8_style_ta_idle(lv_obj_t *ta)
{
    ui_screen8_style_ta(ta, 0u);
}

static void screen8_style_ta_selected(lv_obj_t *ta)
{
    ui_screen8_style_ta(ta, 1u);
}

static void screen8_style_btn_idle(lv_obj_t *btn, lv_obj_t *lbl)
{
    ui_screen8_style_btn(btn, lbl, 0u);
}

static void screen8_style_btn_selected(lv_obj_t *btn, lv_obj_t *lbl)
{
    ui_screen8_style_btn(btn, lbl, 1u);
}

static void screen8_update_cursor_pos(void)
{
    lv_obj_t *ta = screen8_focus_get_ta();
    ui_auth_update_cursor_pos(g_screen8_cursor, ta);
}

static void screen8_set_focus(uint8_t idx)
{
    lv_obj_t *ta1 = guider_ui.screen_8_ta_1;
    lv_obj_t *ta2 = guider_ui.screen_8_ta_2;
    lv_obj_t *cb = guider_ui.screen_8_cb_1;
    lv_obj_t *b_fp_1 = guider_ui.screen_8_btn_3;
    lv_obj_t *b_fp_1_l = guider_ui.screen_8_btn_3_label;
    lv_obj_t *b_fp_2 = guider_ui.screen_8_btn_4;
    lv_obj_t *b_fp_2_l = guider_ui.screen_8_btn_4_label;
    lv_obj_t *b_ok = guider_ui.screen_8_btn_1;
    lv_obj_t *b_ok_l = guider_ui.screen_8_btn_1_label;

    if(idx >= SCREEN8_N_FOCUS) idx = 0u;
    g_screen8_focus = idx;
    screen8_clear_error_if_any();

    screen8_style_ta_idle(ta1);
    screen8_style_ta_idle(ta2);
    screen8_style_btn_idle(b_fp_1, b_fp_1_l);
    screen8_style_btn_idle(b_fp_2, b_fp_2_l);
    screen8_style_btn_idle(b_ok, b_ok_l);
    if(lv_obj_is_valid(cb)) {
        lv_obj_set_style_outline_width(cb, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    switch(g_screen8_focus) {
        case 0:
            screen8_style_ta_selected(ta1);
            ui_cursor_attach_bar(&g_screen8_cursor, ta1);
            screen8_update_cursor_pos();
            ui_cursor_activate(g_screen8_cursor, &g_screen8_cursor_visible, &g_screen8_cursor_last_ms);
            break;
        case 1:
            screen8_style_ta_selected(ta2);
            ui_cursor_attach_bar(&g_screen8_cursor, ta2);
            screen8_update_cursor_pos();
            ui_cursor_activate(g_screen8_cursor, &g_screen8_cursor_visible, &g_screen8_cursor_last_ms);
            break;
        case 2:
            if(lv_obj_is_valid(cb)) {
                lv_obj_set_style_outline_width(cb, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_outline_color(cb, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_outline_opa(cb, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_outline_pad(cb, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            if(g_screen8_cursor != NULL && lv_obj_is_valid(g_screen8_cursor)) {
                lv_obj_add_flag(g_screen8_cursor, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case 3:
            screen8_style_btn_selected(b_fp_1, b_fp_1_l);
            if(g_screen8_cursor != NULL && lv_obj_is_valid(g_screen8_cursor)) {
                lv_obj_add_flag(g_screen8_cursor, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case 4:
            screen8_style_btn_selected(b_fp_2, b_fp_2_l);
            if(g_screen8_cursor != NULL && lv_obj_is_valid(g_screen8_cursor)) {
                lv_obj_add_flag(g_screen8_cursor, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case 5:
            screen8_style_btn_selected(b_ok, b_ok_l);
            if(g_screen8_cursor != NULL && lv_obj_is_valid(g_screen8_cursor)) {
                lv_obj_add_flag(g_screen8_cursor, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        default:
            break;
    }
}

static void screen8_handle_ta_key(KeyValue_t key)
{
    lv_obj_t *ta = screen8_focus_get_ta();
    uint16_t max_len = (g_screen8_focus == 0u) ? 12u : 10u;
    ui_auth_handle_input_key(ta, key, max_len, NULL, screen8_update_cursor_pos);
}

static void screen8_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_8 || g_screen8_focus > 1u) return;
    ui_cursor_blink_step(g_screen8_cursor, &g_screen8_cursor_visible, &g_screen8_cursor_last_ms, 500u);
}

/* 预留芯片读取接口：后续可替换为实际硬件读取流程 */
static bool screen8_try_read_chip(void)
{
    return true;
}

static void screen8_popup_close_and_back(void)
{
    if(g_screen8_result_timer != NULL) {
        lv_timer_del(g_screen8_result_timer);
        g_screen8_result_timer = NULL;
    }
    if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
        lv_obj_del(g_screen8_popup);
    }
    g_screen8_popup = NULL;
    ui_load_scr_animation(&guider_ui, &guider_ui.screen_3, guider_ui.screen_3_del, &guider_ui.screen_8_del,
                          setup_scr_screen_3, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_3;
    g_screen8_cursor = NULL;
    screen3_set_menu_selected(0);
}

static void screen8_popup_close_and_back_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    screen8_popup_close_and_back();
}

static void screen8_popup_close_only(void)
{
    if(g_screen8_result_timer != NULL) {
        lv_timer_del(g_screen8_result_timer);
        g_screen8_result_timer = NULL;
    }
    if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
        lv_obj_del(g_screen8_popup);
    }
    g_screen8_popup = NULL;
}

static void screen8_popup_close_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        screen8_popup_close_and_back();
    }
}

// 通用录入弹窗OK按钮回调
static void screen8_enroll_popup_ok_cb(lv_event_t *e)
{
    (void)e;
    if(g_screen8_popup) {
        lv_obj_del(g_screen8_popup);
        g_screen8_popup = NULL;
    }
    if(g_nfc_enroll_state == 2u) {
        g_nfc_enroll_state = 0u;
    }
    if(g_screen8_fp_enroll_state == 2u) {
        g_screen8_fp_enroll_state = 0u;
    }
}

// 显示通用录入弹窗
static void screen8_show_enroll_popup(const char *message)
{
    bool is_progress = false;

    if(g_screen8_popup) {
        lv_obj_del(g_screen8_popup);
        g_screen8_popup = NULL;
    }

    g_screen8_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_screen8_popup, 200, 120);
    lv_obj_align(g_screen8_popup, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(g_screen8_popup, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(g_screen8_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(g_screen8_popup, LV_OPA_90, 0);
    lv_obj_set_style_bg_color(g_screen8_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_radius(g_screen8_popup, 10, 0);
    lv_obj_set_style_border_width(g_screen8_popup, 0, 0);

    lv_obj_t *label = lv_label_create(g_screen8_popup);
    lv_label_set_text(label, message);
    lv_obj_set_width(label, 180);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label, &lv_font_SourceHanSerifSC_Regular_20, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *btn = lv_btn_create(g_screen8_popup);
    lv_obj_set_size(btn, 80, 30);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *btn_label = lv_label_create(btn);
    if(message != NULL && (strstr(message, "Working") != NULL || strstr(message, "Enrollment") != NULL)) {
        is_progress = true;
    }
    lv_label_set_text(btn_label, is_progress ? "ESC" : "OK");
    lv_obj_set_style_text_font(btn_label, &lv_font_SourceHanSerifSC_Regular_20, 0);
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(btn, screen8_enroll_popup_ok_cb, LV_EVENT_CLICKED, NULL);

    if(g_screen8_result_timer != NULL) {
        lv_timer_del(g_screen8_result_timer);
        g_screen8_result_timer = NULL;
    }
}

static void screen8_show_fp_enroll_popup(void)
{
    uint32_t addr = 0u;
    uint8_t hs_try;
    uint8_t hs_ok = 0u;
    uint16_t valid_n = 0u;

    if(!g_fp_hw_inited) {
        GZ_StaGPIO_Init();
        g_fp_hw_inited = 1u;
    }
    for(hs_try = 0u; hs_try < 3u; hs_try++) {
        as608_rx_buffer_clear();
        if(GZ_HandShake(&addr) == 0u) {
            AS608Addr = addr;
            hs_ok = 1u;
            break;
        }
        HAL_Delay(40);
    }
    if(hs_ok) {
    } else {
        /* Some module firmware may not respond to handshake; keep default/user-set address. */
    }
    if(GZ_ValidTempleteNum(&valid_n) == 0x00u) {
        g_fp_enroll_page_id = valid_n;
        g_fp_enroll_page_id_2 = (uint16_t)(valid_n + 1u);
    } else {
        g_fp_enroll_page_id = 0u;
        g_fp_enroll_page_id_2 = 1u;
    }
    g_fp_enroll_step = 0u;
    g_screen8_fp_enroll_start_time = HAL_GetTick();
    g_screen8_fp_enroll_state = 1u;
    g_screen8_fp_enroll_result = 0u;
    screen8_show_enroll_popup("Fingerprint Enrollment...");
}

static void screen8_handle_fp_enroll(void)
{
    uint32_t current_time;
    static uint32_t s_last_fp_log_ms = 0u;
    static uint32_t s_last_getimg_ms = 0u;
    static uint8_t s_noack_cnt = 0u;
    uint8_t en;

    if(g_screen8_fp_enroll_state != 1u) return;

    current_time = HAL_GetTick();
    if((current_time - s_last_fp_log_ms) >= 500u) {
        s_last_fp_log_ms = current_time;
    }
    if((current_time - g_screen8_fp_enroll_start_time) >= 10000u) {
        g_screen8_fp_enroll_state = 2u;
        g_screen8_fp_enroll_result = 0u;
        screen8_show_enroll_popup("Fingerprint FAIL");
        return;
    }

    /* Limit AS608 polling rate to avoid command flooding in fast GUI loops. */
    if((current_time - s_last_getimg_ms) < 120u) {
        return;
    }
    s_last_getimg_ms = current_time;

    en = GZ_GetImage();
    if(en != 0x02u) {
    }

    /* 0x02: no finger on sensor, keep waiting */
    if(en == 0x02u || en == 2u) {
        if(g_fp_enroll_step == 1u) {
            /* 已完成第一次特征提取，等待手指移开后进入第二次按压采集 */
            g_fp_enroll_step = 2u;
        }
        s_noack_cnt = 0u;
        return;
    }

    if(en == 0xFFu) {
        s_noack_cnt++;
        if(s_noack_cnt < 3u) {
            return;
        }
        g_screen8_fp_enroll_state = 2u;
        g_screen8_fp_enroll_result = 0u;
        screen8_show_enroll_popup("Fingerprint FAIL");
        return;
    }
    s_noack_cnt = 0u;

    /* 0x01: packet receive error, treat as transient and keep waiting */
    if(en == 0x01u) {
        return;
    }
    if(en != 0x00u) {
        g_screen8_fp_enroll_state = 2u;
        g_screen8_fp_enroll_result = 0u;
        screen8_show_enroll_popup("Fingerprint FAIL");
        return;
    }

    if(g_fp_enroll_step == 0u) {
        en = GZ_GenChar(CharBuffer1);
        if(en == 0x00u) {
            g_fp_enroll_step = 1u;
            return;
        }
        g_screen8_fp_enroll_state = 2u;
        g_screen8_fp_enroll_result = 0u;
        screen8_show_enroll_popup("Fingerprint FAIL");
        return;
    }

    if(g_fp_enroll_step == 1u) {
        /* 第一次采集后等待手指移开：若仍检测到手指(0x00)则继续等待，不判失败 */
        return;
    }

    if(g_fp_enroll_step == 2u) {
        en = GZ_GenChar(CharBuffer2);
        if(en != 0x00u) {
            g_screen8_fp_enroll_state = 2u;
            g_screen8_fp_enroll_result = 0u;
            screen8_show_enroll_popup("Fingerprint FAIL");
            return;
        }

        en = GZ_Match();
        if(en != 0x00u) {
            g_screen8_fp_enroll_state = 2u;
            g_screen8_fp_enroll_result = 0u;
            screen8_show_enroll_popup("Fingerprint FAIL");
            return;
        }

        en = GZ_RegModel();
        if(en != 0x00u) {
            g_screen8_fp_enroll_state = 2u;
            g_screen8_fp_enroll_result = 0u;
            screen8_show_enroll_popup("Fingerprint FAIL");
            return;
        }

        en = GZ_StoreChar(CharBuffer2, g_fp_enroll_page_id);
        if(en == 0x00u) {
            en = GZ_StoreChar(CharBuffer2, g_fp_enroll_page_id_2);
            if(en != 0x00u) {
                (void)fp_delete_template_by_page(g_fp_enroll_page_id);
            }
        }
        if(en == 0x00u) {
            if(g_app_scr == APP_SCR_10) {
                if(g_screen5_found_acc[0] != '\0' &&
                   users_bind_fp_by_acc(g_screen5_found_acc, g_fp_enroll_page_id) &&
                   users_bind_fp_by_acc(g_screen5_found_acc, g_fp_enroll_page_id_2)) {
                    g_screen8_fp_enroll_result = 1u;
                } else {
                    (void)fp_delete_template_by_page(g_fp_enroll_page_id);
                    (void)fp_delete_template_by_page(g_fp_enroll_page_id_2);
                    g_screen8_fp_enroll_result = 0u;
                }
            } else {
                if(g_fp_pending_page_valid) {
                    (void)fp_delete_template_by_page(g_fp_pending_page_id);
                }
                if(g_fp_pending_page2_valid) {
                    (void)fp_delete_template_by_page(g_fp_pending_page_id_2);
                }
                g_fp_pending_page_id = g_fp_enroll_page_id;
                g_fp_pending_page_valid = 1u;
                g_fp_pending_page_id_2 = g_fp_enroll_page_id_2;
                g_fp_pending_page2_valid = 1u;
                g_screen8_chip_read_ok = 1u;
                g_screen8_fp_enroll_result = 1u;
            }
            g_screen8_fp_enroll_state = 2u;
            screen8_show_enroll_popup((g_screen8_fp_enroll_result == 1u) ? "Fingerprint OK" : "Fingerprint FAIL");
            return;
        }
    }

    g_screen8_fp_enroll_state = 2u;
    g_screen8_fp_enroll_result = 0u;
    screen8_show_enroll_popup("Fingerprint FAIL");
}

static void screen8_show_result_popup(bool ok)
{
    lv_obj_t *mask;
    lv_obj_t *panel;
    lv_obj_t *lbl;
    lv_obj_t *btn_x;
    lv_obj_t *lbl_x;
    const char *msg_txt = ok ? "增加用户成功" : "增加用户失败";

    if(!lv_obj_is_valid(guider_ui.screen_8)) return;
    if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
        lv_obj_del(g_screen8_popup);
        g_screen8_popup = NULL;
    }

    mask = lv_obj_create(guider_ui.screen_8);
    g_screen8_popup = mask;
    lv_obj_set_size(mask, 240, 320);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_set_scrollbar_mode(mask, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(mask, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(mask, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(mask, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    panel = lv_obj_create(mask);
    lv_obj_set_size(panel, 180, 100);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl = lv_label_create(panel);
    lv_label_set_text(lbl, msg_txt);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl, 150);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x0D3055), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(lbl);

    btn_x = lv_btn_create(panel);
    lv_obj_set_size(btn_x, 24, 24);
    lv_obj_align(btn_x, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_set_style_radius(btn_x, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn_x, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_x, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_x, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_x, screen8_popup_close_event_cb, LV_EVENT_CLICKED, NULL);

    lbl_x = lv_label_create(btn_x);
    lv_label_set_text(lbl_x, "X");
    lv_obj_set_style_text_font(lbl_x, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_x, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(lbl_x);

    lv_obj_move_foreground(mask);
    if(ok) {
        if(g_screen8_result_timer != NULL) {
            lv_timer_del(g_screen8_result_timer);
        }
        g_screen8_result_timer = lv_timer_create(screen8_popup_close_and_back_timer_cb, 1200, NULL);
        if(g_screen8_result_timer != NULL) {
            lv_timer_set_repeat_count(g_screen8_result_timer, 1);
        }
    }
}

static bool screen8_has_valid_acc_pwd_input(void)
{
    const char *acc;
    const char *pwd;
    size_t la;
    size_t lp;
    if(!lv_obj_is_valid(guider_ui.screen_8_ta_1) || !lv_obj_is_valid(guider_ui.screen_8_ta_2)) return false;
    acc = lv_textarea_get_text(guider_ui.screen_8_ta_1);
    pwd = lv_textarea_get_text(guider_ui.screen_8_ta_2);
    la = strlen(acc);
    lp = strlen(pwd);
    return (la >= 1u && la <= 12u && lp >= 4u && lp <= 10u) ? true : false;
}

static void screen8_attempt_commit(void)
{
    const char *acc;
    const char *pwd;
    size_t la;
    size_t lp;
    bool ok = true;
    bool is_admin;

    if(!lv_obj_is_valid(guider_ui.screen_8_ta_1) || !lv_obj_is_valid(guider_ui.screen_8_ta_2)) return;

    acc = lv_textarea_get_text(guider_ui.screen_8_ta_1);
    pwd = lv_textarea_get_text(guider_ui.screen_8_ta_2);
    la = strlen(acc);
    lp = strlen(pwd);

    if(la < 1u || la > 12u || lp < 4u || lp > 10u) ok = false;

    if(ok) {
        is_admin = lv_obj_has_state(guider_ui.screen_8_cb_1, LV_STATE_CHECKED);
        if(!users_try_register(acc, pwd, is_admin)) {
            ok = false;
        } else {
            if(g_nfc_pending_uid_valid && !users_bind_nfc_by_acc(acc, g_nfc_pending_uid)) {
                (void)users_try_delete_by_acc(acc);
                ok = false;
            }
            if(ok && g_fp_pending_page_valid) {
                if(!users_bind_fp_by_acc(acc, g_fp_pending_page_id)) {
                    (void)users_try_delete_by_acc(acc);
                    ok = false;
                }
            }
            if(ok && g_fp_pending_page2_valid) {
                if(!users_bind_fp_by_acc(acc, g_fp_pending_page_id_2)) {
                    (void)users_try_delete_by_acc(acc);
                    ok = false;
                }
            }
        }
    }
    if(ok) {
        g_nfc_pending_uid_valid = 0u;
        memset(g_nfc_pending_uid, 0, sizeof(g_nfc_pending_uid));
        g_fp_pending_page_valid = 0u;
        g_fp_pending_page_id = 0xFFFFu;
        g_fp_pending_page2_valid = 0u;
        g_fp_pending_page_id_2 = 0xFFFFu;
    }
    screen8_show_result_popup(ok);
}

static void enter_screen_8(void)
{
    ui_load_scr_animation(&guider_ui, &guider_ui.screen_8, guider_ui.screen_8_del, &guider_ui.screen_3_del,
                          setup_scr_screen_8, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_8;

    if(lv_obj_is_valid(guider_ui.screen_8_ta_1)) {
        lv_textarea_set_text(guider_ui.screen_8_ta_1, "");
        lv_textarea_set_max_length(guider_ui.screen_8_ta_1, 12);
        lv_textarea_set_password_mode(guider_ui.screen_8_ta_1, false);
    }
    if(lv_obj_is_valid(guider_ui.screen_8_ta_2)) {
        lv_textarea_set_text(guider_ui.screen_8_ta_2, "");
        lv_textarea_set_max_length(guider_ui.screen_8_ta_2, 10);
        lv_textarea_set_password_mode(guider_ui.screen_8_ta_2, false);
    }
    if(lv_obj_is_valid(guider_ui.screen_8_cb_1)) {
        lv_obj_clear_state(guider_ui.screen_8_cb_1, LV_STATE_CHECKED);
    }
    screen8_hide_status_labels();
    g_screen8_chip_read_ok = 0u;
    g_nfc_pending_uid_valid = 0u;
    memset(g_nfc_pending_uid, 0, sizeof(g_nfc_pending_uid));
    g_fp_pending_page_valid = 0u;
    g_fp_pending_page_id = 0xFFFFu;
    g_fp_pending_page2_valid = 0u;
    g_fp_pending_page_id_2 = 0xFFFFu;
    g_nfc_op = NFC_OP_NONE;
    g_screen8_fp_enroll_state = 0u;
    g_screen8_fp_enroll_result = 0u;
    g_screen8_fp_enroll_start_time = 0u;
    g_screen8_popup = NULL;
    g_screen8_cursor = NULL;
    screen8_set_focus(0);
}

static void screen9_set_focus(uint8_t focus)
{
    ui_screen9_set_focus_style(focus, &g_screen9_focus, guider_ui.screen_9_btn_5, guider_ui.screen_9_btn_6);
}

static void screen9_hide_all_msgbox(void)
{
    ui_common_hide_msgbox(&g_screen9_popup, &g_screen9_popup_btn_yes, &g_screen9_popup_btn_no, &g_screen9_msgbox_state);
}

static void screen9_popup_btn_style(lv_obj_t *btn, lv_obj_t *lbl, uint8_t selected)
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

static void screen9_set_msgbox_choice(uint8_t yes_selected)
{
    ui_common_set_yes_no_choice(g_screen9_popup_btn_yes, g_screen9_popup_btn_no, yes_selected, &g_screen9_choice_yes);
}

static void screen9_show_delete_confirm_popup(void)
{
    screen9_hide_all_msgbox();
    if(!lv_obj_is_valid(guider_ui.screen_9)) return;
    ui_common_show_yes_no_popup(guider_ui.screen_9, &g_screen9_popup, &g_screen9_popup_btn_yes, &g_screen9_popup_btn_no,
                                "\xE7\xA1\xAE\xE5\xAE\x9A\xE5\x88\xA0\xE9\x99\xA4NFC", &g_screen9_msgbox_state, 1u);
    screen9_set_msgbox_choice(1u);
}

static void screen10_show_delete_confirm_popup(void)
{
    screen9_hide_all_msgbox();
    if(!lv_obj_is_valid(guider_ui.screen_10)) return;
    ui_common_show_yes_no_popup(guider_ui.screen_10, &g_screen9_popup, &g_screen9_popup_btn_yes, &g_screen9_popup_btn_no,
                                "\xE7\xA1\xAE\xE5\xAE\x9A\xE5\x88\xA0\xE9\x99\xA4\x46\x50", &g_screen9_msgbox_state, 1u);
    screen9_set_msgbox_choice(1u);
}

static void screen9_show_delete_result_popup(uint8_t success)
{
    screen9_hide_all_msgbox();
    if(!lv_obj_is_valid(guider_ui.screen_9)) return;
    ui_common_show_result_popup(guider_ui.screen_9, &g_screen9_popup, success ? "删除成功" : "删除失败",
                                &g_screen9_msgbox_state, success ? 2u : 3u);
}

static void screen10_show_delete_result_popup(uint8_t success)
{
    screen9_hide_all_msgbox();
    if(!lv_obj_is_valid(guider_ui.screen_10)) return;
    ui_common_show_result_popup(guider_ui.screen_10, &g_screen9_popup, success ? "删除成功" : "删除失败",
                                &g_screen9_msgbox_state, success ? 2u : 3u);
}

static uint8_t screen10_try_delete_fp(void)
{
    uint16_t page = 0xFFFFu;
    if(g_screen5_found_acc[0] == '\0') return 0u;
    if(!users_has_fp_by_acc(g_screen5_found_acc, &page)) {
        return 0u;
    }
    users_clear_fp_by_acc(g_screen5_found_acc, 1u);
    return 1u;
}

static void screen9_start_nfc_replace(void)
{
    g_nfc_op = NFC_OP_REPLACE_SCREEN9;
    /* 复用screen_8录入入口，确保更改流程也执行NFC硬件唤醒与天线拉起 */
    screen8_show_nfc_enroll_popup();
}

static void enter_screen_9(void)
{
    ui_load_scr_animation(&guider_ui, &guider_ui.screen_9, guider_ui.screen_9_del, &guider_ui.screen_6_del,
                          setup_scr_screen_9, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_9;
    /* 默认选中 NFC更换 */
    screen9_set_focus(0u);
    g_screen9_popup = NULL;
    g_screen9_popup_btn_yes = NULL;
    g_screen9_popup_btn_no = NULL;
    g_screen9_choice_yes = 1u;
    g_screen9_msgbox_state = 0u;
}

static void screen10_set_focus(uint8_t focus)
{
    lv_obj_t *btn_replace = guider_ui.screen_10_btn_2;
    lv_obj_t *btn_delete = guider_ui.screen_10_btn_1;
    lv_obj_t *btn_esc = guider_ui.screen_10_btn_3;

    if(focus > 2u) focus = 0u;
    g_screen10_focus = focus;

    if(lv_obj_is_valid(btn_replace)) {
        lv_obj_set_style_bg_opa(btn_replace, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_replace, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if(lv_obj_is_valid(btn_delete)) {
        lv_obj_set_style_bg_opa(btn_delete, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if(lv_obj_is_valid(btn_esc)) {
        lv_obj_set_style_bg_opa(btn_esc, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_esc, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if(g_screen10_focus == 0u && lv_obj_is_valid(btn_replace)) {
        lv_obj_set_style_bg_opa(btn_replace, 160, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_replace, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if(g_screen10_focus == 1u && lv_obj_is_valid(btn_delete)) {
        lv_obj_set_style_bg_opa(btn_delete, 160, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if(g_screen10_focus == 2u && lv_obj_is_valid(btn_esc)) {
        lv_obj_set_style_bg_opa(btn_esc, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_esc, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void enter_screen_10(void)
{
    ui_load_scr_animation(&guider_ui, &guider_ui.screen_10, guider_ui.screen_10_del, &guider_ui.screen_6_del,
                          setup_scr_screen_10, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_10;
    g_screen10_focus = 0u;
    g_screen8_popup = NULL;
    screen10_set_focus(0u);
}

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

static void enter_screen_1(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_ops_t ops = build_nav_ops();
    ui_nav_enter_screen_1(&ctx, &ops);

    /* 默认选中账号输入框 */
    screen1_set_field_selected(0);
    screen1_hide_error_label();
    if(lv_obj_is_valid(guider_ui.screen_1_ta_2)) {
        lv_textarea_set_password_bullet(guider_ui.screen_1_ta_2, "*");
        lv_textarea_set_password_mode(guider_ui.screen_1_ta_2, true);
    }
}

static void enter_screen_2(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_ops_t ops = build_nav_ops();
    ui_nav_enter_screen_2(&ctx, &ops);

    /* 默认选中上方账号输入框 ta_2 */
    screen2_set_field_selected(0);
    screen2_hide_error_label();
    if(lv_obj_is_valid(guider_ui.screen_2_ta_1)) {
        lv_textarea_set_password_bullet(guider_ui.screen_2_ta_1, "*");
        lv_textarea_set_password_mode(guider_ui.screen_2_ta_1, true);
    }
}

static void enter_screen_3(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_ops_t ops = build_nav_ops();
    ui_nav_enter_screen_3(&ctx, &ops);
}

static void screen4_refresh_table(void)
{
    lv_obj_t *table = guider_ui.screen_4_table_1;
    if(!lv_obj_is_valid(table)) return;

    const uint16_t header_row = 0u;
    uint16_t user_cnt = 0u;
    uint16_t max_rows = lv_table_get_row_cnt(table);
    /* 第 0 行为表头，数据最多占用 max_rows - 1 行 */
    uint16_t max_data_rows = (max_rows > 1u) ? (max_rows - 1u) : 0u;
    uint16_t row;
    uint16_t placed;
    uint16_t target;
    uint8_t i;

    if(!g_default_admin_deleted) user_cnt++;
    for(i = 0u; i < g_user_count; i++) {
        if(g_users[i].active) user_cnt++;
    }

    lv_table_set_col_cnt(table, 2u);
    lv_table_set_col_width(table, 0u, 120);
    lv_table_set_col_width(table, 1u, 120);

    lv_table_set_cell_value(table, header_row, 0u, "账号");
    lv_table_set_cell_value(table, header_row, 1u, "身份");

    if(max_data_rows == 0u) return;

    if(user_cnt == 0u) {
        lv_table_set_cell_value(table, 1u, 0u, "无用户");
        lv_table_set_cell_value(table, 1u, 1u, "");
        for(row = 2u; row < max_rows; row++) {
            lv_table_set_cell_value(table, row, 0u, "");
            lv_table_set_cell_value(table, row, 1u, "");
        }
        return;
    }

    target = user_cnt;
    if(target > max_data_rows) target = max_data_rows;

    row = 1u;
    placed = 0u;
    if(!g_default_admin_deleted) {
        lv_table_set_cell_value(table, row, 0u, g_default_admin_account);
        lv_table_set_cell_value(table, row, 1u, g_default_admin_is_admin_role ? "管理员" : "用户");
        row++;
        placed++;
    }

    for(i = 0u; i < g_user_count && placed < target; i++) {
        if(!g_users[i].active) continue;
        lv_table_set_cell_value(table, row, 0u, g_users[i].acc);
        lv_table_set_cell_value(table, row, 1u, g_users[i].is_admin ? "管理员" : "用户");
        row++;
        placed++;
    }

    for(; row < max_rows; row++) {
        lv_table_set_cell_value(table, row, 0u, "");
        lv_table_set_cell_value(table, row, 1u, "");
    }
}

static void enter_screen_4(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_ops_t ops = build_nav_ops();
    ui_nav_enter_screen_4(&ctx, &ops);
}

static void screen5_hide_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_5_label_3, 1u);
}

static void screen5_show_error_label(void)
{
    ui_label_set_hidden(guider_ui.screen_5_label_3, 0u);
}

static void screen5_update_cursor_pos(void)
{
    lv_obj_t *ta = guider_ui.screen_5_ta_1;
    ui_auth_update_cursor_pos(g_screen5_cursor, ta);
}

static void screen5_set_field_selected(void)
{
    lv_obj_t *ta = guider_ui.screen_5_ta_1;
    if(!lv_obj_is_valid(ta)) return;
    ui_textarea_apply_input_style(ta, 1u);
    ui_cursor_attach_bar(&g_screen5_cursor, ta);
    screen5_update_cursor_pos();
    ui_cursor_activate(g_screen5_cursor, &g_screen5_cursor_visible, &g_screen5_cursor_last_ms);
}

static void screen5_handle_input_key(KeyValue_t key)
{
    lv_obj_t *ta = guider_ui.screen_5_ta_1;
    ui_auth_handle_input_key(ta, key, 12u, screen5_hide_error_label, screen5_update_cursor_pos);
}

static bool screen5_try_lookup_acc(void)
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

    /* 默认管理员可能不在 g_users 表里 */
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

static void enter_screen_5(void)
{
    /* 新查找：清空上次命中的账号缓存 */
    g_screen5_found_acc[0] = '\0';
    g_screen5_found_is_admin = 0u;

    ui_load_scr_animation(&guider_ui, &guider_ui.screen_5, guider_ui.screen_5_del, &guider_ui.screen_3_del,
                          setup_scr_screen_5, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_5;
    g_screen5_cursor = NULL;
    g_screen5_need_init = 1u;
}

static void screen6_get_pwd_for_found_acc(char *out_pwd, size_t out_sz)
{
    int idx;
    if(out_pwd == NULL || out_sz == 0u) return;

    if(!g_default_admin_deleted && strcmp(g_screen5_found_acc, g_default_admin_account) == 0) {
        strncpy(out_pwd, g_default_admin_password, out_sz - 1u);
        out_pwd[out_sz - 1u] = '\0';
        return;
    }

    idx = users_find_index_by_acc(g_screen5_found_acc);
    if(idx < 0) {
        out_pwd[0] = '\0';
        return;
    }

    strncpy(out_pwd, g_users[idx].pwd, out_sz - 1u);
    out_pwd[out_sz - 1u] = '\0';
}

static void screen6_set_focus(uint8_t focus)
{
    ui_screen6_set_focus_style(focus, &g_screen6_focus,
                               guider_ui.screen_6_btn_3, guider_ui.screen_6_btn_4,
                               guider_ui.screen_6_btn_6, guider_ui.screen_6_btn_7,
                               guider_ui.screen_6_ddlist_1);
}

static void screen6_update_labels_from_found(void)
{
    char pwd_buf[11];
    lv_obj_t *parent;

    if(!lv_obj_is_valid(guider_ui.screen_6_cont_1)) return;
    parent = guider_ui.screen_6_cont_1;

    if(!lv_obj_is_valid(guider_ui.screen_6_label_2) || !lv_obj_is_valid(guider_ui.screen_6_label_3)) return;

    screen6_get_pwd_for_found_acc(pwd_buf, sizeof(pwd_buf));

    /* 保留冒号前缀，数字用独立 label 显示，确保不会自动换行到下一行 */
    lv_label_set_text(guider_ui.screen_6_label_2, "账号：");
    lv_label_set_text(guider_ui.screen_6_label_3, "密码：");

    if(g_screen6_acc_val_label == NULL || !lv_obj_is_valid(g_screen6_acc_val_label)) {
        g_screen6_acc_val_label = lv_label_create(parent);
        lv_obj_set_style_text_font(g_screen6_acc_val_label, &lv_font_SourceHanSerifSC_Regular_20,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(g_screen6_acc_val_label, lv_color_hex(0x000000),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(g_screen6_acc_val_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(g_screen6_acc_val_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(g_screen6_acc_val_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if(g_screen6_pwd_val_label == NULL || !lv_obj_is_valid(g_screen6_pwd_val_label)) {
        g_screen6_pwd_val_label = lv_label_create(parent);
        lv_obj_set_style_text_font(g_screen6_pwd_val_label, &lv_font_SourceHanSerifSC_Regular_20,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(g_screen6_pwd_val_label, lv_color_hex(0x000000),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(g_screen6_pwd_val_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(g_screen6_pwd_val_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(g_screen6_pwd_val_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if(lv_obj_is_valid(g_screen6_acc_val_label)) {
        lv_label_set_text(g_screen6_acc_val_label, g_screen5_found_acc);
        lv_obj_set_pos(g_screen6_acc_val_label, 85, 71);
        lv_obj_set_size(g_screen6_acc_val_label, 90, 24);
    }
    if(lv_obj_is_valid(g_screen6_pwd_val_label)) {
        lv_label_set_text(g_screen6_pwd_val_label, pwd_buf);
        lv_obj_set_pos(g_screen6_pwd_val_label, 70, 111);
        lv_obj_set_size(g_screen6_pwd_val_label, 100, 24);
    }
}

static void screen6_save_identity(bool is_admin)
{
    int idx;
    const char *acc = g_screen5_found_acc;

    if(acc == NULL || acc[0] == '\0') return;

    if(!g_default_admin_deleted && strcmp(acc, g_default_admin_account) == 0) {
        g_default_admin_is_admin_role = is_admin ? 1u : 0u;
    } else {
        idx = users_find_index_by_acc(acc);
        if(idx >= 0) {
            g_users[idx].is_admin = is_admin ? 1u : 0u;
        }
    }

    g_screen5_found_is_admin = is_admin ? 1u : 0u;
    cloud_ota_service_report_event(CLOUD_EVT_USER_ROLE_CHANGE, acc);
}

static bool screen6_try_commit_account_change(const char *new_acc)
{
    size_t len;
    const char *old;
    int idx;

    if(new_acc == NULL) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_ACC_CHANGE_FAIL, g_screen5_found_acc);
        return false;
    }
    len = strlen(new_acc);
    if(len < 1u || len > 12u) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_ACC_CHANGE_FAIL, g_screen5_found_acc);
        return false;
    }

    old = g_screen5_found_acc;
    if(old[0] == '\0') {
        cloud_ota_service_report_event(CLOUD_EVT_USER_ACC_CHANGE_FAIL, g_screen5_found_acc);
        return false;
    }

    if(strcmp(old, new_acc) == 0) return true;

    if(users_find_index_by_acc(new_acc) >= 0) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_ACC_CHANGE_FAIL, old);
        return false;
    }
    if(!g_default_admin_deleted && strcmp(new_acc, g_default_admin_account) == 0 && strcmp(old, g_default_admin_account) != 0)
        cloud_ota_service_report_event(CLOUD_EVT_USER_ACC_CHANGE_FAIL, old);
        return false;

    if(!g_default_admin_deleted && strcmp(old, g_default_admin_account) == 0) {
        strncpy(g_default_admin_account, new_acc, sizeof(g_default_admin_account) - 1u);
        g_default_admin_account[sizeof(g_default_admin_account) - 1u] = '\0';
        strncpy(g_screen5_found_acc, new_acc, sizeof(g_screen5_found_acc) - 1u);
        g_screen5_found_acc[sizeof(g_screen5_found_acc) - 1u] = '\0';
        cloud_ota_service_report_event(CLOUD_EVT_USER_ACC_CHANGE_OK, new_acc);
        return true;
    }

    idx = users_find_index_by_acc(old);
    if(idx < 0) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_ACC_CHANGE_FAIL, old);
        return false;
    }
    strncpy(g_users[idx].acc, new_acc, sizeof(g_users[idx].acc) - 1u);
    g_users[idx].acc[sizeof(g_users[idx].acc) - 1u] = '\0';
    strncpy(g_screen5_found_acc, new_acc, sizeof(g_screen5_found_acc) - 1u);
    g_screen5_found_acc[sizeof(g_screen5_found_acc) - 1u] = '\0';
    cloud_ota_service_report_event(CLOUD_EVT_USER_ACC_CHANGE_OK, new_acc);
    return true;
}

static bool screen6_try_commit_password_change(const char *new_pwd)
{
    size_t len;
    const char *acc;
    int idx;

    if(new_pwd == NULL) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_FAIL, g_screen5_found_acc);
        return false;
    }
    len = strlen(new_pwd);
    if(len < 4u || len > 10u) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_FAIL, g_screen5_found_acc);
        return false;
    }

    acc = g_screen5_found_acc;
    if(acc[0] == '\0') {
        cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_FAIL, g_screen5_found_acc);
        return false;
    }

    if(!g_default_admin_deleted && strcmp(acc, g_default_admin_account) == 0) {
        strncpy(g_default_admin_password, new_pwd, sizeof(g_default_admin_password) - 1u);
        g_default_admin_password[sizeof(g_default_admin_password) - 1u] = '\0';
        cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_OK, acc);
        return true;
    }

    idx = users_find_index_by_acc(acc);
    if(idx < 0) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_FAIL, acc);
        return false;
    }
    strncpy(g_users[idx].pwd, new_pwd, sizeof(g_users[idx].pwd) - 1u);
    g_users[idx].pwd[sizeof(g_users[idx].pwd) - 1u] = '\0';
    cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_OK, acc);
    return true;
}

static void screen6_close_dialog(void)
{
    if(g_screen6_dlg_root != NULL && lv_obj_is_valid(g_screen6_dlg_root)) {
        lv_obj_del(g_screen6_dlg_root);
    }
    g_screen6_dlg_root = NULL;
    g_screen6_dlg_ta = NULL;
    g_screen6_dlg_cursor = NULL;
}

static void screen6_dlg_update_cursor_pos(void)
{
    lv_obj_t *ta = g_screen6_dlg_ta;
    ui_auth_update_cursor_pos(g_screen6_dlg_cursor, ta);
}

static void screen6_dlg_setup_cursor_on_ta(lv_obj_t *ta)
{
    if(!lv_obj_is_valid(ta)) return;
    ui_cursor_attach_bar(&g_screen6_dlg_cursor, ta);
    lv_obj_set_style_text_font(g_screen6_dlg_cursor, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    screen6_dlg_update_cursor_pos();
    ui_cursor_activate(g_screen6_dlg_cursor, &g_screen6_dlg_cursor_vis, &g_screen6_dlg_cursor_ms);
}

static void screen6_dlg_style_ta_like_screen5(lv_obj_t *ta, uint16_t max_len)
{
    lv_textarea_set_text(ta, "");
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, false);
    lv_textarea_set_max_length(ta, max_len);
    lv_obj_set_style_text_color(ta, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ta, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ta, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ta, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ta, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ta, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ta, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ta, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ta, lv_color_hex(0xe6e6e6), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ta, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ta, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ta, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ta, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(ta, 0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_width(ta, 0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ta, 0, LV_PART_CURSOR | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ta, 0, LV_PART_CURSOR | LV_STATE_DEFAULT);
}

static void screen6_dlg_handle_key(KeyValue_t key);

static void screen6_dlg_close_x_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    screen6_dlg_handle_key(KEY_ESC);
}

static void screen6_dlg_ok_btn_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    screen6_dlg_handle_key(KEY_OK);
}

static void screen6_dlg_cancel_btn_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    screen6_dlg_handle_key(KEY_ESC);
}

static void screen6_dlg_apply_overlay(lv_obj_t *root)
{
    lv_obj_set_size(root, 240, 320);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(root, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void screen6_dlg_apply_popup_panel(lv_obj_t *panel, lv_coord_t w, lv_coord_t h)
{
    lv_obj_set_size(panel, w, h);
    lv_obj_center(panel);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void screen6_dlg_add_close_x(lv_obj_t *panel)
{
    lv_obj_t *btn_x;
    lv_obj_t *lbl_x;

    btn_x = lv_btn_create(panel);
    lv_obj_set_size(btn_x, 24, 24);
    lv_obj_align(btn_x, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_set_style_radius(btn_x, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn_x, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_x, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_x, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_x, screen6_dlg_close_x_event_cb, LV_EVENT_CLICKED, NULL);

    lbl_x = lv_label_create(btn_x);
    lv_label_set_text(lbl_x, "X");
    lv_obj_set_style_text_font(lbl_x, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_x, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(lbl_x);
}

static void screen6_dlg_add_bottom_ok_cancel(lv_obj_t *panel)
{
    lv_obj_t *btn_ok;
    lv_obj_t *btn_esc;
    lv_obj_t *lbl_ok;
    lv_obj_t *lbl_esc;

    btn_ok = lv_btn_create(panel);
    btn_esc = lv_btn_create(panel);
    lv_obj_set_size(btn_ok, 62, 30);
    lv_obj_set_size(btn_esc, 62, 30);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    lv_obj_align(btn_esc, LV_ALIGN_BOTTOM_RIGHT, -12, -12);

    lv_obj_set_style_radius(btn_ok, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_esc, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_ok, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_esc, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn_ok, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn_esc, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_esc, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(btn_ok, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(btn_esc, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_ok, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_esc, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_ok, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_esc, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(btn_ok, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(btn_esc, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(btn_ok, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(btn_esc, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(btn_ok, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(btn_esc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn_ok, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn_esc, 0, LV_STATE_DEFAULT);

    lbl_ok = lv_label_create(btn_ok);
    lbl_esc = lv_label_create(btn_esc);
    lv_label_set_text(lbl_ok, "OK");
    lv_label_set_text(lbl_esc, "ESC");
    lv_label_set_long_mode(lbl_ok, LV_LABEL_LONG_WRAP);
    lv_label_set_long_mode(lbl_esc, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_ok, LV_PCT(100));
    lv_obj_set_width(lbl_esc, LV_PCT(100));
    lv_obj_set_style_text_font(lbl_ok, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_esc, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_ok, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_esc, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl_ok, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl_esc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_ok, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(lbl_esc, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(btn_ok, screen6_dlg_ok_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_esc, screen6_dlg_cancel_btn_event_cb, LV_EVENT_CLICKED, NULL);
}

static void screen6_show_result_dialog(bool success)
{
    lv_obj_t *scr = guider_ui.screen_6;
    lv_obj_t *panel;
    lv_obj_t *msg;

    if(!lv_obj_is_valid(scr)) return;

    g_screen6_dlg_root = lv_obj_create(scr);
    screen6_dlg_apply_overlay(g_screen6_dlg_root);

    panel = lv_obj_create(g_screen6_dlg_root);
    screen6_dlg_apply_popup_panel(panel, 180, 100);

    screen6_dlg_add_close_x(panel);

    msg = lv_label_create(panel);
    lv_label_set_text(msg, success ? "更改成功" : "更改失败");
    lv_label_set_long_mode(msg, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(msg, 150);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(msg, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(msg, lv_color_hex(0x0D3055), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(msg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(msg);

    g_screen6_dlg_ta = NULL;
    g_screen6_dlg_cursor = NULL;

    lv_obj_move_foreground(g_screen6_dlg_root);
}

static void screen6_open_edit_account_dialog(void)
{
    lv_obj_t *scr = guider_ui.screen_6;
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *ta;

    if(!lv_obj_is_valid(scr)) return;

    screen6_close_dialog();
    g_screen6_dlg_saved_focus = g_screen6_focus;
    g_screen6_dlg = SCREEN6_DLG_EDIT_ACC;

    g_screen6_dlg_root = lv_obj_create(scr);
    screen6_dlg_apply_overlay(g_screen6_dlg_root);

    panel = lv_obj_create(g_screen6_dlg_root);
    screen6_dlg_apply_popup_panel(panel, 224, 220);

    screen6_dlg_add_close_x(panel);

    title = lv_label_create(panel);
    lv_label_set_text(title, "更改后的账号：");
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(title, 200);
    lv_obj_set_style_text_font(title, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    ta = lv_textarea_create(panel);
    lv_obj_set_size(ta, 200, 35);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 78);
    screen6_dlg_style_ta_like_screen5(ta, 12u);
    g_screen6_dlg_ta = ta;
    screen6_dlg_setup_cursor_on_ta(ta);

    screen6_dlg_add_bottom_ok_cancel(panel);

    lv_obj_move_foreground(g_screen6_dlg_root);
}

static void screen6_open_edit_password_dialog(void)
{
    lv_obj_t *scr = guider_ui.screen_6;
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *ta;

    if(!lv_obj_is_valid(scr)) return;

    screen6_close_dialog();
    g_screen6_dlg_saved_focus = g_screen6_focus;
    g_screen6_dlg = SCREEN6_DLG_EDIT_PWD;

    g_screen6_dlg_root = lv_obj_create(scr);
    screen6_dlg_apply_overlay(g_screen6_dlg_root);

    panel = lv_obj_create(g_screen6_dlg_root);
    screen6_dlg_apply_popup_panel(panel, 224, 220);

    screen6_dlg_add_close_x(panel);

    title = lv_label_create(panel);
    lv_label_set_text(title, "更改后的密码：");
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(title, 200);
    lv_obj_set_style_text_font(title, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    ta = lv_textarea_create(panel);
    lv_obj_set_size(ta, 200, 35);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 78);
    screen6_dlg_style_ta_like_screen5(ta, 10u);
    g_screen6_dlg_ta = ta;
    screen6_dlg_setup_cursor_on_ta(ta);

    screen6_dlg_add_bottom_ok_cancel(panel);

    lv_obj_move_foreground(g_screen6_dlg_root);
}

static void screen6_dlg_handle_ta_key(KeyValue_t key)
{
    lv_obj_t *ta = g_screen6_dlg_ta;
    uint16_t max_len;
    int digit;
    uint16_t cur_len;

    if(!lv_obj_is_valid(ta)) return;

    max_len = (g_screen6_dlg == SCREEN6_DLG_EDIT_ACC) ? 12u : 10u;
    digit = key_to_digit(key);

    if(key == KEY_LEFT) {
        lv_textarea_del_char(ta);
        screen6_dlg_update_cursor_pos();
        return;
    }

    if(digit < 0) return;

    cur_len = (uint16_t)strlen(lv_textarea_get_text(ta));
    if(cur_len >= max_len) return;

    lv_textarea_add_char(ta, (uint32_t)('0' + digit));
    screen6_dlg_update_cursor_pos();
}

static void screen6_dlg_handle_key(KeyValue_t key)
{
    bool ok;

    if(g_screen6_dlg == SCREEN6_DLG_RESULT_OK || g_screen6_dlg == SCREEN6_DLG_RESULT_FAIL) {
        if(key == KEY_OK || key == KEY_ESC) {
            screen6_close_dialog();
            g_screen6_dlg = SCREEN6_DLG_NONE;
            screen6_update_labels_from_found();
            if(lv_obj_is_valid(guider_ui.screen_6_ddlist_1)) {
                lv_dropdown_set_selected(guider_ui.screen_6_ddlist_1, g_screen5_found_is_admin ? 1u : 0u);
            }
            screen6_set_focus(g_screen6_dlg_saved_focus);
        }
        return;
    }

    if(g_screen6_dlg == SCREEN6_DLG_EDIT_ACC || g_screen6_dlg == SCREEN6_DLG_EDIT_PWD) {
        if(key == KEY_ESC) {
            screen6_close_dialog();
            g_screen6_dlg = SCREEN6_DLG_NONE;
            screen6_set_focus(g_screen6_dlg_saved_focus);
            return;
        }
        if(key == KEY_OK) {
            const char *txt = lv_textarea_get_text(g_screen6_dlg_ta);
            if(g_screen6_dlg == SCREEN6_DLG_EDIT_ACC) {
                ok = screen6_try_commit_account_change(txt);
            } else {
                ok = screen6_try_commit_password_change(txt);
            }
            screen6_close_dialog();
            g_screen6_dlg = ok ? SCREEN6_DLG_RESULT_OK : SCREEN6_DLG_RESULT_FAIL;
            screen6_show_result_dialog(ok);
            return;
        }
        screen6_dlg_handle_ta_key(key);
    }
}

static void screen6_dlg_cursor_blink_handle(void)
{
    if(g_app_scr != APP_SCR_6) return;
    if(g_screen6_dlg != SCREEN6_DLG_EDIT_ACC && g_screen6_dlg != SCREEN6_DLG_EDIT_PWD) return;
    ui_cursor_blink_step(g_screen6_dlg_cursor, &g_screen6_dlg_cursor_vis, &g_screen6_dlg_cursor_ms, 500u);
}

static void enter_screen_6(void)
{
    lv_obj_t *dd_list;
    screen6_close_dialog();
    g_screen6_dlg = SCREEN6_DLG_NONE;

    ui_load_scr_animation(&guider_ui, &guider_ui.screen_6, guider_ui.screen_6_del, &guider_ui.screen_5_del,
                          setup_scr_screen_6, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_6;

    /* 显示查找到的账号/密码 */
    screen6_update_labels_from_found();

    /* 根据查找到的身份，初始化下拉框 */
    if(lv_obj_is_valid(guider_ui.screen_6_ddlist_1)) {
        /* options: "用户\n管理员" => 0:用户, 1:管理员 */
        lv_dropdown_set_selected(guider_ui.screen_6_ddlist_1, g_screen5_found_is_admin ? 1u : 0u);
        /* 下拉列表默认样式可能被生成为英文字体，运行时强制改为中文字体避免“空白字形” */
        dd_list = lv_dropdown_get_list(guider_ui.screen_6_ddlist_1);
        if(lv_obj_is_valid(dd_list)) {
            lv_obj_set_style_text_font(dd_list, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(dd_list, lv_color_hex(0x0D3055), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(dd_list, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_SELECTED | LV_STATE_CHECKED);
            lv_obj_set_style_text_color(dd_list, lv_color_hex(0xffffff), LV_PART_SELECTED | LV_STATE_CHECKED);
        }
    }

    /* 默认选中：账号正对的“更改”(screen_6_btn_3) */
    screen6_set_focus(0u);

    if(lv_obj_is_valid(guider_ui.screen_6_ddlist_1)) {
        lv_dropdown_close(guider_ui.screen_6_ddlist_1);
    }
}

static void enter_screen_7(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_enter_screen_7(&ctx);
}

static void try_screen2_admin_login(void)
{
    const char *acc;
    const char *pwd;

    if(!lv_obj_is_valid(guider_ui.screen_2_ta_2) || !lv_obj_is_valid(guider_ui.screen_2_ta_1)) return;

    acc = lv_textarea_get_text(guider_ui.screen_2_ta_2);
    pwd = lv_textarea_get_text(guider_ui.screen_2_ta_1);
    if(admin_credentials_match_with_delete(acc, pwd)) {
        cloud_ota_service_report_event(CLOUD_EVT_ADMIN_LOGIN_OK, acc);
        enter_screen_3();
    } else {
        cloud_ota_service_report_event(CLOUD_EVT_ADMIN_LOGIN_FAIL, acc);
        screen2_show_error_label();
    }
}

static void go_back_prev_screen(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();
    ui_nav_ops_t ops = build_nav_ops();
    ui_nav_go_back_prev_screen(&ctx, &ops);
}

static void key_ui_handle(void)
{
    KeyValue_t key = KEY_Scan_WithDebounce(20);
    if(key == KEY_NONE) return;

    if(g_app_scr == APP_SCR_HOME) {
        if(key == KEY_RIGHT) {
            lock_btn_set_selected(1);
        } else if(key == KEY_LEFT) {
            menu_btn_set_selected(1);
        } else if(key == KEY_OK && g_lock_btn_selected) {
            enter_screen_1();
        } else if(key == KEY_OK && g_menu_btn_selected) {
            enter_screen_2();
        }
        return;
    }

    if(g_app_scr == APP_SCR_6 && g_screen6_dlg != SCREEN6_DLG_NONE) {
        screen6_dlg_handle_key(key);
        return;
    }

    /* ESC：screen_3 直接回首页，其余页面默认返回上一级。
     * 但 screen_6/8/9/10 有录入弹窗时，优先交给对应页面分支处理，避免切屏与弹窗状态机冲突。 */
    if(key == KEY_ESC) {
        if((g_app_scr == APP_SCR_6 || g_app_scr == APP_SCR_8 || g_app_scr == APP_SCR_9 || g_app_scr == APP_SCR_10) &&
           (g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup))) {
            /* handled in APP_SCR_8/APP_SCR_9 branches below */
        } else
        if(g_app_scr == APP_SCR_3) {
            ui_load_scr_animation(&guider_ui, &guider_ui.screen, guider_ui.screen_del, &guider_ui.screen_3_del,
                                  setup_scr_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
            g_app_scr = APP_SCR_HOME;
            g_screen3_menu_index = 0u;
            lock_btn_set_selected(0);
            menu_btn_set_selected(0);
            return;
        }
        else {
            go_back_prev_screen();
            return;
        }
    }

    if(g_app_scr == APP_SCR_1) {
        if(g_screen1_unlock_popup != NULL && lv_obj_is_valid(g_screen1_unlock_popup)) {
            return;
        }
        if(key == KEY_OK) {
            const char *acc = lv_textarea_get_text(guider_ui.screen_1_ta_1);
            const char *pwd = lv_textarea_get_text(guider_ui.screen_1_ta_2);
            if(unlock_credentials_match_with_delete(acc, pwd)) {
                screen1_hide_error_label();
                screen1_show_unlock_popup();
                cloud_ota_service_report_event(CLOUD_EVT_UNLOCK_OK, acc);
                cloud_ota_service_report_unlock_record(acc, "password", HAL_GetTick());
            } else {
                screen1_show_error_label();
            }
        } else if(key == KEY_UP && g_screen1_field_index > 0u) {
            screen1_set_field_selected(g_screen1_field_index - 1u);
        } else if(key == KEY_DOWN && g_screen1_field_index < 1u) {
            screen1_set_field_selected(g_screen1_field_index + 1u);
        } else {
            screen1_handle_input_key(key);
        }
    } else if(g_app_scr == APP_SCR_2) {
        if(key == KEY_OK) {
            try_screen2_admin_login();
        } else if(key == KEY_UP && g_screen2_field_index > 0u) {
            screen2_set_field_selected(g_screen2_field_index - 1u);
        } else if(key == KEY_DOWN && g_screen2_field_index < 1u) {
            screen2_set_field_selected(g_screen2_field_index + 1u);
        } else {
            screen2_handle_input_key(key);
        }
    } else if(g_app_scr == APP_SCR_3) {
        if(key == KEY_UP) {
            if(g_screen3_menu_index == 0u) {
                screen3_set_menu_selected(3u);
            } else {
                screen3_set_menu_selected(g_screen3_menu_index - 1u);
            }
        } else if(key == KEY_DOWN) {
            if(g_screen3_menu_index >= 3u) {
                screen3_set_menu_selected(0u);
            } else {
                screen3_set_menu_selected(g_screen3_menu_index + 1u);
            }
        } else if(key == KEY_OK && g_screen3_menu_index == 0u) {
            enter_screen_8();
        } else if(key == KEY_OK && g_screen3_menu_index == 1u) {
            enter_screen_7();
        } else if(key == KEY_OK && g_screen3_menu_index == 2u) {
            enter_screen_5();
        } else if(key == KEY_OK && g_screen3_menu_index == 3u) {
            enter_screen_4();
        }
    } else if(g_app_scr == APP_SCR_5) {
        if(key == KEY_OK) {
            if(screen5_try_lookup_acc()) {
                screen5_hide_error_label();
                enter_screen_6();
            } else {
                screen5_show_error_label();
            }
        } else {
            screen5_handle_input_key(key);
        }
    } else if(g_app_scr == APP_SCR_6) {
        if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
            if(key == KEY_OK || key == KEY_ESC) {
                if(g_nfc_enroll_state == 1u) {
                    if(key == KEY_ESC) {
                        g_nfc_enroll_state = 2u;
                        g_nfc_enroll_result = 0u;
                        screen8_show_enroll_popup("NFC FAIL");
                    }
                    return;
                }
                if(g_screen8_fp_enroll_state == 1u) {
                    if(key == KEY_ESC) {
                        g_screen8_fp_enroll_state = 2u;
                        g_screen8_fp_enroll_result = 0u;
                        screen8_show_enroll_popup("Fingerprint FAIL");
                    }
                    return;
                }
                if(g_nfc_enroll_state == 2u) g_nfc_enroll_state = 0u;
                if(g_screen8_fp_enroll_state == 2u) g_screen8_fp_enroll_state = 0u;
                screen8_popup_close_only();
            }
            return;
        }

        lv_obj_t *dd = guider_ui.screen_6_ddlist_1;
        if(!lv_obj_is_valid(dd)) return;

        const bool dd_open = lv_dropdown_is_open(dd) ? true : false;

        if(dd_open) {
            /* 身份下拉框打开时：UP/DOWN选择，OK确认保存 */
            if(g_screen6_focus == 2u) {
                if(key == KEY_UP) {
                    char k = LV_KEY_UP;
                    lv_event_send(dd, LV_EVENT_KEY, &k);
                    return;
                } else if(key == KEY_DOWN) {
                    char k = LV_KEY_DOWN;
                    lv_event_send(dd, LV_EVENT_KEY, &k);
                    return;
                } else if(key == KEY_OK) {
                    /* 键盘 UP/DOWN 只更新 sel_opt_id；get_selected_str 仍读 sel_opt_id_orig，须用 get_selected 并 set_selected 同步显示 */
                    uint16_t sel = lv_dropdown_get_selected(dd);
                    screen6_save_identity(sel == 1u);
                    if(lv_dropdown_get_option_cnt(dd) > 1u) {
                        lv_dropdown_set_selected(dd, sel == 0u ? 1u : 0u);
                    }
                    lv_dropdown_set_selected(dd, sel);
                    lv_dropdown_close(dd);
                    screen6_set_focus(2u);
                    return;
                } else if(key == KEY_ESC) {
                    char k = LV_KEY_ESC;
                    lv_event_send(dd, LV_EVENT_KEY, &k);
                    screen6_set_focus(2u);
                    return;
                }
            }
            /* 下拉框打开但当前不在身份选定上：不做焦点切换 */
            return;
        }

        /* 下拉框关闭：UP/DOWN按界面从上到下顺序切换（不选中 ESC，且循环回绕） */
        if(key == KEY_UP) {
            if(g_screen6_focus == 0u) g_screen6_focus = 4u;
            else g_screen6_focus--;
            screen6_set_focus(g_screen6_focus);
        } else if(key == KEY_DOWN) {
            if(g_screen6_focus == 4u) g_screen6_focus = 0u;
            else g_screen6_focus++;
            screen6_set_focus(g_screen6_focus);
        } else if(key == KEY_OK) {
            if(g_screen6_focus == 0u) {
                screen6_open_edit_account_dialog();
            } else if(g_screen6_focus == 1u) {
                screen6_open_edit_password_dialog();
            } else if(g_screen6_focus == 2u) {
                lv_dropdown_open(dd);
            } else if(g_screen6_focus == 3u) {
                enter_screen_10();
            } else if(g_screen6_focus == 4u) {
                enter_screen_9();
            }
        }
    } else if(g_app_scr == APP_SCR_7) {
        if(g_screen7_msgbox_state == 1u) {
            if(key == KEY_RIGHT) {
                screen7_set_msgbox1_choice(0u);
            } else if(key == KEY_LEFT) {
                screen7_set_msgbox1_choice(1u);
            } else if(key == KEY_OK) {
                if(g_screen7_choice_yes) {
                    const char *acc = lv_textarea_get_text(guider_ui.screen_7_ta_1);
                    if(strcmp(acc, g_default_admin_account) == 0) {
                        g_default_admin_deleted = 1u;
                        g_default_admin_has_nfc = 0u;
                        memset(g_default_admin_nfc_uid, 0, sizeof(g_default_admin_nfc_uid));
                        users_clear_fp_by_acc(acc, 1u);
                        screen7_show_msgbox2();
                    } else if(users_try_delete_by_acc(acc)) {
                        screen7_show_msgbox2();
                    } else {
                        screen7_show_msgbox3();
                    }
                } else {
                    screen7_show_msgbox3();
                }
            }
        } else if(g_screen7_msgbox_state == 2u || g_screen7_msgbox_state == 3u) {
            if(key == KEY_OK || key == KEY_ESC) {
                screen7_hide_all_msgbox();
                go_back_prev_screen();
            }
        } else {
            if(key == KEY_OK) {
                screen7_show_msgbox1();
            } else {
                screen7_handle_input_key(key);
            }
        }
    } else if(g_app_scr == APP_SCR_8) {
        if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
            if(key == KEY_OK || key == KEY_ESC) {
                if(g_nfc_enroll_state == 1u) {
                    /* ESC on "NFC Working..." aborts enroll and shows fail result. */
                    if(key == KEY_ESC) {
                        g_nfc_enroll_state = 2u;
                        g_nfc_enroll_result = 0u;
                        screen8_show_enroll_popup("NFC FAIL");
                    }
                    return; /* keep blocking close while running */
                }
                if(g_screen8_fp_enroll_state == 1u) {
                    if(key == KEY_ESC) {
                        g_screen8_fp_enroll_state = 2u;
                        g_screen8_fp_enroll_result = 0u;
                        screen8_show_enroll_popup("Fingerprint FAIL");
                    }
                    return; /* 录入进行中，禁止提前关闭弹窗 */
                }
                if(g_nfc_enroll_state == 2u) {
                    g_nfc_enroll_state = 0u;
                }
                if(g_screen8_fp_enroll_state == 2u) {
                    g_screen8_fp_enroll_state = 0u;
                }
                screen8_popup_close_only();
            }
            return;
        }
        if(key == KEY_UP) {
            if(g_screen8_focus == 0u) {
                screen8_set_focus(SCREEN8_N_FOCUS - 1u);
            } else {
                screen8_set_focus(g_screen8_focus - 1u);
            }
        } else if(key == KEY_DOWN) {
            if(g_screen8_focus >= SCREEN8_N_FOCUS - 1u) {
                screen8_set_focus(0u);
            } else {
                screen8_set_focus(g_screen8_focus + 1u);
            }
        } else if(key == KEY_OK) {
            if(g_screen8_focus == 2u) {
                if(lv_obj_has_state(guider_ui.screen_8_cb_1, LV_STATE_CHECKED)) {
                    lv_obj_clear_state(guider_ui.screen_8_cb_1, LV_STATE_CHECKED);
                } else {
                    lv_obj_add_state(guider_ui.screen_8_cb_1, LV_STATE_CHECKED);
                }
            } else if(g_screen8_focus == 3u) {
                if(screen8_has_valid_acc_pwd_input()) {
                    screen8_show_fp_enroll_popup();
                } else {
                    screen8_show_enroll_popup("Need Account/PWD");
                }
            } else if(g_screen8_focus == 4u) {
                if(screen8_has_valid_acc_pwd_input()) {
                    g_nfc_op = NFC_OP_ENROLL_SCREEN8;
                    screen8_show_nfc_enroll_popup();
                } else {
                    screen8_show_enroll_popup("Need Account/PWD");
                }
            } else if(g_screen8_focus == 5u) {
                screen8_attempt_commit();
            }
            /* focus 0/1：OK 不提交；focus 3/4：录入；focus 5：提交 */
        } else if(g_screen8_focus <= 1u) {
            screen8_handle_ta_key(key);
        }
    } else if(g_app_scr == APP_SCR_9) {
        if(g_screen9_msgbox_state == 1u) {
            if(key == KEY_RIGHT) {
                screen9_set_msgbox_choice(0u);
            } else if(key == KEY_LEFT) {
                screen9_set_msgbox_choice(1u);
            } else if(key == KEY_OK) {
                if(g_screen9_choice_yes) {
                    bool can_delete = false;
                    int idx = users_find_index_by_acc(g_screen5_found_acc);
                    if(!g_default_admin_deleted && strcmp(g_screen5_found_acc, g_default_admin_account) == 0) {
                        can_delete = g_default_admin_has_nfc ? true : false;
                    } else if(idx >= 0 && g_users[idx].has_nfc) {
                        can_delete = true;
                    }
                    if(can_delete) {
                        users_clear_nfc_by_acc(g_screen5_found_acc);
                        cloud_ota_service_report_event(CLOUD_EVT_NFC_DELETE_OK, g_screen5_found_acc);
                        screen9_show_delete_result_popup(1u);
                    } else {
                        cloud_ota_service_report_event(CLOUD_EVT_NFC_DELETE_FAIL, g_screen5_found_acc);
                        screen9_show_delete_result_popup(0u);
                    }
                } else {
                    screen9_hide_all_msgbox();
                }
            }
        } else if(g_screen9_msgbox_state == 2u || g_screen9_msgbox_state == 3u) {
            if(key == KEY_OK || key == KEY_ESC) {
                screen9_hide_all_msgbox();
            }
        } else if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
            if(key == KEY_OK || key == KEY_ESC) {
                if(g_nfc_enroll_state == 1u) {
                    /* ESC on "NFC Working..." aborts enroll and shows fail result. */
                    if(key == KEY_ESC) {
                        g_nfc_enroll_state = 2u;
                        g_nfc_enroll_result = 0u;
                        screen8_show_enroll_popup("NFC FAIL");
                    }
                    return; /* keep blocking close while running */
                }
                if(g_screen8_fp_enroll_state == 1u) {
                    if(key == KEY_ESC) {
                        g_screen8_fp_enroll_state = 2u;
                        g_screen8_fp_enroll_result = 0u;
                        screen8_show_enroll_popup("Fingerprint FAIL");
                    }
                    return; /* 录入进行中，禁止提前关闭弹窗 */
                }
                if(g_nfc_enroll_state == 2u) g_nfc_enroll_state = 0u;
                if(g_screen8_fp_enroll_state == 2u) g_screen8_fp_enroll_state = 0u;
                screen8_popup_close_only();
                g_nfc_op = NFC_OP_NONE;
            }
        } else if(key == KEY_UP) {
            if(g_screen9_focus == 0u) g_screen9_focus = 1u;
            else g_screen9_focus--;
            screen9_set_focus(g_screen9_focus);
        } else if(key == KEY_DOWN) {
            if(g_screen9_focus == 1u) g_screen9_focus = 0u;
            else g_screen9_focus++;
            screen9_set_focus(g_screen9_focus);
        } else if(key == KEY_OK) {
            if(g_screen9_focus == 0u) {
                screen9_start_nfc_replace();
            } else {
                screen9_show_delete_confirm_popup();
            }
        }
    } else if(g_app_scr == APP_SCR_10) {
        if(g_screen9_msgbox_state == 1u) {
            if(key == KEY_RIGHT) {
                screen9_set_msgbox_choice(0u);
            } else if(key == KEY_LEFT) {
                screen9_set_msgbox_choice(1u);
            } else if(key == KEY_OK) {
                if(g_screen9_choice_yes) {
                    uint8_t ok = screen10_try_delete_fp();
                    screen10_show_delete_result_popup(ok);
                } else {
                    screen9_hide_all_msgbox();
                }
            }
            return;
        }
        if(g_screen9_msgbox_state == 2u || g_screen9_msgbox_state == 3u) {
            if(key == KEY_OK || key == KEY_ESC) {
                screen9_hide_all_msgbox();
            }
            return;
        }

        if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
            if(key == KEY_OK || key == KEY_ESC) {
                if(g_screen8_fp_enroll_state == 1u) {
                    if(key == KEY_ESC) {
                        g_screen8_fp_enroll_state = 2u;
                        g_screen8_fp_enroll_result = 0u;
                        screen8_show_enroll_popup("Fingerprint FAIL");
                    }
                    return;
                }
                if(g_screen8_fp_enroll_state == 2u) {
                    g_screen8_fp_enroll_state = 0u;
                }
                screen8_popup_close_only();
            }
            return;
        }

        if(key == KEY_UP) {
            if(g_screen10_focus == 0u) g_screen10_focus = 2u;
            else g_screen10_focus--;
            screen10_set_focus(g_screen10_focus);
        } else if(key == KEY_DOWN) {
            if(g_screen10_focus >= 2u) g_screen10_focus = 0u;
            else g_screen10_focus++;
            screen10_set_focus(g_screen10_focus);
        } else if(key == KEY_OK) {
            if(g_screen10_focus == 0u) {
                screen8_show_fp_enroll_popup();
            } else if(g_screen10_focus == 1u) {
                screen10_show_delete_confirm_popup();
            } else {
                go_back_prev_screen();
            }
        }
    }

    /* screen_4：显示用户列表 */
    if(g_app_scr == APP_SCR_4) {
        lv_obj_t *table = guider_ui.screen_4_table_1;
        if(!lv_obj_is_valid(table)) return;
        if(key == KEY_UP) {
            int32_t k = LV_KEY_UP;
            lv_event_send(table, LV_EVENT_KEY, &k);
        } else if(key == KEY_DOWN) {
            int32_t k = LV_KEY_DOWN;
            lv_event_send(table, LV_EVENT_KEY, &k);
        }
    }
}

static void board_default_gpio_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    gpio_init.Pin = GPIO_PIN_9 | GPIO_PIN_11;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio_init);

    /* 默认初始值：PE9 / PE11 输出高电平 */
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9 | GPIO_PIN_11, GPIO_PIN_SET);
}




void app_main_entry(void)
{
    HAL_Init();                         /* 初始化HAL库 */
    sys_stm32_clock_init(336, 8, 2, 7); /* 配置时钟，168MHz */
    board_default_gpio_init();          /* 默认GPIO电平 */
    delay_init(168);                    /* 初始化延时 */
    usart_init(115200);                 /* 初始化串口 */
//    led_init();                         /* 初始化LED */
    KEY_Init();                         /* 初始化按键 */
//    sram_init();
//    lcd_init();                         /* 初始化LCD */
//    tp_dev.init();                      /* 初始化触摸屏 */
//    nfc_hw_init_once();                 /* NFC上电初始化（仅一次） */
#if (APP_BOOT_VTOR_RELOCATE == 1)
    SCB->VTOR = BOOT_APP_VTOR_ADDR;     /* BootLoader + APP 双工程: APP中断向量重定位 */
#endif
    
    btim_timx_int_init(1000 - 1, 84);   // 1ms中断初始化，重装载值、预分频值
    
    lv_init();                          // lvgl初始化
    lv_port_disp_init();                // 显示设备初始化
    lv_port_indev_init();               // 输入设备初始化
    
    setup_ui(&guider_ui);
    events_init(&guider_ui);
    tcp_mqtt_init();

    strncpy(g_default_admin_account, ADMIN_DEFAULT_ACCOUNT, sizeof(g_default_admin_account) - 1u);
    g_default_admin_account[sizeof(g_default_admin_account) - 1u] = '\0';
    strncpy(g_default_admin_password, ADMIN_DEFAULT_PASSWORD, sizeof(g_default_admin_password) - 1u);
    g_default_admin_password[sizeof(g_default_admin_password) - 1u] = '\0';
    (void)users_try_register("2", "2222", false);
    g_app_scr = APP_SCR_HOME;
    lock_btn_set_selected(0);
    menu_btn_set_selected(0);
    g_home_nfc_last_poll_ms = HAL_GetTick();
    g_home_fp_last_poll_ms = g_home_nfc_last_poll_ms;
    while (1) {
        if(g_usart_rx_sta & 0x8000u) {
            uint16_t len = (uint16_t)(g_usart_rx_sta & 0x3FFFu);
            if(len > 0u && len < USART_REC_LEN) {
                g_usart_rx_buf[len] = '\0';
                cloud_ota_service_ingest_line((const char *)g_usart_rx_buf);
            }
            g_usart_rx_sta = 0u;
        }

        /* 录入状态处理 */
        if((g_app_scr == APP_SCR_6 || g_app_scr == APP_SCR_8 || g_app_scr == APP_SCR_9 || g_app_scr == APP_SCR_10) &&
           (g_nfc_enroll_state == 1u || g_screen8_fp_enroll_state == 1u)) {
            if(g_nfc_enroll_state == 1u) {
                screen8_handle_nfc_enroll();
            } else {
                screen8_handle_fp_enroll();
            }
        }
        home_nfc_poll_handle();
        home_fp_poll_handle();
        
        if(g_screen3_need_init && lv_scr_act() == guider_ui.screen_3) {
            screen3_set_menu_selected(g_screen3_pending_index);
            g_screen3_need_init = 0u;
        }
        if(g_screen4_need_init && g_app_scr == APP_SCR_4) {
            lv_obj_t *table = guider_ui.screen_4_table_1;
            if(lv_obj_is_valid(table)) {
                lv_obj_add_state(table, LV_STATE_FOCUSED);
                lv_obj_add_state(table, LV_STATE_FOCUS_KEY);

                /* 默认选中第一行：仅当当前还没有选中单元格时才触发 */
                uint16_t r_sel = 0u;
                uint16_t c_sel = 0u;
                lv_table_get_selected_cell(table, &r_sel, &c_sel);
                if(r_sel == LV_TABLE_CELL_NONE || c_sel == LV_TABLE_CELL_NONE) {
                    int32_t k = LV_KEY_DOWN;
                    lv_event_send(table, LV_EVENT_KEY, &k);
                }
            }
            g_screen4_need_init = 0u;
        }
        if(g_screen5_need_init && g_app_scr == APP_SCR_5) {
            screen5_hide_error_label();
            if(lv_obj_is_valid(guider_ui.screen_5_ta_1)) {
                if(g_screen5_found_acc[0] != '\0') {
                    lv_textarea_set_text(guider_ui.screen_5_ta_1, g_screen5_found_acc);
                } else {
                    lv_textarea_set_text(guider_ui.screen_5_ta_1, "");
                }
                lv_textarea_set_max_length(guider_ui.screen_5_ta_1, 12);
            }
            screen5_set_field_selected();
            g_screen5_need_init = 0u;
        }
        if(g_screen7_need_init && lv_scr_act() == guider_ui.screen_7) {
            g_screen7_choice_yes = 1u;
            screen7_hide_all_msgbox();
            if(lv_obj_is_valid(guider_ui.screen_7_ta_1)) {
                lv_textarea_set_text(guider_ui.screen_7_ta_1, "");
                lv_textarea_set_max_length(guider_ui.screen_7_ta_1, 12);
            }
            screen7_set_field_selected();
            g_screen7_need_init = 0u;
        }
        key_ui_handle();
        screen1_cursor_blink_handle();
        screen2_cursor_blink_handle();
        screen5_cursor_blink_handle();
        screen6_dlg_cursor_blink_handle();
        screen7_cursor_blink_handle();
        screen8_cursor_blink_handle();
        tcp_mqtt_while();
        ota_firmware_get();
        delay_ms(5);
        lv_timer_handler();
    }

}

// NFC录入处理函数（非阻塞方式）
static void screen8_show_nfc_enroll_popup(void)
{
    if(!g_nfc_hw_ready) {
        nfc_hw_init_once();
    }
    nfc_hw_wakeup_no_reset();
    /* 每次录入前都重新拉起天线并刷新卡表，避免模块偶发休眠/状态卡死 */
    MFRC522_AntennaOn();
    Flash_ReadID();
    if(g_nfc_op == NFC_OP_ENROLL_SCREEN8) {
        g_screen8_chip_read_ok = 0u;
        g_nfc_pending_uid_valid = 0u;
    }

    screen8_show_enroll_popup("NFC Working...");
    g_nfc_enroll_start_time = HAL_GetTick();
    g_nfc_enroll_state = 1u;
    g_nfc_enroll_result = 0u;
}

// NFC录入状态处理（在主循环中调用）
static void screen8_handle_nfc_enroll(void)
{
    uint32_t current_time;
    static uint32_t s_last_poll_log = 0u;
    static uint32_t s_last_recover_ms = 0u;
    uint8_t card_id[4] = {0};
    uint8_t detect_result;

    if(g_nfc_enroll_state != 1u) return;

    current_time = HAL_GetTick();
    if((current_time - s_last_poll_log) >= 500u) s_last_poll_log = current_time;
    if((current_time - g_nfc_enroll_start_time) >= 10000u) {
        g_nfc_enroll_state = 2u;
        g_nfc_enroll_result = 0u;
        screen8_show_enroll_popup("NFC FAIL");
        return;
    }

    /* Recover MFRC522 periodically while waiting, improves stability after repeated attempts. */
    if((current_time - s_last_recover_ms) >= 1500u) {
        s_last_recover_ms = current_time;
        (void)MFRC522_Reset();
        MFRC522_AntennaOn();
    }

    detect_result = NFC_Enroll_Detect(card_id, 100);
    g_nfc_last_detect_result = detect_result;
    if(detect_result != 1u) {
        return;
    }
    g_nfc_last_uid[0] = card_id[0];
    g_nfc_last_uid[1] = card_id[1];
    g_nfc_last_uid[2] = card_id[2];
    g_nfc_last_uid[3] = card_id[3];

    g_nfc_enroll_state = 2u;
    if(card_id[0] == 0u && card_id[1] == 0u && card_id[2] == 0u && card_id[3] == 0u) {
        g_nfc_enroll_result = 0u;
        screen8_show_enroll_popup("NFC FAIL");
    } else {
        if(g_nfc_op == NFC_OP_REPLACE_SCREEN9) {
            if(g_screen5_found_acc[0] != '\0' && users_bind_nfc_by_acc(g_screen5_found_acc, card_id)) {
                g_nfc_enroll_result = 1u;
            } else {
                g_nfc_enroll_result = 0u;
            }
        } else {
            memcpy(g_nfc_pending_uid, card_id, 4u);
            g_nfc_pending_uid_valid = 1u;
            g_screen8_chip_read_ok = 1u;
            g_nfc_enroll_result = 1u;
        }
        screen8_show_enroll_popup((g_nfc_enroll_result == 1u) ? "NFC SUCCESS" : "NFC FAIL");
    }
}

#if 0
// 以下为误粘贴的重复代码，暂时屏蔽
static void screen8_show_nfc_enroll_popup(void)
{
    // 显示"正在录入中..."弹窗
    screen8_show_nfc_popup("NFC正在添加中...");
    
    // 开始NFC录入流程
    g_nfc_enroll_start_time = HAL_GetTick();
    g_nfc_enroll_state = 1; // 录入中状态
    g_nfc_enroll_result = 0;
}

// NFC录入状态处理（在主循环中调用）
static void screen8_handle_nfc_enroll(void)
{
    if(g_nfc_enroll_state != 1) return;
    
    uint32_t current_time = HAL_GetTick();
    
    // 检查是否超时（10秒）
    if((current_time - g_nfc_enroll_start_time) >= 10000) {
        // 超时未检测到NFC卡
        g_nfc_enroll_state = 2;
        g_nfc_enroll_result = 0; // 失败
        screen8_show_nfc_popup("NFC添加失败");
        return;
    }
    
    // 尝试检测NFC卡
    uint8_t card_id[4] = {0};
    uint8_t detect_result = NFC_Enroll_Detect(card_id, 100); // 检测100ms
    
    if(detect_result == 1) {
        // 检测到NFC卡，尝试录入到Flash
        uint8_t enroll_result = Flash_AddID(card_id);
        g_nfc_enroll_state = 2;
        
        if(enroll_result == 0) {
            g_nfc_enroll_result = 1; // 成功
            screen8_show_nfc_popup("NFC添加成功");
        } else if(enroll_result == 1) {
            g_nfc_enroll_result = 2; // 存储空间已满
            screen8_show_nfc_popup("存储空间已满");
        } else if(enroll_result == 2) {
            g_nfc_enroll_result = 3; // 卡已存在
            screen8_show_nfc_popup("卡已存在");
        } else {
            g_nfc_enroll_result = 0; // 失败
            screen8_show_nfc_popup("NFC添加失败");
        }
    }
}

        if(g_screen5_need_init && g_app_scr == APP_SCR_5) {
            screen5_hide_error_label();
        }
    }
}

// NFC录入处理函数（非阻塞方式）
static void screen8_show_nfc_enroll_popup(void)
{
    // 显示"正在录入中..."弹窗
    screen8_show_nfc_popup("NFC正在添加中...");
    
    // 开始NFC录入流程
    g_nfc_enroll_start_time = HAL_GetTick();
    g_nfc_enroll_state = 1; // 录入中状态
    g_nfc_enroll_result = 0;
}

// NFC录入状态处理（在主循环中调用）
static void screen8_handle_nfc_enroll(void)
{
    if(g_nfc_enroll_state != 1) return;
    
    uint32_t current_time = HAL_GetTick();
    
    // 检查是否超时（10秒）
    if((current_time - g_nfc_enroll_start_time) >= 10000) {
        // 超时未检测到NFC卡
        g_nfc_enroll_state = 2;
        g_nfc_enroll_result = 0; // 失败
        screen8_show_nfc_popup("NFC添加失败");
        return;
    }
    
    // 尝试检测NFC卡
    uint8_t card_id[4] = {0};
    uint8_t detect_result = NFC_Enroll_Detect(card_id, 100); // 检测100ms
    
    if(detect_result == 1) {
        // 检测到NFC卡，尝试录入到Flash
        uint8_t enroll_result = Flash_AddID(card_id);
        g_nfc_enroll_state = 2;
        
        if(enroll_result == 0) {
            g_nfc_enroll_result = 1; // 成功
            screen8_show_nfc_popup("NFC添加成功");
        } else if(enroll_result == 1) {
            g_nfc_enroll_result = 2; // 存储空间已满
            screen8_show_nfc_popup("存储空间已满");
        } else if(enroll_result == 2) {
            g_nfc_enroll_result = 3; // 卡已存在
            screen8_show_nfc_popup("卡已存在");
        } else {
            g_nfc_enroll_result = 0; // 失败
            screen8_show_nfc_popup("NFC添加失败");
        }
    }
}

        if(g_screen5_need_init && g_app_scr == APP_SCR_5) {
            screen5_hide_error_label();
        }
    }
}

// NFC录入处理函数（非阻塞方式）
static void screen8_show_nfc_enroll_popup(void)
{
    // 显示"正在录入中..."弹窗
    screen8_show_nfc_popup("NFC正在添加中...");
    
    // 开始NFC录入流程
    g_nfc_enroll_start_time = HAL_GetTick();
    g_nfc_enroll_state = 1; // 录入中状态
    g_nfc_enroll_result = 0;
}

// NFC录入状态处理（在主循环中调用）
static void screen8_handle_nfc_enroll(void)
{
    if(g_nfc_enroll_state != 1) return;
    
    uint32_t current_time = HAL_GetTick();
    
    // 检查是否超时（10秒）
    if((current_time - g_nfc_enroll_start_time) >= 10000) {
        // 超时未检测到NFC卡
        g_nfc_enroll_state = 2;
        g_nfc_enroll_result = 0; // 失败
        screen8_show_nfc_popup("NFC添加失败");
        return;
    }
    
    // 尝试检测NFC卡
    uint8_t card_id[4] = {0};
    uint8_t detect_result = NFC_Enroll_Detect(card_id, 100); // 检测100ms
    
    if(detect_result == 1) {
        // 检测到NFC卡，尝试录入到Flash
        uint8_t enroll_result = Flash_AddID(card_id);
        g_nfc_enroll_state = 2;
        
        if(enroll_result == 0) {
            g_nfc_enroll_result = 1; // 成功
            screen8_show_nfc_popup("NFC添加成功");
        } else if(enroll_result == 1) {
            g_nfc_enroll_result = 2; // 存储空间已满
            screen8_show_nfc_popup("存储空间已满");
        } else if(enroll_result == 2) {
            g_nfc_enroll_result = 3; // 卡已存在
            screen8_show_nfc_popup("卡已存在");
        } else {
            g_nfc_enroll_result = 0; // 失败
            screen8_show_nfc_popup("NFC添加失败");
        }
    }
}

        if(g_screen5_need_init && g_app_scr == APP_SCR_5) {
            screen5_hide_error_label();
        }
    }
}

// NFC录入处理函数（非阻塞方式）
static void screen8_show_nfc_enroll_popup(void)
{
    // 显示"正在录入中..."弹窗
    screen8_show_nfc_popup("NFC正在添加中...");
    
    // 开始NFC录入流程
    g_nfc_enroll_start_time = HAL_GetTick();
    g_nfc_enroll_state = 1; // 录入中状态
    g_nfc_enroll_result = 0;
}

// NFC录入状态处理（在主循环中调用）
static void screen8_handle_nfc_enroll(void)
{
    if(g_nfc_enroll_state != 1) return;
    
    uint32_t current_time = HAL_GetTick();
    
    // 检查是否超时（10秒）
    if((current_time - g_nfc_enroll_start_time) >= 10000) {
        // 超时未检测到NFC卡
        g_nfc_enroll_state = 2;
        g_nfc_enroll_result = 0; // 失败
        screen8_show_nfc_popup("NFC添加失败");
        return;
    }
    
    // 尝试检测NFC卡
    uint8_t card_id[4] = {0};
    uint8_t detect_result = NFC_Enroll_Detect(card_id, 100); // 检测100ms
    
    if(detect_result == 1) {
        // 检测到NFC卡，尝试录入到Flash
        uint8_t enroll_result = Flash_AddID(card_id);
        g_nfc_enroll_state = 2;
        
        if(enroll_result == 0) {
            g_nfc_enroll_result = 1; // 成功
            screen8_show_nfc_popup("NFC添加成功");
        } else if(enroll_result == 1) {
            g_nfc_enroll_result = 2; // 存储空间已满
            screen8_show_nfc_popup("存储空间已满");
        } else if(enroll_result == 2) {
            g_nfc_enroll_result = 3; // 卡已存在
            screen8_show_nfc_popup("卡已存在");
        } else {
            g_nfc_enroll_result = 0; // 失败
            screen8_show_nfc_popup("NFC添加失败");
        }
    }
}

        if(g_screen5_need_init && g_app_scr == APP_SCR_5) {
            screen5_hide_error_label();
        }
    }
}

// NFC录入处理函数（非阻塞方式）
static void screen8_show_nfc_enroll_popup(void)
{
    // 显示"正在录入中..."弹窗
    screen8_show_nfc_popup("NFC正在添加中...");
    
    // 开始NFC录入流程
    g_nfc_enroll_start_time = HAL_GetTick();
    g_nfc_enroll_state = 1; // 录入中状态
    g_nfc_enroll_result = 0;
}

// NFC录入状态处理（在主循环中调用）
static void screen8_handle_nfc_enroll(void)
{
    if(g_nfc_enroll_state != 1) return;
    
    uint32_t current_time = HAL_GetTick();
    
    // 检查是否超时（10秒）
    if((current_time - g_nfc_enroll_start_time) >= 10000) {
        // 超时未检测到NFC卡
        g_nfc_enroll_state = 2;
        g_nfc_enroll_result = 0; // 失败
        screen8_show_nfc_popup("NFC添加失败");
        return;
    }
    
    // 尝试检测NFC卡
    uint8_t card_id[4] = {0};
    uint8_t detect_result = NFC_Enroll_Detect(card_id, 100); // 检测100ms
    
    if(detect_result == 1) {
        // 检测到NFC卡，尝试录入到Flash
        uint8_t enroll_result = Flash_AddID(card_id);
        g_nfc_enroll_state = 2;
        
        if(enroll_result == 0) {
            g_nfc_enroll_result = 1; // 成功
            screen8_show_nfc_popup("NFC添加成功");
        } else if(enroll_result == 1) {
            g_nfc_enroll_result = 2; // 存储空间已满
            screen8_show_nfc_popup("存储空间已满");
        } else if(enroll_result == 2) {
            g_nfc_enroll_result = 3; // 卡已存在
            screen8_show_nfc_popup("卡已存在");
        } else {
            g_nfc_enroll_result = 0; // 失败
            screen8_show_nfc_popup("NFC添加失败");
        }
    }
}

        if(g_screen5_need_init && g_app_scr == APP_SCR_5) {
            screen5_hide_error_label();
        }
    }
}

// NFC录入处理函数（非阻塞方式）
static void screen8_show_nfc_enroll_popup(void)
{
    // 显示"正在录入中..."弹窗
    screen8_show_nfc_popup("NFC正在添加中...");
    
    // 开始NFC录入流程
    g_nfc_enroll_start_time = HAL_GetTick();
    g_nfc_enroll_state = 1; // 录入中状态
    g_nfc_enroll_result = 0;
}

// NFC录入状态处理（在主循环中调用）
static void screen8_handle_nfc_enroll(void)
{
    if(g_nfc_enroll_state != 1) return;
    
    uint32_t current_time = HAL_GetTick();
    
    // 检查是否超时（10秒）
    if((current_time - g_nfc_enroll_start_time) >= 10000) {
        // 超时未检测到NFC卡
        g_nfc_enroll_state = 2;
        g_nfc_enroll_result = 0; // 失败
        screen8_show_nfc_popup("NFC添加失败");
        return;
    }
    
    // 尝试检测NFC卡
    uint8_t card_id[4] = {0};
    uint8_t detect_result = NFC_Enroll_Detect(card_id, 100); // 检测100ms
    
    if(detect_result == 1) {
        // 检测到NFC卡，尝试录入到Flash
        uint8_t enroll_result = Flash_AddID(card_id);
        g_nfc_enroll_state = 2;
        
        if(enroll_result == 0) {
            g_nfc_enroll_result = 1; // 成功
            screen8_show_nfc_popup("NFC添加成功");
        } else if(enroll_result == 1) {
            g_nfc_enroll_result = 2; // 存储空间已满
            screen8_show_nfc_popup("存储空间已满");
        } else if(enroll_result == 2) {
            g_nfc_enroll_result = 3; // 卡已存在
            screen8_show_nfc_popup("卡已存在");
        } else {
            g_nfc_enroll_result = 0; // 失败
            screen8_show_nfc_popup("NFC添加失败");
        }
    }
}

        if(g_screen5_need_init && g_app_scr == APP_SCR_5) {
            screen5_hide_error_label();
        }
    }
}

// NFC录入处理函数（非阻塞方式）
static void screen8_show_nfc_enroll_popup(void)
{
    // 显示"正在录入中..."弹窗
    screen8_show_nfc_popup("NFC正在添加中...");
    
    // 开始NFC录入流程
    g_nfc_enroll_start_time = HAL_GetTick();
    g_nfc_enroll_state = 1; // 录入中状态
    g_nfc_enroll_result = 0;
}

// NFC录入状态处理（在主循环中调用）
static void screen8_handle_nfc_enroll(void)
{
    if(g_nfc_enroll_state != 1) return;
    
    uint32_t current_time = HAL_GetTick();
    
    // 检查是否超时（10秒）
    if((current_time - g_nfc_enroll_start_time) >= 10000) {
        // 超时未检测到NFC卡
        g_nfc_enroll_state = 2;
        g_nfc_enroll_result = 0; // 失败
        screen8_show_nfc_popup("NFC添加失败");
        return;
    }
    
    // 尝试检测NFC卡
    uint8_t card_id[4] = {0};
    uint8_t detect_result = NFC_Enroll_Detect(card_id, 100); // 检测100ms
    
    if(detect_result == 1) {
        // 检测到NFC卡，尝试录入到Flash
        uint8_t enroll_result = Flash_AddID(card_id);
        g_nfc_enroll_state = 2;
        
        if(enroll_result == 0) {
            g_nfc_enroll_result = 1; // 成功
            screen8_show_nfc_popup("NFC添加成功");
        } else if(enroll_result == 1) {
            g_nfc_enroll_result = 2; // 存储空间已满
            screen8_show_nfc_popup("存储空间已满");
        } else if(enroll_result == 2) {
            g_nfc_enroll_result = 3; // 卡已存在
            screen8_show_nfc_popup("卡已存在");
        } else {
            g_nfc_enroll_result = 0; // 失败
            screen8_show_nfc_popup("NFC添加失败");
        }
    }
}

        if(g_screen5_need_init && g_app_scr == APP_SCR_5) {
            screen5_hide_error_label();
            if(lv_obj_is_valid(guider_ui.screen_5_ta_1)) {
                if(g_screen5_found_acc[0] != '\0') {
                    lv_textarea_set_text(guider_ui.screen_5_ta_1, g_screen5_found_acc);
                } else {
                    lv_textarea_set_text(guider_ui.screen_5_ta_1, "");
                }
                lv_textarea_set_max_length(guider_ui.screen_5_ta_1, 12);
            }
            screen5_set_field_selected();
            g_screen5_need_init = 0u;
        }
        if(g_screen7_need_init && lv_scr_act() == guider_ui.screen_7) {
            g_screen7_choice_yes = 1u;
            screen7_hide_all_msgbox();
            if(lv_obj_is_valid(guider_ui.screen_7_ta_1)) {
                lv_textarea_set_text(guider_ui.screen_7_ta_1, "");
                lv_textarea_set_max_length(guider_ui.screen_7_ta_1, 12);
            }
            screen7_set_field_selected();
            g_screen7_need_init = 0u;
        }
        key_ui_handle();
        screen1_cursor_blink_handle();
        screen2_cursor_blink_handle();
        screen5_cursor_blink_handle();
        screen6_dlg_cursor_blink_handle();
        screen7_cursor_blink_handle();
        screen8_cursor_blink_handle();
        delay_ms(5);
        lv_timer_handler();
    }
}
#endif
