#include "app_config.h"

#if (APP_LEGACY_UI_ENABLE == 0)

#include "app_screen_wifi_flow.h"
#include "app_screen8_popup.h"
#include "app_screen8_flow.h"
#include "lvgl.h"

void enter_screen_wifi(void) {}
void screen_wifi_prepare_on_enter(void) {}
void screen_wifi_poll_tick(void) {}
void screen_wifi_gui_wake(void) {}
uint8_t screen_wifi_gui_work_pending(void) { return 0u; }
void screen_wifi_notify_sta_up(void) {}
void screen_wifi_notify_sta_down(void) {}
void screen_wifi_notify_connect_fail(void) {}
void screen_wifi_notify_scan_start(void) {}
void screen_wifi_notify_scan_done(void) {}
uint8_t screen_wifi_popup_is_active(void) { return 0u; }
void screen_wifi_popup_cancel(void) {}
void screen_wifi_popup_confirm(void) {}
void screen_wifi_handle_popup_input(KeyValue_t key) { (void)key; }
void screen_wifi_list_move(int8_t delta) { (void)delta; }
void screen_wifi_list_select(uint8_t idx) { (void)idx; }
uint8_t screen_wifi_hit_row(lv_coord_t x, lv_coord_t y) { (void)x; (void)y; return 0u; }
uint8_t screen_wifi_get_sel_index(void) { return 0u; }
void screen_wifi_list_ok(void) {}
void screen_wifi_cursor_blink_handle(void) {}
void screen_wifi_exit_cleanup(void) {}
void screen_wifi_exit_to_menu(void) {}
uint8_t screen_wifi_point_in_list(lv_coord_t x, lv_coord_t y) { (void)x; (void)y; return 0u; }
lv_obj_t *screen_wifi_refresh_btn_obj(void) { return NULL; }
void screen_wifi_list_scroll_by(lv_coord_t dy) { (void)dy; }
uint8_t screen_wifi_list_lvgl_scrolling(void) { return 0u; }
void screen_wifi_list_touch_begin(void) {}
uint8_t screen_wifi_hit_refresh_btn(lv_coord_t x, lv_coord_t y) { (void)x; (void)y; return 0u; }
void screen_wifi_refresh_once(void) {}
uint8_t screen_wifi_popup_tap_outside_ta(lv_coord_t x, lv_coord_t y) { (void)x; (void)y; return 0u; }
uint8_t screen_wifi_popup_hit_btn(lv_coord_t x, lv_coord_t y) { (void)x; (void)y; return 0u; }

bool screen8_try_read_chip(void) { return true; }
void screen8_popup_close_and_back(void) {}
void screen8_popup_close_and_back_timer_cb(lv_timer_t *timer) { (void)timer; }
void screen8_popup_close_only(void) {}
void screen8_popup_close_event_cb(lv_event_t *e) { (void)e; }
void screen8_show_enroll_popup(const char *message) { (void)message; }
uint8_t screen8_popup_touch(lv_coord_t x, lv_coord_t y) { (void)x; (void)y; return 0u; }

bool screen8_has_valid_acc_pwd_input(void) { return false; }
void screen8_attempt_commit(void) {}
void screen8_success_popup_poll(void) {}
uint8_t screen8_add_user_success_popup_active(void) { return 0u; }
void screen8_add_user_success_popup_clear(void) {}
void enter_screen_8(void) {}

#endif
