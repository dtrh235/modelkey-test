#include "cloud_ota_service.h"

#include "app_cloud_session.h"
#include "app_unlock_flash_queue.h"
#include "app_wall_clock.h"
#include "app_wifi_scan.h"
#include "app_state.h"

#include <string.h>

static uint32_t s_unlock_seq;

void cloud_ota_service_init(void)
{
    s_unlock_seq = 0u;
    app_unlock_flash_init();
    app_cloud_session_init();
}

void cloud_ota_service_tick_1ms(void)
{
}

void cloud_ota_service_poll_5ms(void)
{
    if(g_app_scr == APP_SCR_11 && app_wifi_exclusive_mode() != 0u) {
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

    if(app_wall_clock_valid() != 0u) {
        unlock_time_sec = app_wall_clock_epoch_sec();
    } else {
        unlock_time_sec = uptime_sec;
    }

    seq = ++s_unlock_seq;
    app_unlock_flash_append(acc, method_code, device_code, unlock_time_sec, uptime_sec, seq);
}
