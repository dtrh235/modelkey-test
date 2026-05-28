#include "cloud_ota_service.h"

#include "./SYSTEM/usart/usart.h"
#include "cloud_aliyun_at.h"
#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#define APP_CLOUD_LOG(...) ((void)0)

typedef struct {
    uint32_t ms_tick;
    uint32_t heartbeat_ms;
    uint32_t unlock_upload_ms;
    uint32_t unlock_seq;
} cloud_ota_ctx_t;

static cloud_ota_ctx_t g_cloud_ota;

#define CLOUD_UNLOCK_LOG_MAX 32u
#define CLOUD_UNLOCK_UPLOAD_PERIOD_MS 60000u

typedef struct {
    char account[13];
    char method[16];
    uint32_t uptime_ms;
    uint32_t seq;
} cloud_unlock_log_t;

static cloud_unlock_log_t g_unlock_logs[CLOUD_UNLOCK_LOG_MAX];
static uint8_t g_unlock_log_cnt = 0u;

void cloud_ota_service_init(void)
{
    memset(&g_cloud_ota, 0, sizeof(g_cloud_ota));
}

void cloud_ota_service_tick_1ms(void)
{
    g_cloud_ota.ms_tick++;
}

void cloud_ota_service_poll_5ms(void)
{
    if((g_cloud_ota.ms_tick - g_cloud_ota.heartbeat_ms) >= 5000u) {
        g_cloud_ota.heartbeat_ms = g_cloud_ota.ms_tick;
    }

    if((g_cloud_ota.ms_tick - g_cloud_ota.unlock_upload_ms) >= CLOUD_UNLOCK_UPLOAD_PERIOD_MS) {
        uint8_t i;
        g_cloud_ota.unlock_upload_ms = g_cloud_ota.ms_tick;
        if(g_unlock_log_cnt > 0u) {
            for(i = 0u; i < g_unlock_log_cnt; i++) {
                (void)g_unlock_logs[i];
            }
            /* 姣忔涓婁紶鍚庢竻绌哄垪琛紝閲婃斁宸蹭娇鐢ㄨ褰曟Ы浣?*/
            memset(g_unlock_logs, 0, sizeof(g_unlock_logs));
            g_unlock_log_cnt = 0u;
        }
    }
}

void cloud_ota_service_report_event(cloud_event_t evt, const char *account)
{
    (void)evt;
    (void)account;
}

bool cloud_ota_service_mark_upgrade(const char *url, uint32_t file_size)
{
    (void)url;
    (void)file_size;
    return false;
}

void cloud_ota_service_report_unlock_record(const char *account, const char *method, uint32_t uptime_ms)
{
    cloud_unlock_log_t *slot;
    const char *acc = (account != NULL) ? account : "";
    const char *mtd = (method != NULL) ? method : "unknown";
    char payload[320];
    uint32_t secs = uptime_ms / 1000u;
    int method_code = 0;
    uint8_t pub_ok;

    if(g_unlock_log_cnt >= CLOUD_UNLOCK_LOG_MAX) {
        /* 婊′簡涓㈡渶鏃э細鏁翠綋鍓嶇Щ涓€浣嶏紝淇濇寔鏈€杩戜簨浠?*/
        memmove(&g_unlock_logs[0], &g_unlock_logs[1], sizeof(g_unlock_logs[0]) * (CLOUD_UNLOCK_LOG_MAX - 1u));
        g_unlock_log_cnt = (uint8_t)(CLOUD_UNLOCK_LOG_MAX - 1u);
    }

    slot = &g_unlock_logs[g_unlock_log_cnt];
    memset(slot, 0, sizeof(*slot));
    strncpy(slot->account, acc, sizeof(slot->account) - 1u);
    strncpy(slot->method, mtd, sizeof(slot->method) - 1u);
    slot->uptime_ms = uptime_ms;
    slot->seq = ++g_cloud_ota.unlock_seq;
    g_unlock_log_cnt++;

    if(strcmp(mtd, "password") == 0) method_code = 1;
    else if(strcmp(mtd, "nfc") == 0) method_code = 2;
    else if(strcmp(mtd, "fingerprint") == 0) method_code = 3;
    else if(strcmp(mtd, "remote") == 0) method_code = 4;

    snprintf(payload, sizeof(payload),
             "{\"method\":\"thing.event.property.post\",\"id\":%lu,\"params\":{\"unlock_account\":\"%s\",\"unlock_time\":\"%lu\",\"unlock_method\":%d},\"version\":\"1.0\"}",
             (unsigned long)slot->seq, acc, (unsigned long)secs, method_code);
    pub_ok = cloud_aliyun_at_publish_property(payload);
    if(pub_ok) {
        APP_CLOUD_LOG("[ALIYUN] unlock record uploaded acc=%s method=%s t=%lus\r\n",
               acc, mtd, (unsigned long)secs);
    } else if(!cloud_aliyun_at_is_online()) {
        APP_CLOUD_LOG("[ALIYUN] unlock upload skipped: mqtt offline\r\n");
    } else {
        APP_CLOUD_LOG("[ALIYUN] unlock upload failed\r\n");
    }
}

void cloud_ota_service_ingest_line(const char *line)
{
    const char *p;
    uint32_t size;
    char url[256];

    if(line == NULL) return;
    if(strncmp(line, "OTA:", 4) != 0) return;

    p = strstr(line + 4, ",size=");
    if(p == NULL) return;
    if((size_t)(p - (line + 4)) >= sizeof(url)) return;

    memset(url, 0, sizeof(url));
    memcpy(url, line + 4, (size_t)(p - (line + 4)));
    size = (uint32_t)strtoul(p + 6, NULL, 10);

    if(cloud_ota_service_mark_upgrade(url, size)) {
        (void)0;
    }
}
