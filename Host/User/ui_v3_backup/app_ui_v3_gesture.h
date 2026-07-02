#ifndef APP_UI_V3_GESTURE_H
#define APP_UI_V3_GESTURE_H

#include "lvgl.h"
#include "app_ui_v3_types.h"

void ui3_gesture_bind(ui3_state_t *st, lv_obj_t *track);
void ui3_gesture_poll(ui3_state_t *st);
void ui3_scroll_apply(ui3_state_t *st, lv_obj_t *track, bool anim);

#endif
