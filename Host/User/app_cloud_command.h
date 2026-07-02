#ifndef APP_CLOUD_COMMAND_H
#define APP_CLOUD_COMMAND_H

#include <stddef.h>
#include <stdint.h>

void app_cloud_command_init(void);
void app_cloud_command_reset(void);
void app_cloud_command_poll(void);
void app_cloud_command_on_suback(void);
/** SUBACK 包 id 与 terminal/get 订阅一致时才置 bridge ready */
void app_cloud_command_on_suback_for_pkt(uint16_t pkt_id);
/** 1=已发 SUBSCRIBE 但尚未收到 SUBACK（MQTT ping 应让路） */
uint8_t app_cloud_command_waiting_suback(void);
/** 1=已订阅 terminal/get，可接收 App 下发指令（bind 等） */
uint8_t app_cloud_command_is_ready(void);
/** MQTT 主动重连前通知后端离线（弥补 CIPCLOSE 不触发 LWT） */
void app_cloud_command_publish_offline_best_effort(void);

/** 诊断快照用：idle / sent / ok / off */
void app_cloud_command_diag_status(char *buf, size_t buf_sz);

/** 处理 +IPD 内 MQTT PUBLISH（非 NTP）。@return 1=已消费 */
uint8_t app_cloud_command_on_mqtt_publish(const uint8_t *buf, uint16_t len);
/** WF24 +MQTTSUBRECV 明文 JSON 下行 */
uint8_t app_cloud_command_on_mqtt_plain(const uint8_t *buf, uint16_t len);

#endif
