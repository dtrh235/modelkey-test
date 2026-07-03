#include "app_ui_v3.h"

#if (APP_UI_V3_ENABLE == 1)

#include "lvgl.h"
#include "app_ui_v3_theme.h"
#include "app_ui_v3_screens.h"
#include "app_ui_v3_widgets.h"
#include "app_ui_v3_gesture.h"
#include "app_ui_v3_anim.h"
#include "app_ui_v3_users.h"
#include "app_ui_v3_services.h"
#include "app_state.h"
#include "app_screen.h"
#include "app_pair_bind.h"
#include "app_home_unlock.h"
#include "app_screen8_enroll.h"
#include "app_config.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdlib.h>

typedef enum {
    UI3_NAV_OP_TO = 0,
    UI3_NAV_OP_BACK,
    UI3_NAV_OP_RELOAD
} ui3_nav_op_t;

static ui3_state_t s_st;
static uint32_t s_clock_ms;
static uint32_t s_lockout_tick_ms;
static struct {
    ui3_nav_op_t op;
    ui3_scr_id_t id;
    uint8_t push;
    uint8_t pending;
} s_nav_defer;

typedef enum {
    UI3_POST_FB_NONE = 0,
    UI3_POST_FB_TOAST,
    UI3_POST_FB_MODAL
} ui3_post_fb_kind_t;

static struct {
    ui3_post_fb_kind_t kind;
    uint8_t modal_success;
    char title[40];
    char sub[48];
} s_post_fb;

ui3_state_t *ui3_state(void)
{
    return &s_st;
}

static void ui3_sync_legacy_scr(ui3_scr_id_t id)
{
    switch(id) {
    case UI3_SCR_HOME:
        g_app_scr = APP_SCR_HOME;
        break;
    case UI3_SCR_UNLOCK:
        g_app_scr = APP_SCR_1;
        break;
    case UI3_SCR_ADMIN:
        g_app_scr = APP_SCR_2;
        break;
    case UI3_SCR_MENU:
    case UI3_SCR_PAIR:
        g_app_scr = APP_SCR_3;
        break;
    case UI3_SCR_USER_LIST:
        g_app_scr = APP_SCR_4;
        break;
    case UI3_SCR_SEARCH:
        g_app_scr = APP_SCR_5;
        break;
    case UI3_SCR_EDIT_USER:
        g_app_scr = APP_SCR_6;
        break;
    case UI3_SCR_ADD_USER:
        g_app_scr = APP_SCR_8;
        break;
    case UI3_SCR_DEL_USER:
        g_app_scr = APP_SCR_7;
        break;
    case UI3_SCR_NFC_MGMT:
        g_app_scr = APP_SCR_9;
        break;
    case UI3_SCR_FP_MGMT:
        g_app_scr = APP_SCR_10;
        break;
    case UI3_SCR_WIFI:
        g_app_scr = APP_SCR_11;
        break;
    default:
        g_app_scr = APP_SCR_HOME;
        break;
    }
}

static void ui3_load(ui3_scr_id_t id)
{
    lv_obj_t *new_scr;
    lv_obj_t *old_scr = lv_scr_act();
    lv_disp_t *disp = lv_disp_get_default();

    ui3_scr_id_t prev = s_st.scr;

    s_st.scr = id;
    ui3_sync_legacy_scr(id);
    if(prev == UI3_SCR_WIFI && id != UI3_SCR_WIFI) {
        ui3_wifi_on_leave();
    }
    if(id == UI3_SCR_WIFI && prev != UI3_SCR_WIFI) {
        ui3_wifi_on_enter(&s_st);
    }
    if(id != UI3_SCR_ADD_USER && id != UI3_SCR_MENU) {
        if(prev != id) {
            s_st.scroll_px = 0;
        }
    }

    if(disp != NULL && disp->scr_to_load != NULL) {
        lv_anim_del(disp->scr_to_load, NULL);
        if(old_scr != NULL) {
            lv_anim_del(old_scr, NULL);
        }
        disp->scr_to_load = NULL;
    }

    new_scr = ui3_build_screen(&s_st);
    if(new_scr == NULL) {
        return;
    }
    ui3_users_on_screen_changing();
    lv_scr_load(new_scr);
    if(old_scr != NULL && old_scr != new_scr) {
        lv_obj_del(old_scr);
    }
}

