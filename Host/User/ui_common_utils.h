#ifndef UI_COMMON_UTILS_H
#define UI_COMMON_UTILS_H

#include <stdint.h>
#include "lvgl.h"
#include "./BSP/KEY/key.h"

int ui_key_to_digit(KeyValue_t key);
void ui_lock_btn_set_selected(lv_obj_t *lock_btn, lv_obj_t *menu_btn,
                              uint8_t *lock_selected, uint8_t *menu_selected,
                              uint8_t selected);
void ui_menu_btn_set_selected(lv_obj_t *lock_btn, lv_obj_t *menu_btn,
                              uint8_t *lock_selected, uint8_t *menu_selected,
                              uint8_t selected);
void ui_textarea_measure_text_run_width(lv_obj_t *ta, lv_coord_t *txt_w, uint32_t mask_buf_len);
void ui_label_set_hidden(lv_obj_t *label, uint8_t hidden);
void ui_cursor_blink_step(lv_obj_t *cursor, uint8_t *visible, uint32_t *last_ms, uint32_t period_ms);
void ui_textarea_apply_input_style(lv_obj_t *ta, uint8_t selected);
void ui_cursor_attach_bar(lv_obj_t **cursor, lv_obj_t *parent);
void ui_cursor_activate(lv_obj_t *cursor, uint8_t *visible, uint32_t *last_ms);

#endif
