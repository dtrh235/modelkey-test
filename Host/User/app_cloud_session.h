#ifndef APP_CLOUD_SESSION_H
#define APP_CLOUD_SESSION_H

#include <stdint.h>

void app_cloud_session_init(void);
void app_cloud_session_poll(void);
uint8_t app_cloud_session_busy(void);
/* WiFi STA 掉线：清 session，禁止自动重连 MQTT */
void app_cloud_session_wifi_down(void);

#endif
