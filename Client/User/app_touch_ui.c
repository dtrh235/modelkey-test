#include "app_touch_ui.h"

#include "app_config.h"

#if APP_RS485_IS_SLAVE

#include "./BSP/TOUCH/touch.h"
#include "./BSP/KEY/key.h"
#include "app_key_ui.h"
#include "app_screen.h"
#include "app_screen_auth.h"
#include "app_state.h"
#include "app_touch_diag.h"

#include "lvgl.h"
#include "gui_guider.h"

#define APP_TOUCH_HIT_PAD  6

static app_scr_t s_touch_scr = APP_SCR_1;
static uint8_t s_touch_was_down = 0u;
static lv_coord_t s_touch_last_x;
static lv_coord_t s_touch_last_y;

static bool app_touch_hit(lv_obj_t *obj, lv_coord_t x, lv_coord_t y);
static void app_touch_key_once(KeyValue_t key);

void app_touch_diag_log_init_once(void)
{
    static uint8_t s_done = 0u;

    if(s_done != 0u) {
        return;
    }
    s_done = 1u;
    TOUCH_LOG("diag init cap=%u touchtype=0x%02X",
              (unsigned)((tp_dev.touchtype & 0x80u) ? 1u : 0u),
              (unsigned)tp_dev.touchtype);
}

static void app_touch_auth_screen(lv_coord_t x, lv_coord_t y,
                                  lv_obj_t *ta_acc, lv_obj_t *ta_pwd,
                                  lv_obj_t *btn_ok, lv_obj_t *btn_esc,
                                  void (*set_field_fn)(uint8_t))
{
    if(app_touch_hit(btn_esc, x, y)) {
        TOUCH_LOG("release hit=ESC xy=%d,%d", (int)x, (int)y);
        app_touch_key_once(KEY_ESC);
        return;
    }
    if(app_touch_hit(btn_ok, x, y)) {
        TOUCH_LOG("release hit=OK xy=%d,%d", (int)x, (int)y);
        app_touch_key_once(KEY_OK);
        return;
    }
    if(app_touch_hit(ta_acc, x, y)) {
        TOUCH_LOG("release hit=ACC xy=%d,%d", (int)x, (int)y);
        if(set_field_fn != NULL) {
            set_field_fn(0u);
        }
        return;
    }
    if(app_touch_hit(ta_pwd, x, y)) {
        TOUCH_LOG("release hit=PWD xy=%d,%d", (int)x, (int)y);
        if(set_field_fn != NULL) {
            set_field_fn(1u);
        }
        return;
    }
    TOUCH_LOG("release miss xy=%d,%d", (int)x, (int)y);
}

static bool app_touch_hit(lv_obj_t *obj, lv_coord_t x, lv_coord_t y)
{
    lv_area_t a;
    lv_point_t p;

    if(obj == NULL || !lv_obj_is_valid(obj)) {
        return false;
    }
    lv_obj_get_coords(obj, &a);
    a.x1 -= APP_TOUCH_HIT_PAD;
    a.y1 -= APP_TOUCH_HIT_PAD;
    a.x2 += APP_TOUCH_HIT_PAD;
    a.y2 += APP_TOUCH_HIT_PAD;
    p.x = x;
    p.y = y;
    return _lv_area_is_point_on(&a, &p, 0);
}

static void app_touch_key_once(KeyValue_t key)
{
    app_key_ui_dispatch(key);
}

static void app_touch_process_release(lv_coord_t x, lv_coord_t y)
{
    if(g_app_scr != APP_SCR_1) {
        return;
    }
    if(g_screen1_unlock_popup != NULL && lv_obj_is_valid(g_screen1_unlock_popup)) {
        return;
    }
    if(lv_scr_act() != guider_ui.screen_1) {
        return;
    }
    app_touch_auth_screen(x, y,
                          guider_ui.screen_1_ta_1, guider_ui.screen_1_ta_2,
                          guider_ui.screen_1_btn_2, guider_ui.screen_1_btn_3,
                          screen1_set_field_selected);
}

void app_touch_ui_handle(void)
{
    uint8_t down;
    lv_coord_t cx;
    lv_coord_t cy;

    app_touch_diag_log_init_once();

    tp_dev.scan(0);
    down = (tp_dev.sta & TP_PRES_DOWN) ? 1u : 0u;

    if(g_app_scr != s_touch_scr) {
        s_touch_scr = g_app_scr;
        s_touch_was_down = 0u;
    }

    if(g_app_scr != APP_SCR_1) {
        return;
    }

    if(down) {
        cx = (lv_coord_t)tp_dev.x[0];
        cy = (lv_coord_t)tp_dev.y[0];
        if(!s_touch_was_down) {
            s_touch_last_x = cx;
            s_touch_last_y = cy;
        } else {
            s_touch_last_x = cx;
            s_touch_last_y = cy;
        }
        s_touch_was_down = 1u;
        return;
    }

    if(s_touch_was_down) {
        app_touch_process_release(s_touch_last_x, s_touch_last_y);
        s_touch_was_down = 0u;
    }
}

#else

void app_touch_ui_handle(void)
{
}

#endif
