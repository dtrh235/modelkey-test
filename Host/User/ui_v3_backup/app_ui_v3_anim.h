#ifndef APP_UI_V3_ANIM_H
#define APP_UI_V3_ANIM_H

#include "lvgl.h"

void ui3_anim_y(lv_obj_t *obj, int32_t from, int32_t to, uint32_t ms);
void ui3_anim_zoom_breathe(lv_obj_t *obj, int32_t min_zoom, int32_t max_zoom, uint32_t ms);
void ui3_anim_opa_pulse(lv_obj_t *obj, lv_opa_t min_opa, lv_opa_t max_opa, uint32_t ms);
void ui3_anim_arc_rotate(lv_obj_t *arc, uint32_t period_ms);
void ui3_anim_screen_in(lv_obj_t *scr);
void ui3_show_unlock_ripple(lv_obj_t *parent);
void ui3_show_modal_result(lv_obj_t *parent, const char *title, const char *sub, bool success);
void ui3_show_toast(lv_obj_t *parent, const char *msg);
void ui3_show_scan_modal(lv_obj_t *parent, const char *title, const char *sub);
void ui3_show_fp_modal(lv_obj_t *parent, const char *done_title, bool success);
void ui3_show_nfc_modal(lv_obj_t *parent, const char *done_title, bool success);
void ui3_show_wifi_pwd_modal(lv_obj_t *parent, const char *ssid);

#endif
