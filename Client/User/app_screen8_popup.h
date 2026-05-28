#ifndef APP_SCREEN8_POPUP_H
#define APP_SCREEN8_POPUP_H

#include <stdbool.h>
#include "lvgl.h"

bool screen8_try_read_chip(void);
void screen8_popup_close_and_back(void);
void screen8_popup_close_and_back_timer_cb(lv_timer_t *timer);
void screen8_popup_close_only(void);
void screen8_popup_close_event_cb(lv_event_t *e);
void screen8_show_enroll_popup(const char *message);

#endif
