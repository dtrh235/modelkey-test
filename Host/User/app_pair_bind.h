#ifndef APP_PAIR_BIND_H
#define APP_PAIR_BIND_H

#include <stdint.h>

#define APP_PAIR_CODE_LEN 6u

extern volatile uint8_t g_pair_ui_dirty;

/** 上电从 Flash 恢复 App 绑定（须在 MQTT 开锁前调用） */
void app_pair_bind_init(void);
void app_pair_ensure_code(void);
void app_pair_regenerate_code(void);
/** 解除 App 绑定，保留当前配对码（不自动重新生成） */
void app_pair_clear_bind(void);
uint8_t app_pair_is_bound(void);
/** 已绑定且 appAccount 与门锁记录一致时允许远程开锁 */
uint8_t app_pair_remote_unlock_allowed(const char *app_account);
uint8_t app_pair_code_valid(void);
/** 1=MQTT 在线且已订阅 terminal/get（可与 NTP 校时并行） */
uint8_t app_pair_cloud_ready(void);
const char *app_pair_get_code(void);
const char *app_pair_get_bound_account(void);
void app_pair_mark_ui_dirty(void);
void app_pair_publish_bind_ack(uint8_t success, const char *req_id);
void app_pair_publish_unbind_ack(uint8_t success, const char *req_id);
/** 门锁本地解除 App 绑定时上行通知后端 */
void app_pair_publish_lock_unbind(void);
uint8_t app_pair_try_bind(const char *code, const char *app_account);
/** 密码连续错误触发自锁后上报告警（device: 1主屏 2侧门） */
void app_pair_publish_unlock_alert(uint8_t device_id, const char *last_account);

#endif
