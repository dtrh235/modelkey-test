#ifndef UI_SCREEN8_UTILS_H
#define UI_SCREEN8_UTILS_H

#include <stdint.h>
#include "lvgl.h"

lv_obj_t *ui_screen8_focus_get_ta(uint8_t focus, lv_obj_t *ta1, lv_obj_t *ta2);
void ui_screen8_hide_status_labels(lv_obj_t *label_5, lv_obj_t *label_6);
void ui_screen8_clear_error_label(lv_obj_t *label_5);
void ui_screen8_style_ta(lv_obj_t *ta, uint8_t selected);
void ui_screen8_style_btn(lv_obj_t *btn, lv_obj_t *lbl, uint8_t selected);

#endif