static void ui3_nav_to_impl(ui3_scr_id_t id, bool push)
{
    if(push && s_st.stack_len < UI3_NAV_STACK_MAX) {
        s_st.stack[s_st.stack_len++] = s_st.scr;
    }
    ui3_load(id);
}

static void ui3_nav_back_impl(void)
{
    if(s_st.stack_len == 0u) {
        ui3_load(UI3_SCR_HOME);
        return;
    }
    ui3_scr_id_t prev = s_st.stack[--s_st.stack_len];
    ui3_load(prev);
}

static void ui3_reload_impl(void)
{
    ui3_load(s_st.scr);
}

static void ui3_nav_pump(void)
{
    if(s_nav_defer.pending == 0u) {
        return;
    }
    s_nav_defer.pending = 0u;
    switch(s_nav_defer.op) {
    case UI3_NAV_OP_TO:
        ui3_nav_to_impl(s_nav_defer.id, s_nav_defer.push != 0u);
        break;
    case UI3_NAV_OP_BACK:
        ui3_nav_back_impl();
        break;
    case UI3_NAV_OP_RELOAD:
        ui3_reload_impl();
        break;
    default:
        break;
    }
}

static void ui3_nav_schedule(ui3_nav_op_t op, ui3_scr_id_t id, bool push)
{
    s_nav_defer.op = op;
    s_nav_defer.id = id;
    s_nav_defer.push = push ? 1u : 0u;
    s_nav_defer.pending = 1u;
}

void ui3_nav_to(ui3_scr_id_t id, bool push)
{
    ui3_nav_schedule(UI3_NAV_OP_TO, id, push);
}

void ui3_nav_back(void)
{
    ui3_nav_schedule(UI3_NAV_OP_BACK, UI3_SCR_HOME, false);
}

void ui3_nav_home(void)
{
    s_st.stack_len = 0u;
    ui3_nav_schedule(UI3_NAV_OP_TO, UI3_SCR_HOME, false);
}

void ui3_nav_back_to_menu(void)
{
    uint8_t i;

    for(i = 0u; i < s_st.stack_len; i++) {
        if(s_st.stack[i] == UI3_SCR_MENU) {
            s_st.stack_len = (uint8_t)(i + 1u);
            break;
        }
    }
    ui3_nav_schedule(UI3_NAV_OP_TO, UI3_SCR_MENU, false);
}

void ui3_reload_current(void)
{
    ui3_nav_schedule(UI3_NAV_OP_RELOAD, s_st.scr, false);
}

void ui3_reload_sync(void)
{
    ui3_reload_impl();
}

void ui3_post_feedback_toast(const char *msg)
{
    if(msg == NULL) {
        return;
    }
    (void)strncpy(s_post_fb.title, msg, sizeof(s_post_fb.title) - 1u);
    s_post_fb.title[sizeof(s_post_fb.title) - 1u] = '\0';
    s_post_fb.sub[0] = '\0';
    s_post_fb.kind = UI3_POST_FB_TOAST;
    s_post_fb.modal_success = 0u;
}

void ui3_post_feedback_modal(const char *title, const char *sub, bool success)
{
    if(title == NULL) {
        return;
    }
    (void)strncpy(s_post_fb.title, title, sizeof(s_post_fb.title) - 1u);
    s_post_fb.title[sizeof(s_post_fb.title) - 1u] = '\0';
    if(sub != NULL) {
        (void)strncpy(s_post_fb.sub, sub, sizeof(s_post_fb.sub) - 1u);
    } else {
        s_post_fb.sub[0] = '\0';
    }
    s_post_fb.sub[sizeof(s_post_fb.sub) - 1u] = '\0';
    s_post_fb.kind = UI3_POST_FB_MODAL;
    s_post_fb.modal_success = success ? 1u : 0u;
}

