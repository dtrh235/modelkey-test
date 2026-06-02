#ifndef APP_SCREEN4_TABLE_H
#define APP_SCREEN4_TABLE_H

#include "lvgl.h"
#include "./BSP/KEY/key.h"

void screen4_refresh_table(void);
void screen4_handle_table_key(KeyValue_t key);
void screen4_list_scroll_by(lv_coord_t dy);
uint8_t screen4_point_in_list(lv_coord_t x, lv_coord_t y);

#endif
