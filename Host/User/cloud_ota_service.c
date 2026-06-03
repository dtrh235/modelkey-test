#include "cloud_ota_service.h"

#include "app_cloud_session.h"
#include "app_unlock_flash_queue.h"
#include "app_wall_clock.h"
#include "app_wifi_scan.h"
#include "app_wifi_diag.h"
#include "app_state.h"
#include "app_link_guard.h"
#include "cloud_aliyun_at.h"
#include "app_wifi_diag.h"
#include "stm32f4xx_hal.h"

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
    static uint32_t s_flush_skip_no_time_log_ms;

    if(cloud_unlock_time_ready() == 0u) {
        uint32_t tnow = HAL_GetTick();
        if(s_flush_skip_no_time_log_ms == 0u ||
           (tnow - s_flush_skip_no_time_log_ms) >= 30000u) {
            s_flush_skip_no_time_log_ms = tnow;
            UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] flush skip no time\r\n");
        }
        return;
    }
    s_flush_skip_no_time_log_ms = 0u;
    if(cloud_aliyun_at_is_online() == 0u) {
        UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] flush skip offline\r\n");
        return;
    }
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(app_wifi_connect_busy() != 0u || app_wifi_scan_busy() != 0u ||
       app_wifi_scan_has_pending() != 0u) {
        UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] flush skip uart busy\r\n");
        return;
    }
#endif
    n = app_unlock_flash_count();
    if(n == 0u) {
        return;
    }
    (void)app_unlock_flash_upload_next(cloud_unlock_drain_cb, NULL);
    (void)n;
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

static uint8_t cloud_unlock_format_time_text(char *buf, size_t buf_sz,
                                             uint32_t stored_epoch_sec,
                                             uint32_t event_uptime_sec)
{
    if(app_wall_clock_format_unlock_event(buf, buf_sz, stored_epoch_sec, event_uptime_sec) != 0u) {
        return 1u;
    }
    return app_wall_clock_format_now(buf, buf_sz);
}

static uint8_t cloud_unlock_try_publish_now(const char *account, uint8_t method_code,
                                            uint8_t device_code, uint32_t seq,
                                            uint32_t unlock_time_sec, uint32_t uptime_sec)
{
    char payload[400];
    char time_txt[24];

    if(cloud_unlock_format_time_text(time_txt, sizeof(time_txt), unlock_time_sec, uptime_sec) == 0u) {
        if(device_code == 2u) {
            UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] slave try pub no time\r\n");
        } else {
            UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] host try pub no time\r\n");
        }
        return 0u;
    }
    /* 物模型 unlock_time：同步后的本地时刻 YYYY.MM.DD HH:MM */
    (void)snprintf(payload, sizeof(payload),
                   "{\"method\":\"thing.event.property.post\",\"id\":%lu,"
                   "\"params\":{\"unlock_account\":\"%s\",\"unlock_time\":\"%s\","
                   "\"unlock_method\":%u,\"unlock_device\":%u},\"version\":\"1.0\"}",
                   (unsigned long)seq, account, time_txt,
                   (unsigned)method_code, (unsigned)device_code);
    if(cloud_aliyun_at_publish_property(payload) == 0u) {
        if(device_code == 2u) {
            UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] slave try pub fail\r\n");
        } else {
            UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] host try pub fail\r\n");
        }
        return 0u;
    }
    return 1u;
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

#if (APP_HOST_SLAVE_UNLOCK_CLOUD_TRACE != 0)
    {
        char ent[96];
        (void)snprintf(ent, sizeof(ent),
                       "[UNLOCK] report dev=%s code=%u acc=%s mtd=%s seq=%lu\r\n",
                       (unlock_device == CLOUD_UNLOCK_DEVICE_SLAVE) ? "slave" : "host",
                       (unsigned)device_code, acc, mtd, (unsigned long)seq);
        UNLOCK_CLOUD_TRACE_MSG(ent);
    }
#endif

    if(cloud_unlock_time_ready() == 0u) {
        if(unlock_device == CLOUD_UNLOCK_DEVICE_SLAVE) {
            char dbg[96];
            (void)snprintf(dbg, sizeof(dbg),
                           "[UNLOCK] slave drop no time sn=%u clk=%u wifi=%u mqtt=%u\r\n",
                           (unsigned)cloud_aliyun_at_time_is_synced(),
                           (unsigned)app_wall_clock_valid(),
                           (unsigned)cloud_aliyun_at_wifi_link_ready(),
                           (unsigned)cloud_aliyun_at_is_online());
            UNLOCK_CLOUD_TRACE_MSG(dbg);
            return;
        }
        UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] host queue flash no time\r\n");
        app_unlock_flash_append(acc, method_code, device_code, 0u, uptime_sec, seq);
        cloud_aliyun_at_invalidate_unlock_flush();
        return;
    }

    unlock_time_sec = app_wall_clock_epoch_sec();

    if(cloud_aliyun_at_wifi_link_ready() != 0u &&
       cloud_aliyun_at_is_online() != 0u &&
       cloud_unlock_try_publish_now(acc, method_code, device_code, seq, unlock_time_sec,
                                    uptime_sec) != 0u) {
        if(unlock_device == CLOUD_UNLOCK_DEVICE_SLAVE) {
            UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] slave live ok\r\n");
        } else {
            UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] host live ok\r\n");
        }
        return;
    }

    if(unlock_device == CLOUD_UNLOCK_DEVICE_SLAVE) {
        UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] slave queue flash\r\n");
    } else {
        UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] host queue flash\r\n");
    }
    app_unlock_flash_append(acc, method_code, device_code, unlock_time_sec, uptime_sec, seq);
    cloud_aliyun_at_invalidate_unlock_flush();
}

