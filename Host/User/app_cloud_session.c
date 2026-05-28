#include "app_cloud_session.h"

#include "cloud_aliyun_at.h"
#include "app_unlock_flash_queue.h"
#include "app_wifi_scan.h"
#include "app_wifi_diag.h"
#include "stm32f4xx_hal.h"

#define CLOUD_SESSION_INTERVAL_MS   (300000u)
#define CLOUD_SESSION_FIRST_MS      (15000u)
#define CLOUD_SESSION_CONNECT_MS    (90000u)
#define CLOUD_SESSION_UPLOAD_MS     (60000u)
#define CLOUD_SESSION_SYNC_MS       (25000u)

typedef enum {
    SESS_IDLE = 0,
    SESS_CONNECTING,
    SESS_UPLOAD,
    SESS_SYNC,
    SESS_DISCONNECT,
    SESS_COOLDOWN
} cloud_sess_st_t;

static cloud_sess_st_t s_st = SESS_IDLE;
static uint32_t s_next_ms = 0u;
static uint32_t s_phase_ms = 0u;

static int cloud_session_publish_cb(const char *json, void *ctx)
{
    (void)ctx;
    return (cloud_aliyun_at_publish_property(json) != 0u) ? 1 : 0;
}

void app_cloud_session_init(void)
{
    s_st = SESS_IDLE;
    s_next_ms = HAL_GetTick() + CLOUD_SESSION_FIRST_MS;
    s_phase_ms = 0u;
}

uint8_t app_cloud_session_busy(void)
{
    return (s_st == SESS_CONNECTING || s_st == SESS_UPLOAD ||
            s_st == SESS_SYNC || s_st == SESS_DISCONNECT) ? 1u : 0u;
}

static void cloud_session_schedule_next(void)
{
    s_next_ms = HAL_GetTick() + CLOUD_SESSION_INTERVAL_MS;
    s_st = SESS_COOLDOWN;
}

void app_cloud_session_poll(void)
{
    uint32_t now = HAL_GetTick();

    if(cloud_aliyun_at_scr11_cloud_hold() != 0u) {
        return;
    }

    if(s_st == SESS_COOLDOWN) {
        if((int32_t)(now - s_next_ms) >= 0) {
            s_st = SESS_IDLE;
        }
        return;
    }

    if(cloud_aliyun_at_wifi_link_ready() == 0u) {
        return;
    }

    switch(s_st) {
    case SESS_IDLE:
        if(app_wifi_connect_busy() != 0u) {
            break;
        }
        if((int32_t)(now - s_next_ms) < 0 && app_unlock_flash_count() == 0u) {
            break;
        }
        if((int32_t)(now - s_next_ms) >= 0 || app_unlock_flash_count() > 0u) {
            if(cloud_aliyun_at_is_online() != 0u) {
                s_st = SESS_UPLOAD;
                s_phase_ms = now;
                CLOUD_DBG("session upload start pend=%u", (unsigned)app_unlock_flash_count());
            } else {
                s_st = SESS_CONNECTING;
                s_phase_ms = now;
                CLOUD_DBG("session wait MQTT online pend=%u", (unsigned)app_unlock_flash_count());
            }
        }
        break;
    case SESS_CONNECTING:
        if(cloud_aliyun_at_is_online() != 0u) {
            s_st = SESS_UPLOAD;
            s_phase_ms = now;
            CLOUD_DBG("session MQTT online -> upload");
            break;
        }
        if(cloud_aliyun_at_wifi_bringup_active() == 0u &&
           cloud_aliyun_at_wifi_link_ready() != 0u &&
           cloud_aliyun_at_is_online() == 0u) {
            cloud_aliyun_at_request_mqtt_connect();
        }
        if((now - s_phase_ms) >= CLOUD_SESSION_CONNECT_MS) {
            CLOUD_DBG("session connect timeout");
            cloud_session_schedule_next();
        }
        break;
    case SESS_UPLOAD:
        if(cloud_aliyun_at_is_online() == 0u) {
            s_st = SESS_CONNECTING;
            s_phase_ms = now;
            cloud_aliyun_at_request_mqtt_connect();
            break;
        }
        if(app_unlock_flash_count() == 0u) {
            s_st = SESS_SYNC;
            s_phase_ms = now;
            break;
        }
        if(app_unlock_flash_upload_next(cloud_session_publish_cb, NULL) != 0u) {
            s_phase_ms = now;
        }
        if((now - s_phase_ms) >= CLOUD_SESSION_UPLOAD_MS) {
            CLOUD_DBG("session upload timeout left=%u", (unsigned)app_unlock_flash_count());
            s_st = SESS_SYNC;
            s_phase_ms = now;
        }
        break;
    case SESS_SYNC:
        if(cloud_aliyun_at_time_is_synced() != 0u ||
           (now - s_phase_ms) >= CLOUD_SESSION_SYNC_MS) {
            s_st = SESS_DISCONNECT;
            s_phase_ms = now;
        }
        break;
    case SESS_DISCONNECT:
        cloud_aliyun_at_mqtt_session_disconnect();
        CLOUD_DBG("session disconnect done flash=%u", (unsigned)app_unlock_flash_count());
        cloud_session_schedule_next();
        break;
    default:
        break;
    }
}
