#ifndef APP_SCREEN3_MENU_H
#define APP_SCREEN3_MENU_H

#include <stdint.h>
#include "lvgl.h"

void screen3_init_scroll_layout(void);
void screen3_hide_menu_panel(void);
void screen3_show_menu_panel(void);
void screen3_apply_uniform_menu_style(void);
void screen3_set_menu_selected(uint8_t idx);
void screen3_scroll_to_item(uint8_t idx);
void screen3_list_scroll_by(lv_coord_t dy);
uint8_t screen3_point_in_menu_panel(lv_coord_t x, lv_coord_t y);
lv_obj_t *screen3_menu_btn_by_index(uint8_t idx);

#endif
