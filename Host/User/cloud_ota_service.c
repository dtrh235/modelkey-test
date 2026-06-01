#include "cloud_ota_service.h"

#include "app_cloud_session.h"
#include "app_unlock_flash_queue.h"
#include "app_wall_clock.h"
#include "app_wifi_scan.h"
#include "app_wifi_diag.h"
#include "app_state.h"
#include "app_link_guard.h"
#include "cloud_aliyun_at.h"

#include <stdio.h>
#include <string.h>

static uint32_t s_unlock_seq;

static int cloud_unlock_drain_cb(const char *json, void *ctx)
{
    (void)ctx;
    return (cloud_aliyun_at_publish_property(json) != 0u) ? 1 : 0;
}

static uint8_t cloud_unlock_time_ready(void)
{
    return (cloud_aliyun_at_time_is_synced() != 0u && app_wall_clock_valid() != 0u) ? 1u : 0u;
}

void cloud_ota_service_init(void)
{
    s_unlock_seq = 0u;
    app_unlock_flash_init();
    app_cloud_session_init();
}

void cloud_ota_service_flush_unlock_pending(void)
{
    uint16_t n;

    if(cloud_unlock_time_ready() == 0u) {
        return;
    }
    if(cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    n = app_unlock_flash_count();
    if(n == 0u) {
        return;
    }
    while(app_unlock_flash_count() > 0u) {
        if(cloud_aliyun_at_is_online() == 0u) {
            break;
        }
        if(app_unlock_flash_upload_next(cloud_unlock_drain_cb, NULL) == 0u) {
            break;
        }
    }
    if(app_unlock_flash_count() < n) {
        CLOUD_TRACE_MSG("[CLOUD] unlock backlog flush\r\n");
    }
}

void cloud_ota_service_tick_1ms(void)
{
}

void cloud_ota_service_poll_5ms(void)
{
    if(g_app_scr == APP_SCR_11 && app_wifi_exclusive_mode() != 0u &&
       cloud_aliyun_at_wifi_link_ready() == 0u &&
       app_link_guard_mqtt() == 0u &&
       cloud_aliyun_at_wifi_bringup_active() == 0u) {
        return;
    }
    app_cloud_session_poll();
}

static uint8_t cloud_method_to_code(const char *method)
{
    if(method == NULL) {
        return 0u;
    }
    if(strcmp(method, "password") == 0) {
        return 1u;
    }
    if(strcmp(method, "nfc") == 0) {
        return 2u;
    }
    if(strcmp(method, "fingerprint") == 0) {
        return 3u;
    }
    if(strcmp(method, "remote") == 0) {
        return 4u;
    }
    return 0u;
}

void cloud_ota_service_report_event(cloud_event_t evt, const char *account)
{
    (void)evt;
    (void)account;
}

static uint8_t cloud_unlock_try_publish_now(const char *account, uint8_t method_code,
                                            uint8_t device_code, uint32_t seq)
{
    char time_str[24];
    char payload[384];

    if(app_wall_clock_format_now(time_str, sizeof(time_str)) == 0u) {
        return 0u;
    }
    (void)snprintf(payload, sizeof(payload),
                   "{\"method\":\"thing.event.property.post\",\"id\":%lu,"
                   "\"params\":{\"unlock_account\":\"%s\",\"unlock_time\":\"%s\","
                   "\"unlock_method\":%u,\"unlock_device\":%u},\"version\":\"1.0\"}",
                   (unsigned long)seq, account, time_str,
                   (unsigned)method_code, (unsigned)device_code);
    return cloud_aliyun_at_publish_property(payload);
}

void cloud_ota_service_report_unlock_record_ex(const char *account, const char *method,
                                               uint32_t uptime_ms, int unlock_device)
{
    const char *acc = (account != NULL) ? account : "";
    const char *mtd = (method != NULL) ? method : "unknown";
    uint8_t method_code = cloud_method_to_code(mtd);
    uint8_t device_code;
    uint32_t unlock_time_sec;
    uint32_t uptime_sec = uptime_ms / 1000u;
    uint32_t seq;

    if(unlock_device == CLOUD_UNLOCK_DEVICE_SLAVE) {
        device_code = 2u;
    } else {
        device_code = 1u;
    }

    seq = ++s_unlock_seq;

    if(cloud_unlock_time_ready() == 0u) {
        app_unlock_flash_append(acc, method_code, device_code, 0u, uptime_sec, seq);
        CLOUD_TRACE_MSG("[CLOUD] unlock stored (wait time sync)\r\n");
        return;
    }

    unlock_time_sec = app_wall_clock_epoch_sec();

    if(cloud_aliyun_at_wifi_link_ready() != 0u &&
       cloud_aliyun_at_is_online() != 0u &&
       cloud_unlock_try_publish_now(acc, method_code, device_code, seq) != 0u) {
        CLOUD_TRACE_MSG("[CLOUD] unlock sent live\r\n");
        return;
    }

    app_unlock_flash_append(acc, method_code, device_code, unlock_time_sec, uptime_sec, seq);
    CLOUD_TRACE_MSG("[CLOUD] unlock stored (wait cloud)\r\n");

    if(cloud_aliyun_at_is_online() != 0u) {
        cloud_ota_service_flush_unlock_pending();
    }
}
