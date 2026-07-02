#include "app_ui_v3.h"

#if (APP_UI_V3_ENABLE == 1)

#include "lvgl.h"
#include "app_ui_v3_theme.h"
#include "app_ui_v3_screens.h"
#include "app_ui_v3_widgets.h"
#include "app_ui_v3_gesture.h"
#include "app_ui_v3_users.h"
#include "app_ui_v3_diag.h"
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

ui3_state_t *ui3_state(void)
{
    return &s_st;
}

static void ui3_load(ui3_scr_id_t id)
{
    lv_obj_t *new_scr;
    lv_obj_t *old_scr = lv_scr_act();
    lv_disp_t *disp = lv_disp_get_default();

    ui3_diag_nav("load_begin", id, 0u);
    ui3_diag_mem("pre_load");

    s_st.scr = id;
    if(id != UI3_SCR_ADD_USER && id != UI3_SCR_MENU) {
        s_st.scroll_px = 0;
    }

    /* 40 行双缓冲下滑动切屏会一顿一顿；改为即时切屏 */
    if(disp != NULL && disp->scr_to_load != NULL) {
        lv_anim_del(disp->scr_to_load, NULL);
        if(old_scr != NULL) {
            lv_anim_del(old_scr, NULL);
        }
        disp->scr_to_load = NULL;
    }

    new_scr = ui3_build_screen(&s_st);
    if(new_scr == NULL) {
        ui3_diag_fmt("load", "FAIL build_screen returned NULL");
        ui3_diag_mem("load_fail");
        return;
    }
    lv_scr_load(new_scr);
    if(old_scr != NULL && old_scr != new_scr) {
        lv_obj_del(old_scr);
    }
    ui3_diag_scr("load_ok", id, new_scr);
    ui3_diag_mem("post_load");
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
        ui3_diag_nav("pump_to", s_nav_defer.id, s_nav_defer.push);
        ui3_nav_to_impl(s_nav_defer.id, s_nav_defer.push != 0u);
        break;
    case UI3_NAV_OP_BACK:
        ui3_diag_str("[UI3][nav] pump_back\r\n");
        ui3_nav_back_impl();
        break;
    case UI3_NAV_OP_RELOAD:
        ui3_diag_nav("pump_reload", s_st.scr, 0u);
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

void ui3_reload_current(void)
{
    ui3_nav_schedule(UI3_NAV_OP_RELOAD, s_st.scr, false);
}

void app_ui_v3_init(void)
{
    ui3_diag_str("[UI3] app_ui_v3_init begin\r\n");
    ui3_diag_mem("init0");
    memset(&s_st, 0, sizeof(s_st));
    s_st.scr = UI3_SCR_HOME;
    s_st.stack_len = 0;
    s_st.menu_index = 0;
    s_st.mqtt_online = 1u;
    s_st.wifi_index = 0;
    s_st.edit_acc[0] = '\0';
    s_st.home_nav_sel = UI3_HOME_NAV_NONE;
    (void)strncpy(s_st.pair_code, "384729", sizeof(s_st.pair_code));
    srand((unsigned)HAL_GetTick());
    ui3_theme_init();
    ui3_diag_mem("post_theme");
    ui3_load(UI3_SCR_HOME);
    ui3_theme_apply_disp_bg();
    if(lv_scr_act() != NULL) {
        lv_obj_set_style_opa(lv_scr_act(), LV_OPA_COVER, 0);
        lv_obj_invalidate(lv_scr_act());
    }
    lv_timer_handler();
    lv_refr_now(lv_disp_get_default());
    s_clock_ms = HAL_GetTick();
    ui3_diag_boot_done();
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
}

void app_ui_v3_poll(void)
{
    ui3_diag_gui_first_poll();
    ui3_gesture_poll(&s_st);
    ui3_users_enroll_poll();
    ui3_nav_pump();
    ui3_diag_gui_heartbeat(s_st.scr);
}

#endif
