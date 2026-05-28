#ifndef CLOUD_OTA_SERVICE_H
#define CLOUD_OTA_SERVICE_H

#include <stdbool.h>
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

void cloud_ota_service_init(void);
void cloud_ota_service_poll_5ms(void);
void cloud_ota_service_tick_1ms(void);
void cloud_ota_service_report_event(cloud_event_t evt, const char *account);
void cloud_ota_service_report_unlock_record(const char *account, const char *method, uint32_t uptime_ms);

/* Triggered by cloud command parser (topic/payload), non-blocking mark only */
bool cloud_ota_service_mark_upgrade(const char *url, uint32_t file_size);
void cloud_ota_service_ingest_line(const char *line);

#endif
