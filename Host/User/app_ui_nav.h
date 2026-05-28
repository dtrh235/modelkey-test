#ifndef APP_UI_NAV_H
#define APP_UI_NAV_H

#include <stdint.h>

/* LVGL 切屏期间置位，Storage/RS485 让路，避免 Flash 擦写与切屏抢资源导致假死 */
void app_ui_nav_begin(uint8_t from_scr);
void app_ui_nav_end(uint8_t to_scr);
uint8_t app_ui_nav_is_busy(void);

#endif