static void ui3_post_feedback_pump(void)
{
    lv_obj_t *scr;

    if(s_post_fb.kind == UI3_POST_FB_NONE) {
        return;
    }
    scr = lv_scr_act();
    if(scr == NULL) {
        s_post_fb.kind = UI3_POST_FB_NONE;
        return;
    }
    if(s_post_fb.kind == UI3_POST_FB_TOAST) {
        ui3_show_toast(scr, s_post_fb.title);
    } else {
        ui3_show_modal_result(scr, s_post_fb.title,
                              s_post_fb.sub[0] != '\0' ? s_post_fb.sub : NULL,
                              s_post_fb.modal_success != 0u);
    }
    s_post_fb.kind = UI3_POST_FB_NONE;
}

void app_ui_v3_init(void)
{
    memset(&s_st, 0, sizeof(s_st));
    s_st.scr = UI3_SCR_HOME;
    g_app_scr = APP_SCR_HOME;
    s_st.mqtt_online = app_pair_cloud_ready() ? 1u : 0u;
    s_st.home_nav_sel = UI3_HOME_NAV_NONE;
    ui3_pair_sync(&s_st);
    srand((unsigned)HAL_GetTick());

    ui3_theme_init();
    ui3_load(UI3_SCR_HOME);
    ui3_theme_apply_disp_bg();

    if(lv_scr_act() != NULL) {
        lv_obj_set_style_opa(lv_scr_act(), LV_OPA_COVER, 0);
        lv_obj_invalidate(lv_scr_act());
    }
    lv_timer_handler();
    lv_refr_now(lv_disp_get_default());
    s_clock_ms = HAL_GetTick();
}

void ui3_wall_clock_refresh(void)
{
    if(s_st.lbl_top_time && lv_obj_is_valid(s_st.lbl_top_time)) {
        ui3_update_clock_label(s_st.lbl_top_time);
    }
    if(s_st.lbl_clock_big && lv_obj_is_valid(s_st.lbl_clock_big)) {
        ui3_update_clock_label(s_st.lbl_clock_big);
    }
    if(s_st.lbl_date && lv_obj_is_valid(s_st.lbl_date)) {
        ui3_update_date_label(s_st.lbl_date);
    }
}

void app_ui_v3_tick(void)
{
    uint32_t now = HAL_GetTick();

    if((now - s_clock_ms) >= 1000u) {
        s_clock_ms = now;
        if(s_st.lbl_top_time && lv_obj_is_valid(s_st.lbl_top_time)) {
            ui3_update_clock_label(s_st.lbl_top_time);
        }
        if(s_st.lbl_clock_big && lv_obj_is_valid(s_st.lbl_clock_big)) {
            ui3_update_clock_label(s_st.lbl_clock_big);
        }
        if(s_st.lbl_date && lv_obj_is_valid(s_st.lbl_date)) {
            ui3_update_date_label(s_st.lbl_date);
        }
        if(s_st.scr == UI3_SCR_UNLOCK && s_st.lockout_until_ms > now) {
            if((now - s_lockout_tick_ms) >= 1000u) {
                s_lockout_tick_ms = now;
                ui3_reload_current();
            }
        } else if(s_st.lockout_until_ms > 0u && s_st.lockout_until_ms <= now) {
            s_st.lockout_until_ms = 0u;
            s_st.unlock_errors = 0u;
            if(s_st.scr == UI3_SCR_UNLOCK) {
                ui3_reload_current();
            }
        }
    }
    ui3_caret_poll();
}

void app_ui_v3_poll(void)
{
    ui3_services_poll(&s_st);
    ui3_gesture_poll(&s_st);
#if (APP_NFC_ENABLE != 0)
    if(g_nfc_enroll_state == 1u && g_enroll_ui_v3_mode != 0u) {
        screen8_handle_nfc_enroll();
    }
#endif
    ui3_users_enroll_poll();
    ui3_nav_pump();
    ui3_post_feedback_pump();
    if(s_st.pending_welcome != 0u && s_st.scr == UI3_SCR_HOME && s_nav_defer.pending == 0u) {
        s_st.pending_welcome = 0u;
        app_home_show_unlock_popup();
    }
}

#include "app_ui_v3_services.c"

#endif
