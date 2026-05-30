#ifndef APP_LINK_GUARD_H
#define APP_LINK_GUARD_H

#include <stdint.h>

typedef enum {
    APP_LINK_GUARD_NONE = 0,
    APP_LINK_GUARD_WIFI,
    APP_LINK_GUARD_MQTT
} app_link_guard_phase_t;

/* WiFi / MQTT 建链期间：屏蔽扫描、二次连接等会抢 UART2 的操作 */
uint8_t app_link_guard_active(void);
uint8_t app_link_guard_wifi(void);
uint8_t app_link_guard_mqtt(void);
uint8_t app_link_guard_blocks_scan(void);
uint8_t app_link_guard_blocks_wifi_connect(void);

void app_link_guard_wifi_begin(void);
void app_link_guard_wifi_end(uint8_t ok);
void app_link_guard_mqtt_begin(void);
void app_link_guard_mqtt_end(uint8_t ok);

#endif
