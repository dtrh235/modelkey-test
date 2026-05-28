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

void cloud_ota_service_init(void);
void cloud_ota_service_poll_5ms(void);
void cloud_ota_service_tick_1ms(void);
void cloud_ota_service_report_event(cloud_event_t evt, const char *account);
#define cloud_ota_service_report_unlock_record(acc, mtd, ms) \
    cloud_ota_service_report_unlock_record_ex((acc), (mtd), (ms), CLOUD_UNLOCK_DEVICE_MASTER)
void cloud_ota_service_report_unlock_record_ex(const char *account, const char *method,
                                               uint32_t uptime_ms, int unlock_device);

#endif
