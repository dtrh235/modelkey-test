#include "app_cloud_session.h"

#include "app_pair_bind.h"
#include "cloud_aliyun_at.h"
#include "cloud_ota_service.h"
#include "app_unlock_flash_queue.h"
#include "app_wall_clock.h"
#include "app_wifi_scan.h"
#include "app_wifi_diag.h"
#include "app_link_guard.h"
#include "stm32f4xx_hal.h"

#if (APP_CLOUD_UART_DEBUG != 0)
#include "SYSTEM/usart/usart.h"
#endif

#define CLOUD_SESSION_CONNECT_MS    (90000u)
#define CLOUD_SESSION_OFFLINE_MS    (2000u)
#define CLOUD_SESSION_UPLOAD_GAP_MS (300u)

typedef enum {
    SESS_OFF = 0,
    SESS_CONNECTING,
    SESS_ONLINE
} cloud_sess_st_t;

static cloud_sess_st_t s_st = SESS_OFF;
static uint32_t s_phase_ms = 0u;
static uint32_t s_offline_ms = 0u;
static uint32_t s_last_upload_ms = 0u;

static int cloud_session_publish_cb(const char *json, void *ctx)
{
    (void)ctx;
    return cloud_ota_service_publish_flash_json(json);
}

void app_cloud_session_init(void)
{
    s_st = SESS_OFF;
    s_phase_ms = 0u;
    s_offline_ms = 0u;
    s_last_upload_ms = 0u;
}

void app_cloud_session_wifi_down(void)
{
    s_st = SESS_OFF;
    s_phase_ms = 0u;
    s_offline_ms = 0u;
    s_last_upload_ms = 0u;
    app_pair_mark_ui_dirty();
    CLOUD_TRACE_MSG("[CLOUD] session wifi down\r\n");
}

uint8_t app_cloud_session_busy(void)
{
    return (s_st == SESS_CONNECTING) ? 1u : 0u;
}

void app_cloud_session_poll(void)
{
    uint32_t now = HAL_GetTick();

    if(cloud_aliyun_at_scr11_cloud_hold() != 0u) {
        return;
    }
    if(app_link_guard_wifi() != 0u) {
        return;
    }

    if(cloud_aliyun_at_wifi_link_ready() == 0u) {
        s_st = SESS_OFF;
        s_offline_ms = 0u;
        return;
    }

    switch(s_st) {
    case SESS_OFF:
        if(app_wifi_connect_busy() != 0u) {
            break;
        }
        if(cloud_aliyun_at_is_online() != 0u) {
            s_st = SESS_ONLINE;
            s_offline_ms = 0u;
            app_pair_mark_ui_dirty();
            CLOUD_TRACE_MSG("[CLOUD] session online (persistent)\r\n");
            CLOUD_DBG("session online persistent pend=%u",
                      (unsigned)app_unlock_flash_count());
            break;
        }
        if(app_link_guard_mqtt() == 0u &&
           cloud_aliyun_at_wifi_bringup_active() == 0u &&
           cloud_aliyun_at_mqtt_connecting() == 0u) {
            cloud_aliyun_at_request_mqtt_connect();
            if(cloud_aliyun_at_wifi_bringup_active() == 0u) {
                CLOUD_TRACE_MSG("[CLOUD] session wait MQTT\r\n");
            }
        }
        s_st = SESS_CONNECTING;
        s_phase_ms = now;
        CLOUD_DBG("session connect start pend=%u", (unsigned)app_unlock_flash_count());
        break;

    case SESS_CONNECTING:
        if(app_link_guard_mqtt() == 0u &&
           cloud_aliyun_at_wifi_bringup_active() == 0u &&
           cloud_aliyun_at_mqtt_connecting() == 0u &&
           cloud_aliyun_at_is_online() == 0u) {
            cloud_aliyun_at_request_mqtt_connect();
        }
        if(cloud_aliyun_at_is_online() != 0u) {
            s_st = SESS_ONLINE;
            s_offline_ms = 0u;
            app_pair_mark_ui_dirty();
            CLOUD_DBG("session MQTT online persistent");
            cloud_ota_service_flush_unlock_pending();
            break;
        }
        if((now - s_phase_ms) >= CLOUD_SESSION_CONNECT_MS) {
            CLOUD_TRACE_MSG("[CLOUD] session MQTT timeout retry\r\n");
            CLOUD_DBG("session connect timeout retry");
            app_link_guard_mqtt_end(0u);
            if(app_link_guard_mqtt() == 0u &&
               cloud_aliyun_at_wifi_bringup_active() == 0u &&
               cloud_aliyun_at_mqtt_connecting() == 0u) {
                cloud_aliyun_at_request_mqtt_connect();
            }
            s_phase_ms = now;
        }
        break;

    case SESS_ONLINE:
        if(cloud_aliyun_at_is_online() != 0u) {
            s_offline_ms = 0u;
            if(cloud_aliyun_at_time_is_synced() != 0u &&
               app_wall_clock_valid() != 0u &&
               app_unlock_flash_count() > 0u &&
               (s_last_upload_ms == 0u ||
                (now - s_last_upload_ms) >= CLOUD_SESSION_UPLOAD_GAP_MS)) {
                cloud_ota_service_flush_property_retry();
                (void)app_unlock_flash_upload_next(cloud_session_publish_cb, NULL);
                s_last_upload_ms = now;
            }
            break;
        }
        if(cloud_aliyun_at_mqtt_connecting() != 0u) {
            break;
        }
        if(s_offline_ms == 0u) {
            s_offline_ms = now;
            break;
        }
        if((now - s_offline_ms) < CLOUD_SESSION_OFFLINE_MS) {
            break;
        }
        s_offline_ms = 0u;
        s_st = SESS_CONNECTING;
        s_phase_ms = now;
        CLOUD_TRACE_MSG("[CLOUD] session MQTT lost reconnect\r\n");
        CLOUD_DBG("session MQTT lost -> reconnect");
        if(app_link_guard_mqtt() == 0u &&
           cloud_aliyun_at_wifi_bringup_active() == 0u &&
           cloud_aliyun_at_mqtt_connecting() == 0u) {
            cloud_aliyun_at_request_mqtt_connect();
        }
        break;

    default:
        s_st = SESS_OFF;
        break;
    }
}
