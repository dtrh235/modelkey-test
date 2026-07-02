#ifndef APP_WIFI_CFG_H
#define APP_WIFI_CFG_H

#include <stdint.h>

void app_wifi_cfg_init_defaults(void);
const char *app_wifi_cfg_get_ssid(void);
const char *app_wifi_cfg_get_password(void);
void app_wifi_cfg_set(const char *ssid, const char *password);
uint8_t app_wifi_cfg_request_reconnect(void);
void app_wifi_cfg_clear_reconnect_request(void);
void app_wifi_cfg_mark_reconnect_if_saved(void);

#endif
