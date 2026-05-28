#ifndef APP_SCREEN_WIFI_FLOW_H
#define APP_SCREEN_WIFI_FLOW_H

#include "lvgl.h"
#include "./BSP/KEY/key.h"

void enter_screen_wifi(void);
void screen_wifi_prepare_on_enter(void);
void screen_wifi_poll_tick(void);
void screen_wifi_gui_wake(void);
uint8_t screen_wifi_gui_work_pending(void);
uint8_t screen_wifi_popup_is_active(void);
void screen_wifi_popup_cancel(void);
void screen_wifi_popup_confirm(void);
void screen_wifi_handle_popup_input(KeyValue_t key);
void screen_wifi_list_move(int8_t delta);
void screen_wifi_list_select(uint8_t idx);
uint8_t screen_wifi_hit_row(lv_coord_t x, lv_coord_t y);
uint8_t screen_wifi_get_sel_index(void);
void screen_wifi_list_ok(void);
void screen_wifi_cursor_blink_handle(void);
void screen_wifi_exit_cleanup(void);
void screen_wifi_exit_to_menu(void);
uint8_t screen_wifi_point_in_list(lv_coord_t x, lv_coord_t y);
lv_obj_t *screen_wifi_refresh_btn_obj(void);
void screen_wifi_list_scroll_by(lv_coord_t dy);
uint8_t screen_wifi_list_lvgl_scrolling(void);
void screen_wifi_list_touch_begin(void);
uint8_t screen_wifi_hit_refresh_btn(lv_coord_t x, lv_coord_t y);
void screen_wifi_refresh_once(void);
uint8_t screen_wifi_popup_tap_outside_ta(lv_coord_t x, lv_coord_t y);
uint8_t screen_wifi_popup_hit_btn(lv_coord_t x, lv_coord_t y);

#endif
