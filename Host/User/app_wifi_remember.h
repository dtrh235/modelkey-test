#ifndef APP_WIFI_REMEMBER_H
#define APP_WIFI_REMEMBER_H

#include <stdint.h>

#define APP_WIFI_REMEMBER_SLOTS  3u

void app_wifi_remember_init(void);
void app_wifi_remember_scr11_reset(void);
void app_wifi_remember_on_scan_done(void);
/* GuiTask scr11 tick: poll connect + walk auto batch; returns 1 on new connect OK */
uint8_t app_wifi_remember_scr11_poll(void);
void app_wifi_remember_on_connect_success(const char *ssid, const char *password);
void app_wifi_remember_popup_open(void);
void app_wifi_remember_manual_begin(const char *ssid);
uint8_t app_wifi_remember_autoconnect_busy(void);
void app_wifi_remember_try_save_linked(void);

uint8_t app_wifi_remember_load_primary(char *ssid, uint16_t ssid_sz,
                                       char *pwd, uint16_t pwd_sz);

#endif
