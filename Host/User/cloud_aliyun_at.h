#ifndef CLOUD_ALIYUN_AT_H
#define CLOUD_ALIYUN_AT_H

#include <stdint.h>
#include "app_config.h"
#include "app_config.h"

void cloud_aliyun_at_init(void);
void cloud_aliyun_at_poll_5ms(void);
/* 1=曾连上 WiFi（STA 有 IP） */
uint8_t cloud_aliyun_at_wifi_was_up(void);
uint8_t cloud_aliyun_at_wifi_ui_up(void);
/* 1=允许跑 cloud poll：在 WiFi 页，或已经连过 WiFi */
uint8_t cloud_aliyun_at_poll_allowed(uint8_t on_wifi_scr);
/* 进入/离开 WiFi 设置页：暂停 MQTT 占用 UART2，便于 CWLAP */
void cloud_aliyun_at_scr11_enter(void);
void cloud_aliyun_at_scr11_leave(void);
uint8_t cloud_aliyun_at_is_online(void);
uint8_t cloud_aliyun_at_wifi_joined(void);
uint8_t cloud_aliyun_at_sta_on_target_ssid(void);
uint8_t cloud_aliyun_at_wifi_link_ready(void);
/* 进 WiFi 页时校验 STA IP（防掉线后仍显示已连接） */
void cloud_aliyun_at_request_wifi_ip_verify(void);
void cloud_aliyun_at_request_target_wifi_join(void);
/* 已连 STA 的 SSID（仅 wifi_joined 时有效） */
uint8_t cloud_aliyun_at_get_connected_ssid(char *out, uint16_t out_sz);
uint8_t cloud_aliyun_at_refresh_connected_ssid(void);
/* 连接超时诊断：step/join/ui_busy/async（仅 APP_WIFI_UART_DEBUG） */
#if (APP_WIFI_UART_DEBUG != 0)
void cloud_aliyun_at_wifi_join_diag_printf(void);
#else
#define cloud_aliyun_at_wifi_join_diag_printf() ((void)0)
#endif
/** 仅发出 property/post（SEND OK 即成功，不等待 post_reply） */
uint8_t cloud_aliyun_at_publish_property_send(const char *json_payload);
/** 等待 post_reply：1=code200，0=明确失败，2=超时无 reply（均不算成功） */
uint8_t cloud_aliyun_at_property_post_await(uint32_t wait_ms);
uint8_t cloud_aliyun_at_publish_property(const char *json_payload);
/** 向任意 MQTT Topic 发布 QoS0 JSON。 @return 1=已发出 */
uint8_t cloud_aliyun_at_mqtt_publish_qos0(const char *topic, const char *json_payload);
void cloud_aliyun_at_invalidate_unlock_flush(void);
void cloud_aliyun_at_time_sync_done(void);

