#ifndef APP_SCREEN7_FLOW_H
#define APP_SCREEN7_FLOW_H

#include <stdint.h>
#include "lvgl.h"
#include "./BSP/KEY/key.h"

void screen7_hide_all_msgbox(void);
void screen7_popup_btn_style(lv_obj_t *btn, lv_obj_t *lbl, uint8_t selected);
void screen7_set_msgbox1_choice(uint8_t yes_selected);
void screen7_show_msgbox1(void);
void screen7_show_msgbox2(void);
void screen7_show_msgbox3(void);
void screen7_set_field_selected(void);
void screen7_handle_input_key(KeyValue_t key);
void screen7_cursor_blink_handle(void);

#endif
