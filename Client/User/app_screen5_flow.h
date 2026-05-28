#ifndef APP_SCREEN5_FLOW_H
#define APP_SCREEN5_FLOW_H

#include <stdbool.h>
#include "./BSP/KEY/key.h"

void screen5_hide_error_label(void);
void screen5_show_error_label(void);
void screen5_set_field_selected(void);
void screen5_handle_input_key(KeyValue_t key);
bool screen5_try_lookup_acc(void);
void screen5_cursor_blink_handle(void);
void enter_screen_5(void);

#endif
