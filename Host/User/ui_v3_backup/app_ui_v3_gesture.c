#include "app_ui_v3_gesture.h"
#include "app_ui_v3_anim.h"
#include "app_ui_v3_theme.h"
#include "app_ui_v3_widgets.h"
#include "app_ui_v3.h"
#include "./BSP/TOUCH/touch.h"
#include "app_config.h"

static lv_obj_t *s_track;
static lv_coord_t s_x0, s_y0, s_x1, s_y1;
static uint8_t s_down;

void ui3_gesture_bind(ui3_state_t *st, lv_obj_t *track)
{
    (void)st;
    s_track = track;
}

void ui3_scroll_apply(ui3_state_t *st, lv_obj_t *track, bool anim)
{
    int16_t y = -(int16_t)(st->scroll_px * st->scroll_row_h);
    if(anim) {
        ui3_anim_y(track, lv_obj_get_y(track), y, 280);
    } else {
        lv_obj_set_y(track, y);
    }
}

void ui3_gesture_poll(ui3_state_t *st)
{
    tp_dev.scan(0);
    if(tp_dev.sta & TP_PRES_DOWN) {
        lv_coord_t x = (lv_coord_t)tp_dev.x[0];
        lv_coord_t y = (lv_coord_t)tp_dev.y[0];
#if (APP_TP_MIRROR_X != 0)
        x = (lv_coord_t)(UI3_W - 1 - x);
#endif
        if(!s_down) {
            s_x0 = s_x1 = x;
            s_y0 = s_y1 = y;
            s_down = 1u;
        } else {
            s_x1 = x;
            s_y1 = y;
        }
        return;
    }

    if(!s_down) {
        return;
    }
    s_down = 0u;

    lv_coord_t dx = s_x1 - s_x0;
    lv_coord_t dy = s_y1 - s_y0;

    if(st == NULL) {
        return;
    }

    if(st->scr == UI3_SCR_HOME && dx > 40 && (dy > -40 && dy < 40)) {
        st->home_nav_sel = 0u;
        ui3_home_footer_sel(st);
        return;
    }
    if(st->scr == UI3_SCR_HOME && dx < -40 && (dy > -40 && dy < 40)) {
        st->home_nav_sel = 1u;
        ui3_home_footer_sel(st);
        return;
    }

    if(dx < -48 && (dy > -48 && dy < 48) && st->scr != UI3_SCR_HOME) {
        ui3_nav_back();
        return;
    }

    if(s_track == NULL || st->scroll_max == 0u) {
        return;
    }

    if((dy > 16 || dy < -16) && (dy > dx || dy < -dx)) {
        int8_t steps = (int8_t)(-dy / 42);
        if(steps == 0) {
            steps = (dy > 0) ? -1 : 1;
        }
        int16_t next = (int16_t)st->scroll_px + steps;
        if(next < 0) {
            next = 0;
        }
        if(next > (int16_t)st->scroll_max) {
            next = (int16_t)st->scroll_max;
        }
        if(next != (int16_t)st->scroll_px) {
            st->scroll_px = (uint8_t)next;
            ui3_scroll_apply(st, s_track, true);
        }
    }
}
