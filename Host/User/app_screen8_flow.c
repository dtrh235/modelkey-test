#include "app_screen8_flow.h"

#include <stdbool.h>
#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "ui.h"
#include "app_screen.h"
#include "app_user_ops.h"
#include "app_screen8_popup.h"
#include "app_screen8_focus.h"

LV_FONT_DECLARE(lv_font_montserratMedium_16);
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20);

extern lv_ui guider_ui;
extern volatile app_scr_t g_app_scr;
extern lv_obj_t *g_screen8_popup;
extern lv_timer_t *g_screen8_result_timer;
extern uint8_t g_screen8_chip_read_ok;
extern uint8_t g_nfc_pending_uid_valid;
extern uint8_t g_nfc_pending_uid[4];
extern uint8_t g_fp_pending_page_valid;
extern uint16_t g_fp_pending_page_id;
extern uint8_t g_fp_pending_page2_valid;
extern uint16_t g_fp_pending_page_id_2;
extern int g_nfc_op;
extern uint8_t g_screen8_fp_enroll_state;
extern uint8_t g_screen8_fp_enroll_result;
extern uint32_t g_screen8_fp_enroll_start_time;
extern lv_obj_t *g_screen8_cursor;
extern uint32_t HAL_GetTick(void);

static uint8_t s_screen8_success_popup_pending = 0u;

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
        if(g_screen8_result_timer != NULL) lv_timer_del(g_screen8_result_timer);
        g_screen8_result_timer = lv_timer_create(screen8_popup_close_and_back_timer_cb, 1200, NULL);
        if(g_screen8_result_timer != NULL) lv_timer_set_repeat_count(g_screen8_result_timer, 1);
        s_screen8_success_popup_pending = 1u;
    } else {
        s_screen8_success_popup_pending = 0u;
    }
}

bool screen8_has_valid_acc_pwd_input(void)
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

void screen8_attempt_commit(void)
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
            /* 第二页绑定失败不撤销用户（单页即可开锁）。 */
            if(ok && g_fp_pending_page2_valid) {
                (void)users_bind_fp_by_acc(acc, g_fp_pending_page_id_2);
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

uint8_t screen8_add_user_success_popup_active(void)
{
    return s_screen8_success_popup_pending;
}

void screen8_add_user_success_popup_clear(void)
{
    s_screen8_success_popup_pending = 0u;
}

void screen8_success_popup_poll(void)
{
    /* 仅 lv_timer 自动返回；此处不再二次切屏，避免与 timer 竞态 */
}

void enter_screen_8(void)
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
    if(lv_obj_is_valid(guider_ui.screen_8_cb_1)) lv_obj_clear_state(guider_ui.screen_8_cb_1, LV_STATE_CHECKED);

    screen8_hide_status_labels();
    g_screen8_chip_read_ok = 0u;
    g_nfc_pending_uid_valid = 0u;
    memset(g_nfc_pending_uid, 0, sizeof(g_nfc_pending_uid));
    g_fp_pending_page_valid = 0u;
    g_fp_pending_page_id = 0xFFFFu;
    g_fp_pending_page2_valid = 0u;
    g_fp_pending_page_id_2 = 0xFFFFu;
    g_nfc_op = 0;
    g_screen8_fp_enroll_state = 0u;
    g_screen8_fp_enroll_result = 0u;
    g_screen8_fp_enroll_start_time = 0u;
    g_screen8_popup = NULL;
    g_screen8_cursor = NULL;
    s_screen8_success_popup_pending = 0u;
    screen8_set_focus(0);
}
