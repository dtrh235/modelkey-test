#ifndef APP_TOUCH_UI_H
#define APP_TOUCH_UI_H

#include <stdint.h>

void app_touch_ui_handle(void);

/* 离开 WiFi 页时重置触摸槽位 */
void app_touch_wifi_reset(void);

#endif
