#ifndef APP_UI_V3_SERVICES_H
#define APP_UI_V3_SERVICES_H

#include "app_ui_v3_types.h"

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t connected;
} ui3_wifi_row_t;

void ui3_services_poll(ui3_state_t *st);
void ui3_services_sync_cloud(ui3_state_t *st);

void ui3_wifi_on_enter(ui3_state_t *st);
void ui3_wifi_on_leave(void);
uint8_t ui3_wifi_row_count(void);
uint8_t ui3_wifi_row_at(uint8_t idx, ui3_wifi_row_t *out);
uint8_t ui3_wifi_rssi_level(int8_t rssi);
uint8_t ui3_wifi_row_connected(const char *ssid);
void ui3_wifi_request_scan(ui3_state_t *st);
void ui3_wifi_try_connect(ui3_state_t *st);
void ui3_wifi_modal_confirm(ui3_state_t *st);
uint8_t ui3_wifi_modal_active(void);
void ui3_wifi_modal_close(void);
const char *ui3_wifi_modal_ssid(void);

/* 1=界面应显示「扫描中」（仅在实际扫描/pending 且未超时） */
uint8_t ui3_wifi_scan_ui_active(ui3_state_t *st);

/* 增量更新 WiFi 连接结果（对齐 Guider screen_wifi_show_connect_result） */
void ui3_wifi_show_connect_result(ui3_state_t *st, uint8_t ok);
void ui3_wifi_notify_sta_up(void);
void ui3_wifi_notify_connect_fail(void);

void ui3_pair_on_enter(ui3_state_t *st);
void ui3_pair_sync(ui3_state_t *st);
void ui3_pair_regen(ui3_state_t *st);
const char *ui3_pair_status_line(void);

#endif
