#ifndef APP_SCREEN8_FOCUS_H
#define APP_SCREEN8_FOCUS_H

#include <stdint.h>
#include "./BSP/KEY/key.h"

/* 学号、密码、管理员、btn_3、btn_4(录入)、确定 */
#define APP_SCREEN8_N_FOCUS 6u

void screen8_hide_status_labels(void);
void screen8_clear_error_if_any(void);
void screen8_set_focus(uint8_t idx);
void screen8_handle_ta_key(KeyValue_t key);
void screen8_cursor_blink_handle(void);

#endif
