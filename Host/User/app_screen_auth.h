#ifndef APP_SCREEN_AUTH_H
#define APP_SCREEN_AUTH_H

#include <stdint.h>
#include "./BSP/KEY/key.h"

void screen1_hide_error_label(void);
void screen1_show_error_label(void);
void screen1_handle_input_key(KeyValue_t key);
void screen1_set_field_selected(uint8_t idx);
void screen1_cursor_blink_handle(void);

void screen2_hide_error_label(void);
void screen2_show_error_label(void);
void screen2_handle_input_key(KeyValue_t key);
void screen2_set_field_selected(uint8_t idx);
void screen2_cursor_blink_handle(void);

#endif
