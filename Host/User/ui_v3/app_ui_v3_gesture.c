#include "app_ui_v3_gesture.h"
#include "app_ui_v3_theme.h"
#include "app_ui_v3_widgets.h"
#include "app_ui_v3.h"
#include "app_ui_v3_anim.h"
#include "./BSP/TOUCH/touch.h"
#include "app_config.h"

#define UI3_GESTURE_SCROLL_THRESH  12
#define UI3_GESTURE_BACK_DX        48
#define UI3_GESTURE_HOME_DX        40

static lv_obj_t *s_track;
static lv_coord_t s_x0, s_y0, s_x1, s_y1;
static lv_coord_t s_last_x, s_last_y;
static uint8_t s_down;
static uint8_t s_moved;

static lv_obj_t *track_viewport(lv_obj_t *track)
{
    if(track == NULL) {
        return NULL;
    }
    return lv_obj_get_parent(track);
}

void ui3_gesture_bind(ui3_state_t *st, lv_obj_t *track)
{
    (void)st;
    s_track = track;
}

void ui3_scroll_apply(ui3_state_t *st, lv_obj_t *track, bool anim)
{
    lv_obj_t *vp;
    lv_coord_t y;

    if(st == NULL || track == NULL) {
        return;
    }
    vp = track_viewport(track);
    if(vp == NULL || !lv_obj_is_valid(vp)) {
        return;
    }
    y = (lv_coord_t)(st->scroll_px * st->scroll_row_h);
    lv_obj_scroll_to_y(vp, y, anim ? LV_ANIM_ON : LV_ANIM_OFF);
}

void ui3_gesture_sync_scroll_px(ui3_state_t *st, lv_obj_t *track)
{
    lv_obj_t *vp;
    lv_coord_t y;
    int16_t px;

    if(st == NULL || track == NULL || st->scroll_row_h == 0u) {
        return;
    }
    vp = track_viewport(track);
    if(vp == NULL || !lv_obj_is_valid(vp)) {
        return;
    }
    y = lv_obj_get_scroll_y(vp);
    px = (int16_t)((y + (st->scroll_row_h / 2)) / st->scroll_row_h);
    if(px < 0) {
        px = 0;
    }
    if(px > (int16_t)st->scroll_max) {
        px = (int16_t)st->scroll_max;
    }
    st->scroll_px = (uint8_t)px;
}

void ui3_gesture_poll(ui3_state_t *st)
{
    uint8_t down;
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t dx;
    lv_coord_t dy;
    lv_coord_t adx;
    lv_coord_t ady;

    if(ui3_modal_blocks_input() != 0u) {
        return;
    }

    /* touch 已在 lv_timer_handler -> touchpad_read 中采样，勿重复 scan */
    down = (tp_dev.sta & TP_PRES_DOWN) ? 1u : 0u;
    x = (lv_coord_t)tp_dev.x[0];
    y = (lv_coord_t)tp_dev.y[0];
#if (APP_TP_MIRROR_X != 0)
    x = (lv_coord_t)(UI3_W - 1 - x);
#endif

    if(down) {
        if(!s_down) {
            s_x0 = s_x1 = s_last_x = x;
            s_y0 = s_y1 = s_last_y = y;
            s_down = 1u;
            s_moved = 0u;
        } else {
            dx = (lv_coord_t)(x - s_last_x);
            dy = (lv_coord_t)(y - s_last_y);
            s_x1 = x;
            s_y1 = y;
            s_last_x = x;
            s_last_y = y;
            if(dx < 0) {
                dx = (lv_coord_t)(-dx);
            }
            if(dy < 0) {
                dy = (lv_coord_t)(-dy);
            }
            if((dx + dy) >= UI3_GESTURE_SCROLL_THRESH) {
                s_moved = 1u;
            }
        }
        return;
    }

    if(!s_down) {
        return;
    }
    s_down = 0u;

    if(st == NULL) {
        return;
    }

    dx = (lv_coord_t)(s_x1 - s_x0);
    dy = (lv_coord_t)(s_y1 - s_y0);
    adx = dx;
    ady = dy;
    if(adx < 0) {
        adx = (lv_coord_t)(-adx);
    }
    if(ady < 0) {
        ady = (lv_coord_t)(-ady);
    }

    if(s_moved != 0u && s_track != NULL) {
        ui3_gesture_sync_scroll_px(st, s_track);
    }

    if(st->scr == UI3_SCR_HOME && dx > UI3_GESTURE_HOME_DX && ady < UI3_GESTURE_HOME_DX) {
        st->home_nav_sel = 0u;
        ui3_home_footer_sel(st);
        return;
    }
    if(st->scr == UI3_SCR_HOME && dx < -(lv_coord_t)UI3_GESTURE_HOME_DX && ady < UI3_GESTURE_HOME_DX) {
        st->home_nav_sel = 1u;
        ui3_home_footer_sel(st);
        return;
    }

    if(s_moved != 0u) {
        return;
    }

    if(dx < -(lv_coord_t)UI3_GESTURE_BACK_DX && ady < UI3_GESTURE_BACK_DX && st->scr != UI3_SCR_HOME) {
        ui3_nav_back();
    }
}
