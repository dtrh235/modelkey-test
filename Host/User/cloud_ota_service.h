#ifndef CLOUD_OTA_SERVICE_H
#define CLOUD_OTA_SERVICE_H

#include <stdint.h>

typedef enum {
    CLOUD_EVT_UNLOCK_OK = 0,
    CLOUD_EVT_ADMIN_LOGIN_OK,
    CLOUD_EVT_ADMIN_LOGIN_FAIL,
    CLOUD_EVT_USER_ADD_OK,
    CLOUD_EVT_USER_ADD_FAIL,
    CLOUD_EVT_USER_DELETE_OK,
    CLOUD_EVT_USER_DELETE_FAIL,
    CLOUD_EVT_USER_ACC_CHANGE_OK,
    CLOUD_EVT_USER_ACC_CHANGE_FAIL,
    CLOUD_EVT_USER_PWD_CHANGE_OK,
    CLOUD_EVT_USER_PWD_CHANGE_FAIL,
    CLOUD_EVT_USER_ROLE_CHANGE,
    CLOUD_EVT_NFC_BIND_OK,
    CLOUD_EVT_NFC_BIND_FAIL,
    CLOUD_EVT_NFC_DELETE_OK,
    CLOUD_EVT_NFC_DELETE_FAIL
} cloud_event_t;

#define CLOUD_UNLOCK_DEVICE_MASTER  1
#define CLOUD_UNLOCK_DEVICE_SLAVE   2
#define CLOUD_UNLOCK_DEVICE_PHONE   3

void cloud_ota_service_init(void);
/** 临时密码开锁：unlock_record 附带真实访客账号供 App/后端作废 */
void cloud_ota_service_set_unlock_guest(const char *guest_account);
void cloud_ota_service_bind_cloud_task(void *task_handle);
void cloud_ota_service_poll_5ms(void);
void cloud_ota_service_tick_1ms(void);
void cloud_ota_service_report_event(cloud_event_t evt, const char *account);
/** 门锁用户表变更上行（App/云端同步与通知） */
void cloud_ota_publish_user_changed(const char *action, const char *account);
#define cloud_ota_service_report_unlock_record(acc, mtd, ms) \
    cloud_ota_service_report_unlock_record_ex((acc), (mtd), (ms), CLOUD_UNLOCK_DEVICE_MASTER)
void cloud_ota_service_report_unlock_record_ex(const char *account, const char *method,
                                               uint32_t uptime_ms, int unlock_device);
#define cloud_ota_service_queue_unlock_report(acc, mtd, ms) \
    cloud_ota_service_queue_unlock_report_ex((acc), (mtd), (ms), CLOUD_UNLOCK_DEVICE_MASTER)
void cloud_ota_service_queue_unlock_report_ex(const char *account, const char *method,
                                              uint32_t uptime_ms, int unlock_device);
/** CloudTask 持 UART2 锁期间调用：上报 deferred 开锁，避免与 poll 并发 */
void cloud_ota_service_flush_deferred_unlock(void);
void cloud_ota_service_flush_unlock_pending(void);
/** 仅补传物模型 property/post（失败重试队列，不影响 App terminal/push） */
void cloud_ota_service_flush_property_retry(void);
/** MQTT CONNACK 后立刻冲刷物模型重试（不清队列） */
void cloud_ota_service_on_mqtt_online(void);
void cloud_ota_service_clear_property_retry(void);
/** Flash 队列补传：同时发物模型 + terminal/push（与实时开锁一致） */
int cloud_ota_service_publish_flash_json(const char *json);
int cloud_ota_service_publish_flash_property_only(const char *json, void *ctx);

#endif
