#ifndef UI_AUTH_INPUT_H
#define UI_AUTH_INPUT_H

#include <stdint.h>
#include "lvgl.h"
#include "./BSP/KEY/key.h"

lv_obj_t *ui_auth_get_field_by_index(lv_obj_t *first_field, lv_obj_t *second_field, uint8_t idx);
uint16_t ui_auth_get_field_max_len(uint8_t idx);
void ui_auth_update_cursor_pos(lv_obj_t *cursor, lv_obj_t *ta);
void ui_auth_handle_input_key(lv_obj_t *ta, KeyValue_t key, uint16_t max_len,
                              void (*hide_error_cb)(void),
                              void (*update_cursor_cb)(void));

#endif
