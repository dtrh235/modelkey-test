#ifndef APP_SCREEN_PAIR_FLOW_H
#define APP_SCREEN_PAIR_FLOW_H

#include <stdint.h>
#include "lvgl.h"

void enter_screen_pair(void);
void screen_pair_exit_to_menu(void);
uint8_t screen_pair_is_open(void);
void screen_pair_refresh(void);
void screen_pair_on_regen(void);
uint8_t screen_pair_hit_regen(lv_coord_t x, lv_coord_t y);

#endif
