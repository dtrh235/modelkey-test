#include "app_link_guard.h"

#include "app_config.h"
#include "app_state.h"
#include "cloud_aliyun_at.h"

#define LINK_GUARD_TRACE(s)  ((void)0)

static app_link_guard_phase_t s_phase = APP_LINK_GUARD_NONE;

static void link_guard_abort_scan(void)
{
    g_wifi_scan_pending = 0u;
    g_wifi_scan_abort = 1u;
    cloud_aliyun_cwlap_scan_abort();
    cloud_aliyun_at_cwlap_scan_async_abort();
}

uint8_t app_link_guard_active(void)
{
    return (s_phase != APP_LINK_GUARD_NONE) ? 1u : 0u;
}

uint8_t app_link_guard_wifi(void)
{
    return (s_phase == APP_LINK_GUARD_WIFI) ? 1u : 0u;
}

uint8_t app_link_guard_mqtt(void)
{
    return (s_phase == APP_LINK_GUARD_MQTT) ? 1u : 0u;
}

uint8_t app_link_guard_blocks_scan(void)
{
    return app_link_guard_active();
}

uint8_t app_link_guard_blocks_wifi_connect(void)
{
    return app_link_guard_active();
}

void app_link_guard_wifi_begin(void)
{
    if(s_phase == APP_LINK_GUARD_MQTT) {
        app_link_guard_mqtt_end(0u);
    }
    s_phase = APP_LINK_GUARD_WIFI;
    link_guard_abort_scan();
    cloud_uart2_set_ui_busy(0u);
    LINK_GUARD_TRACE("[LINK] guard WIFI on\r\n");
}

void app_link_guard_wifi_end(uint8_t ok)
{
    if(s_phase != APP_LINK_GUARD_WIFI) {
        return;
    }
    s_phase = APP_LINK_GUARD_NONE;
    LINK_GUARD_TRACE(ok ? "[LINK] guard WIFI ok\r\n" : "[LINK] guard WIFI fail\r\n");
}

void app_link_guard_mqtt_begin(void)
{
    if(s_phase == APP_LINK_GUARD_WIFI) {
        s_phase = APP_LINK_GUARD_NONE;
    }
    if(s_phase == APP_LINK_GUARD_MQTT) {
        return;
    }
    s_phase = APP_LINK_GUARD_MQTT;
    link_guard_abort_scan();
    cloud_uart2_set_ui_busy(0u);
    LINK_GUARD_TRACE("[LINK] guard MQTT on\r\n");
}

void app_link_guard_mqtt_end(uint8_t ok)
{
    if(s_phase != APP_LINK_GUARD_MQTT) {
        return;
    }
    s_phase = APP_LINK_GUARD_NONE;
    LINK_GUARD_TRACE(ok ? "[LINK] guard MQTT ok\r\n" : "[LINK] guard MQTT fail\r\n");
}
