#include "app_screen8_popup.h"

#include <string.h>

#include "gui_guider.h"
#include "ui.h"
#include "app_screen.h"
#include "app_screen3_menu.h"
#include "app_screen8_flow.h"
#include "app_screen8_enroll.h"
#include "app_config.h"
#include "app_ui_nav.h"
#include "app_state.h"
#include "app_key_ui.h"
#include "./BSP/KEY/key.h"
#include "./SYSTEM/sys/sys.h"

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20);

extern lv_ui guider_ui;
extern lv_obj_t *g_screen8_cursor;
extern lv_obj_t *g_screen8_popup;
extern uint8_t g_nfc_enroll_state;
extern uint8_t g_screen8_fp_enroll_state;
extern lv_timer_t *g_screen8_result_timer;
extern volatile app_scr_t g_app_scr;

static lv_obj_t *s_screen8_popup_btn = NULL;

#define SCREEN8_POPUP_HIT_PAD  6

static bool screen8_popup_hit_btn(lv_coord_t x, lv_coord_t y)
{
    lv_area_t a;
    lv_point_t p;

    if(s_screen8_popup_btn == NULL || !lv_obj_is_valid(s_screen8_popup_btn)) {
        return false;
    }
    lv_obj_get_coords(s_screen8_popup_btn, &a);
    a.x1 -= SCREEN8_POPUP_HIT_PAD;
    a.y1 -= SCREEN8_POPUP_HIT_PAD;
    a.x2 += SCREEN8_POPUP_HIT_PAD;
    a.y2 += SCREEN8_POPUP_HIT_PAD;
    p.x = x;
    p.y = y;
    return _lv_area_is_point_on(&a, &p, 0);
}

bool screen8_try_read_chip(void)
{
    return true;
}

void screen8_popup_close_and_back(void)
{
    if(g_screen8_result_timer != NULL) {
        lv_timer_del(g_screen8_result_timer);
        g_screen8_result_timer = NULL;
    }
    if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
        lv_obj_del(g_screen8_popup);
    }
    g_screen8_popup = NULL;
    s_screen8_popup_btn = NULL;
    g_screen8_cursor = NULL;
    g_screen3_pending_index = 0u;
    g_screen3_need_init = 1u;
    app_ui_nav_begin((uint8_t)APP_SCR_8);
    ui_load_scr_animation(&guider_ui, &guider_ui.screen_3, guider_ui.screen_3_del, &guider_ui.screen_8_del,
                          setup_scr_screen_3, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_3;
    screen8_add_user_success_popup_clear();
    app_ui_nav_end((uint8_t)APP_SCR_3);
    screen3_set_menu_selected(0);
}

void screen8_popup_close_and_back_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    screen8_popup_close_and_back();
}

void screen8_popup_close_only(void)
{
    if(g_screen8_result_timer != NULL) {
        lv_timer_del(g_screen8_result_timer);
        g_screen8_result_timer = NULL;
    }
    if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
        lv_obj_del(g_screen8_popup);
    }
    g_screen8_popup = NULL;
    s_screen8_popup_btn = NULL;
}

uint8_t screen8_popup_touch(lv_coord_t x, lv_coord_t y)
{
    if(g_screen8_popup == NULL || !lv_obj_is_valid(g_screen8_popup)) {
        return 0u;
    }
    if(!screen8_popup_hit_btn(x, y)) {
        return 0u;
    }
    if(g_nfc_enroll_state == 1u || g_screen8_fp_enroll_state == 1u) {
        app_key_ui_dispatch(KEY_ESC);
    } else {
        app_key_ui_dispatch(KEY_OK);
    }
    return 1u;
}

void screen8_popup_close_event_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        screen8_popup_close_and_back();
    }
}

static void screen8_enroll_popup_ok_cb(lv_event_t *e)
{
    (void)e;
    if(g_nfc_enroll_state == 1u || g_screen8_fp_enroll_state == 1u) {
        app_key_ui_dispatch(KEY_ESC);
        return;
    }
    app_key_ui_dispatch(KEY_OK);
}

void screen8_show_enroll_popup(const char *message)
{
#if (APP_UI_V3_ENABLE == 1)
    if(g_enroll_ui_v3_mode != 0u) {
        return;
    }
#endif
    bool is_progress = false;

    if(g_screen8_popup) {
        lv_obj_del(g_screen8_popup);
        g_screen8_popup = NULL;
    }
    s_screen8_popup_btn = NULL;

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

    s_screen8_popup_btn = btn;
    lv_obj_add_event_cb(btn, screen8_enroll_popup_ok_cb, LV_EVENT_CLICKED, NULL);

    if(!is_progress) {
        g_nfc_enroll_state = 0u;
        g_screen8_fp_enroll_state = 0u;
    }

    if(g_screen8_result_timer != NULL) {
        lv_timer_del(g_screen8_result_timer);
        g_screen8_result_timer = NULL;
    }
}