/* Shared UART2 (ESP WiFi) for WiFi settings UI scan/connect */
void cloud_uart2_ensure_init(void);
int cloud_uart2_send(const char *text);
int cloud_uart2_wait(const char *needle, uint32_t timeout_ms);
int cloud_uart2_read_byte(uint8_t *out, uint32_t timeout_ms);
void cloud_uart2_rx_clear(void);
int cloud_uart2_rx_has(const char *needle);
void cloud_uart2_pump_rx(uint32_t duration_ms);
uint16_t cloud_uart2_collect_to(char *dst, uint16_t dst_sz, uint16_t dst_pos, uint32_t duration_ms);
uint8_t cloud_uart2_ui_busy(void);
void cloud_uart2_set_ui_busy(uint8_t busy);
/* ESP8266 已完成 cloud 侧 AT 握手（勿在 init 复位期间单独发 AT） */
void cloud_uart2_drain_hw(uint32_t ms);
/* 连接/CWJAP 前：等待 CWLAP 尾包吐完 */
void cloud_uart2_wait_scan_tail_quiet(uint32_t quiet_ms);
/* 拉低脉冲硬复位：WF24=PE12(KEY) / ESP8266=PA8(RST)，等待 boot_ms 后排空 UART RX */
void cloud_uart2_esp_hw_reset(uint32_t boot_ms);
/* 非阻塞复位：GuiTask 每帧 poll，避免 HAL_Delay 卡死触摸 */
uint8_t cloud_uart2_esp_hw_reset_begin(uint32_t boot_ms);
uint8_t cloud_uart2_esp_hw_reset_poll(void);
uint8_t cloud_uart2_esp_hw_reset_active(void);
void cloud_uart2_prepare_for_cwlap_only(void);
uint8_t cloud_uart2_modem_ready(void);
/* 发 AT 探测模组；成功则置 modem_ready（WiFi 页暂停 cloud poll 时仍可握手） */
uint8_t cloud_uart2_try_modem_ready(void);
uint8_t cloud_uart2_try_modem_ready_timeout(uint32_t wait_ms);
/* WiFi 页发 AT 前：排空 RX、固定 115200，MQTT 在线时先 CIPCLOSE */
void cloud_uart2_prepare_for_ui_at(void);
void cloud_uart2_copy_rx_win(char *dst, uint16_t dst_sz);
/* CWLAP 扫描：blocking + 帧校验 + 重试；0=成功 -5=超时 -6=脏帧 */
int cloud_aliyun_at_run_cwlap_scan(char *dst, uint16_t dst_sz, uint16_t *out_len);
/* 分片 CWLAP：GuiTask 每帧 poll，UART2 IRQ 收包；1=进行中 0=成功 <0=失败 */
int cloud_aliyun_at_cwlap_scan_async_start(char *dst, uint16_t dst_sz, uint16_t *out_len);
int cloud_aliyun_at_cwlap_scan_async_poll(void);
void cloud_aliyun_at_cwlap_scan_async_abort(void);
uint8_t cloud_aliyun_at_cwlap_scan_async_active(void);
/* CloudTask：扫描 abort/结束后等 ESP AT 真 idle（持 uart2 mutex） */
uint8_t cloud_aliyun_at_cwlap_teardown_idle(uint32_t timeout_ms);
/* 已废弃：rev=23 起不再使用 async 分片扫描 */
#define CWLAP_SCAN_RUNNING  1
#define CWLAP_SCAN_DEFERRED 2  /* UART2 忙，未发 CWLAP */
void cloud_aliyun_cwlap_scan_begin(void);
void cloud_aliyun_cwlap_scan_abort(void);
uint8_t cloud_aliyun_cwlap_scan_active(void);
/* 1=进行中 0=成功 <0=失败 */
int cloud_aliyun_cwlap_scan_step(char *dst, uint16_t dst_sz, uint16_t *out_len);
/* 用户选热点后走原 CWJAP 流程（app_wifi_cfg 中的 SSID/密码） */
void cloud_aliyun_at_user_wifi_join_set_esp_idle_ok(void);
void cloud_aliyun_at_user_wifi_join_start(void);
/* 0=进行中 1=成功 2=失败 */
uint8_t cloud_aliyun_at_user_wifi_join_poll(void);
void cloud_aliyun_at_user_wifi_join_abort(void);
/* CWJAP 已失败：供 GuiTask 立即结束 connect_busy（勿等 CloudTask 轮询） */
uint8_t cloud_aliyun_at_user_wifi_join_fail_pending(void);
void cloud_aliyun_at_user_wifi_join_fail_clear(void);
/* 1=用户手动/自动连 WiFi 占用 UART2（扫描应让路） */
uint8_t cloud_aliyun_at_user_wifi_join_active(void);
/* 0 idle 1 pending 2 ok 3 fail */
uint8_t cloud_aliyun_at_user_wifi_join_state(void);
/* 1=CWJAP/CIFSR/SNTP/MQTT 建链进行中 */
uint8_t cloud_aliyun_at_wifi_bringup_active(void);
uint8_t cloud_aliyun_at_scr11_cloud_hold(void);
void cloud_aliyun_at_request_mqtt_connect(void);
void cloud_aliyun_at_mqtt_session_disconnect(void);
uint8_t cloud_aliyun_at_mqtt_connecting(void);
uint8_t cloud_aliyun_at_time_is_synced(void);
/** 1=MQTT-NTP 多次失败，本连接周期内不再阻塞 terminal/get 订阅 */
uint8_t cloud_aliyun_at_ntp_give_up(void);
/** 下一次 SUBSCRIBE 将使用的 MQTT 包 ID（不递增） */
uint16_t cloud_aliyun_at_mqtt_peek_pkt_id(void);
/** MQTT 在线时发送 SUBSCRIBE(qos0)。@return 1=已发出；out_pkt_id 可填 NULL */
uint8_t cloud_aliyun_at_mqtt_subscribe_qos0(const char *topic, uint16_t *out_pkt_id);
/** 轮询 UART2 上 MQTT SUBACK / 下行 PUBLISH（订阅等待期调用） */
void cloud_aliyun_at_pump_mqtt_ctrl(void);
/** rx 窗口里是否有匹配 pkt_id 的 MQTT SUBACK */
uint8_t cloud_aliyun_at_mqtt_suback_matches(uint16_t pkt_id);
/** 调试：一行 MQTT/订阅状态快照（APP_CLOUD_COMMAND_TRACE） */
void cloud_aliyun_at_cmd_diag_log(const char *tag);
/** 调试：rx 窗口十六进制预览 */
void cloud_aliyun_at_cmd_diag_rx_hex(void);

#endif
