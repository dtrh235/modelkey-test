#include "cloud_ota_service.h"
#include "app_cloud_bind_cmd.h"

#include "app_cloud_session.h"
#include "app_cloud_command.h"
#include "app_config.h"
#include "app_user_ops.h"
#include "user_auth_portable.h"
#include "app_unlock_flash_queue.h"
#include "app_temp_password.h"
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

static volatile uint8_t s_defer_unlock_pending;
static char s_defer_unlock_acc[20];
static char s_defer_unlock_mtd[20];
static volatile uint32_t s_defer_unlock_uptime;
static int s_defer_unlock_device;

#define CLOUD_PROP_RETRY_SLOTS  8u
#define CLOUD_PROP_RETRY_BYTES  400u
#define CLOUD_PROP_INLINE_TRIES 1u
#define CLOUD_PROP_RETRY_MIN_MS 1500u
#define CLOUD_PROP_POST_AWAIT_MS  1500u

static char s_prop_retry[CLOUD_PROP_RETRY_SLOTS][CLOUD_PROP_RETRY_BYTES];
static uint8_t s_prop_retry_cnt;
static uint32_t s_prop_retry_last_ms;
static uint32_t s_prop_flush_earliest_ms;
static uint32_t s_prop_last_confirmed_seq;
static uint32_t s_prop_last_confirmed_ms;
static char s_unlock_guest_acc[13];

static uint32_t cloud_prop_seq_from_payload(const char *payload)
{
    unsigned long seq = 0ul;
    const char *p;

    if(payload == NULL) {
        return 0u;
    }
    p = strstr(payload, "\"id\":");
    if(p != NULL) {
        (void)sscanf(p + 5, "%lu", &seq);
    }
    return (uint32_t)seq;
}

static uint8_t cloud_prop_already_confirmed(uint32_t seq)
{
    uint32_t now;

    if(seq == 0u) {
        return 0u;
    }
    now = HAL_GetTick();
    if(s_prop_last_confirmed_seq == seq &&
       s_prop_last_confirmed_ms != 0u &&
       (now - s_prop_last_confirmed_ms) < 30000u) {
        return 1u;
    }
    return 0u;
}

static void cloud_prop_confirmed_from_payload(const char *payload)
{
    uint32_t seq = cloud_prop_seq_from_payload(payload);

    if(seq == 0u) {
        return;
    }
    s_prop_last_confirmed_seq = seq;
    s_prop_last_confirmed_ms = HAL_GetTick();
    app_unlock_flash_drop_seq(seq);
}

static void cloud_prop_persist_backup_from_payload(const char *payload)
{
    char account[13] = "";
    unsigned method_code = 0u;
    unsigned device_code = 1u;
    uint32_t seq;
    const char *p;

    if(payload == NULL || cloud_prop_already_confirmed(cloud_prop_seq_from_payload(payload)) != 0u) {
        return;
    }
    seq = cloud_prop_seq_from_payload(payload);
    if(seq == 0u) {
        return;
    }
    p = strstr(payload, "\"unlock_account\":\"");
    if(p != NULL) {
        (void)sscanf(p + 18, "%12[^\"]", account);
    }
    if(account[0] == '\0') {
        return;
    }
    p = strstr(payload, "\"unlock_method\":");
    if(p != NULL) {
        (void)sscanf(p + 16, "%u", &method_code);
    }
    p = strstr(payload, "\"unlock_device\":");
    if(p != NULL) {
        (void)sscanf(p + 16, "%u", &device_code);
    }
    if(method_code == 0u) {
        return;
    }
    app_unlock_flash_append_bridge_done(account, (uint8_t)method_code, (uint8_t)device_code,
                                        app_wall_clock_epoch_sec(), HAL_GetTick() / 1000u, seq);
}

static uint8_t cloud_unlock_publish_property_resilient(const char *payload);

void cloud_ota_service_set_unlock_guest(const char *guest_account)
{
    if(guest_account == NULL || guest_account[0] == '\0') {
        s_unlock_guest_acc[0] = '\0';
        return;
    }
    strncpy(s_unlock_guest_acc, guest_account, sizeof(s_unlock_guest_acc) - 1u);
    s_unlock_guest_acc[sizeof(s_unlock_guest_acc) - 1u] = '\0';
}

static uint8_t cloud_prop_retry_enqueue(const char *payload)
{
    uint8_t slot;
    uint8_t i;

    if(payload == NULL || payload[0] == '\0') {
        return 0u;
    }
    for(i = 0u; i < s_prop_retry_cnt; i++) {
        if(strcmp(s_prop_retry[i], payload) == 0) {
            return 1u;
        }
    }
    if(s_prop_retry_cnt >= CLOUD_PROP_RETRY_SLOTS) {
        memmove(&s_prop_retry[0], &s_prop_retry[1],
                sizeof(s_prop_retry[0]) * (size_t)(CLOUD_PROP_RETRY_SLOTS - 1u));
        s_prop_retry_cnt = (uint8_t)(CLOUD_PROP_RETRY_SLOTS - 1u);
    }
    slot = s_prop_retry_cnt;
    (void)snprintf(s_prop_retry[slot], CLOUD_PROP_RETRY_BYTES, "%s", payload);
    s_prop_retry_cnt++;
    s_prop_flush_earliest_ms = HAL_GetTick() + 400u;
    return 1u;
}

static uint8_t cloud_unlock_publish_property_resilient(const char *payload)
{
    uint8_t attempt;
    uint32_t seq;

    if(payload == NULL || payload[0] == '\0') {
        return 0u;
    }
    seq = cloud_prop_seq_from_payload(payload);
    if(cloud_prop_already_confirmed(seq) != 0u) {
        return 1u;
    }
    for(attempt = 0u; attempt < CLOUD_PROP_INLINE_TRIES; attempt++) {
        uint8_t await_r;

        if(cloud_aliyun_at_publish_property_send(payload) == 0u) {
            cloud_aliyun_at_pump_mqtt_ctrl();
            continue;
        }
        await_r = cloud_aliyun_at_property_post_await(CLOUD_PROP_POST_AWAIT_MS);
        if(await_r == 1u) {
            cloud_prop_confirmed_from_payload(payload);
            return 1u;
        }
        if(await_r == 2u) {
            /* SEND 成功但未收到 post_reply：不再重复发，避免阿里云双份 */
            cloud_prop_confirmed_from_payload(payload);
            return 1u;
        }
        cloud_aliyun_at_pump_mqtt_ctrl();
    }
    if(s_prop_retry_cnt == 0u || strcmp(s_prop_retry[0], payload) != 0) {
        (void)cloud_prop_retry_enqueue(payload);
    }
    return 0u;
}

int cloud_ota_service_publish_flash_property_only(const char *json, void *ctx)
{
    uint8_t prop_ok;

    (void)ctx;
    prop_ok = cloud_unlock_publish_property_resilient(json);
    return (prop_ok != 0u) ? 1 : 0;
}

void cloud_ota_service_clear_property_retry(void)
{
    s_prop_retry_cnt = 0u;
    s_prop_retry_last_ms = 0u;
}

void cloud_ota_service_on_mqtt_online(void)
{
    s_prop_flush_earliest_ms = HAL_GetTick();
    cloud_ota_service_flush_property_retry();
}

void cloud_ota_service_flush_property_retry(void)
{
    uint32_t now = HAL_GetTick();

    if(s_prop_retry_cnt == 0u) {
        return;
    }
    if(cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    if(s_prop_flush_earliest_ms != 0u && now < s_prop_flush_earliest_ms) {
        return;
    }
    if(s_prop_retry_last_ms != 0u &&
       (now - s_prop_retry_last_ms) < CLOUD_PROP_RETRY_MIN_MS) {
        return;
    }
    s_prop_retry_last_ms = now;
    if(cloud_unlock_publish_property_resilient(s_prop_retry[0]) != 0u) {
        if(s_prop_retry_cnt > 1u) {
            memmove(&s_prop_retry[0], &s_prop_retry[1],
                    sizeof(s_prop_retry[0]) * (size_t)(s_prop_retry_cnt - 1u));
        }
        s_prop_retry_cnt--;
    } else {
        cloud_prop_persist_backup_from_payload(s_prop_retry[0]);
    }
}

static uint8_t cloud_unlock_publish_terminal_bridge(const char *account, uint8_t method_code,
                                                    uint8_t device_code, const char *time_txt,
                                                    uint32_t seq)
{
    char topic[96];
    char json[360];

    if(time_txt == NULL || time_txt[0] == '\0' || cloud_aliyun_at_is_online() == 0u) {
        return 0u;
    }
    (void)snprintf(topic, sizeof(topic), "/%s/%s%s",
                   APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
                   APP_BRIDGE_TOPIC_TERMINAL_PUSH);
    if(method_code == 5u && s_unlock_guest_acc[0] != '\0') {
        (void)snprintf(json, sizeof(json),
                       "{\"cmd\":\"unlock_record\",\"unlock_account\":\"%s\","
                       "\"guest_account\":\"%s\","
                       "\"unlock_time\":\"%s\",\"unlock_method\":%u,\"unlock_device\":%u,"
                       "\"id\":\"%lu\"}",
                       account, s_unlock_guest_acc, time_txt,
                       (unsigned)method_code, (unsigned)device_code,
                       (unsigned long)seq);
        s_unlock_guest_acc[0] = '\0';
    } else {
        (void)snprintf(json, sizeof(json),
                       "{\"cmd\":\"unlock_record\",\"unlock_account\":\"%s\","
                       "\"unlock_time\":\"%s\",\"unlock_method\":%u,\"unlock_device\":%u,"
                       "\"id\":\"%lu\"}",
                       account, time_txt,
                       (unsigned)method_code, (unsigned)device_code,
                       (unsigned long)seq);
    }
    return (cloud_aliyun_at_mqtt_publish_qos0(topic, json) != 0u) ? 1u : 0u;
}

/** 开锁热路径：仅同步发 App 桥接，物模型入后台重试队列，不阻塞 GUI */
static uint8_t cloud_unlock_publish_live_fast(const char *account, uint8_t method_code,
                                              uint8_t device_code, const char *time_txt,
                                              uint32_t seq)
{
    char payload[400];
    uint8_t bridge_ok;

    (void)snprintf(payload, sizeof(payload),
                   "{\"method\":\"thing.event.property.post\",\"id\":%lu,"
                   "\"params\":{\"unlock_account\":\"%s\",\"unlock_time\":\"%s\","
                   "\"unlock_method\":%u,\"unlock_device\":%u},\"version\":\"1.0\"}",
                   (unsigned long)seq, account, time_txt,
                   (unsigned)method_code, (unsigned)device_code);
    bridge_ok = cloud_unlock_publish_terminal_bridge(account, method_code, device_code,
                                                     time_txt, seq);
    /* 物模型延后到 poll 发，避免与 terminal/push 连续 CIPSEND 导致 MQTT 掉线 */
    (void)cloud_prop_retry_enqueue(payload);
    return bridge_ok;
}

static uint8_t cloud_unlock_publish_all(const char *account, uint8_t method_code,
                                        uint8_t device_code, const char *time_txt,
                                        uint32_t seq)
{
    char payload[400];
    uint8_t bridge_ok;
    uint8_t prop_ok;

    (void)snprintf(payload, sizeof(payload),
                   "{\"method\":\"thing.event.property.post\",\"id\":%lu,"
                   "\"params\":{\"unlock_account\":\"%s\",\"unlock_time\":\"%s\","
                   "\"unlock_method\":%u,\"unlock_device\":%u},\"version\":\"1.0\"}",
                   (unsigned long)seq, account, time_txt,
                   (unsigned)method_code, (unsigned)device_code);
    bridge_ok = cloud_unlock_publish_terminal_bridge(account, method_code, device_code,
                                                     time_txt, seq);
    prop_ok = cloud_unlock_publish_property_resilient(payload);
    return (bridge_ok != 0u || prop_ok != 0u) ? 1u : 0u;
}

static int cloud_unlock_drain_cb(const char *json, void *ctx)
{
    uint8_t prop_ok;
    uint8_t bridge_ok = 0u;
    char account[13] = "";
    char time_txt[24] = "";
    unsigned method_code = 0u;
    unsigned device_code = 1u;
    unsigned long seq = 0ul;
    const char *p;

    (void)ctx;
    if(json == NULL) {
        return 0;
    }
    p = strstr(json, "\"unlock_account\":\"");
    if(p != NULL) {
        (void)sscanf(p + 18, "%12[^\"]", account);
    }
    p = strstr(json, "\"unlock_time\":\"");
    if(p != NULL) {
        (void)sscanf(p + 15, "%23[^\"]", time_txt);
    }
    p = strstr(json, "\"unlock_method\":");
    if(p != NULL) {
        (void)sscanf(p + 16, "%u", &method_code);
    }
    p = strstr(json, "\"unlock_device\":");
    if(p != NULL) {
        (void)sscanf(p + 16, "%u", &device_code);
    }
    p = strstr(json, "\"id\":");
    if(p != NULL) {
        (void)sscanf(p + 5, "%lu", &seq);
    }
    if(time_txt[0] != '\0' && method_code > 0u) {
        bridge_ok = cloud_unlock_publish_terminal_bridge(account, (uint8_t)method_code,
                                                         (uint8_t)device_code, time_txt,
                                                         (uint32_t)seq);
    }
    prop_ok = cloud_unlock_publish_property_resilient(json);
    if(prop_ok != 0u) {
        cloud_prop_confirmed_from_payload(json);
    }
    return (prop_ok != 0u || bridge_ok != 0u) ? 1 : 0;
}

int cloud_ota_service_publish_flash_json(const char *json)
{
    return cloud_unlock_drain_cb(json, NULL);
}

static uint8_t cloud_unlock_time_ready(void)
{
    return (cloud_aliyun_at_time_is_synced() != 0u && app_wall_clock_valid() != 0u) ? 1u : 0u;
}

void cloud_ota_service_init(void)
{
    s_unlock_seq = 0u;
    s_prop_last_confirmed_seq = 0u;
    s_prop_last_confirmed_ms = 0u;
    app_unlock_flash_init();
    app_temp_password_init();
    app_cloud_session_init();
    app_cloud_command_init();
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
    cloud_ota_service_flush_property_retry();
    (void)app_unlock_flash_upload_next(cloud_unlock_drain_cb, NULL);
    if(s_prop_retry_cnt == 0u) {
        (void)app_unlock_flash_upload_next_property(cloud_ota_service_publish_flash_property_only,
                                                    NULL);
    }
    n = app_unlock_flash_count();
    (void)n;
}

void cloud_ota_service_tick_1ms(void)
{
}

static void cloud_ota_flush_deferred_unlock(void)
{
    char acc[sizeof(s_defer_unlock_acc)];
    char mtd[sizeof(s_defer_unlock_mtd)];
    uint32_t uptime_ms;
    int unlock_device;

    if(s_defer_unlock_pending == 0u) {
        return;
    }
    strncpy(acc, s_defer_unlock_acc, sizeof(acc) - 1u);
    acc[sizeof(acc) - 1u] = '\0';
    strncpy(mtd, s_defer_unlock_mtd, sizeof(mtd) - 1u);
    mtd[sizeof(mtd) - 1u] = '\0';
    uptime_ms = s_defer_unlock_uptime;
    unlock_device = s_defer_unlock_device;
    s_defer_unlock_pending = 0u;
    cloud_ota_service_report_unlock_record_ex(acc, mtd, uptime_ms, unlock_device);
}

void cloud_ota_service_queue_unlock_report_ex(const char *account, const char *method,
                                              uint32_t uptime_ms, int unlock_device)
{
    const char *acc = (account != NULL) ? account : "";
    const char *mtd = (method != NULL) ? method : "unknown";

    strncpy(s_defer_unlock_acc, acc, sizeof(s_defer_unlock_acc) - 1u);
    s_defer_unlock_acc[sizeof(s_defer_unlock_acc) - 1u] = '\0';
    strncpy(s_defer_unlock_mtd, mtd, sizeof(s_defer_unlock_mtd) - 1u);
    s_defer_unlock_mtd[sizeof(s_defer_unlock_mtd) - 1u] = '\0';
    s_defer_unlock_uptime = uptime_ms;
    s_defer_unlock_device = unlock_device;
    s_defer_unlock_pending = 1u;
}

void cloud_ota_service_poll_5ms(void)
{
    app_temp_password_tick();
    if(g_app_scr == APP_SCR_11 && app_wifi_exclusive_mode() != 0u &&
       cloud_aliyun_at_wifi_link_ready() == 0u &&
       app_link_guard_mqtt() == 0u &&
       cloud_aliyun_at_wifi_bringup_active() == 0u) {
        return;
    }
    app_cloud_flush_deferred_actions();
    cloud_ota_flush_deferred_unlock();
    cloud_ota_service_flush_property_retry();
    app_cloud_session_poll();
    app_cloud_command_poll();
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
    if(strcmp(method, "remote") == 0 || strcmp(method, "phone") == 0) {
        return 4u;
    }
    if(strcmp(method, "temporary-password") == 0 || strcmp(method, "temporary") == 0) {
        return 5u;
    }
    return 0u;
}

extern user_cred_t g_users[APP_USER_MAX];
extern uint8_t g_user_count;
extern uint8_t g_default_admin_deleted;
extern uint8_t g_default_admin_is_admin_role;
extern uint8_t g_default_admin_has_nfc;
extern uint8_t g_default_admin_has_fp;
extern char g_default_admin_account[13];
extern char g_default_admin_password[11];

static void cloud_user_cred_flags(const char *acc, uint8_t *is_admin,
                                    uint8_t *has_pwd, uint8_t *has_nfc, uint8_t *has_fp)
{
    int idx;

    if(is_admin != NULL) {
        *is_admin = 0u;
    }
    if(has_pwd != NULL) {
        *has_pwd = 0u;
    }
    if(has_nfc != NULL) {
        *has_nfc = 0u;
    }
    if(has_fp != NULL) {
        *has_fp = 0u;
    }
    if(acc == NULL || acc[0] == '\0') {
        return;
    }

    if(g_default_admin_deleted == 0u && strcmp(acc, g_default_admin_account) == 0) {
        if(is_admin != NULL) {
            *is_admin = g_default_admin_is_admin_role;
        }
        if(has_pwd != NULL) {
            *has_pwd = (g_default_admin_password[0] != '\0') ? 1u : 0u;
        }
        if(has_nfc != NULL) {
            *has_nfc = g_default_admin_has_nfc;
        }
        if(has_fp != NULL) {
            *has_fp = g_default_admin_has_fp;
        }
        return;
    }

    idx = users_find_index_by_acc(acc);
    if(idx < 0 || g_users[idx].active == 0u) {
        return;
    }
    if(is_admin != NULL) {
        *is_admin = g_users[idx].is_admin;
    }
    if(has_pwd != NULL) {
        *has_pwd = (g_users[idx].pwd[0] != '\0') ? 1u : 0u;
    }
    if(has_nfc != NULL) {
        *has_nfc = g_users[idx].has_nfc;
    }
    if(has_fp != NULL) {
        *has_fp = g_users[idx].has_fp;
    }
}

void cloud_ota_publish_user_changed(const char *action, const char *account)
{
    char topic[96];
    char json[220];
    uint8_t attempt;
    uint8_t is_admin = 0u;
    uint8_t has_pwd = 0u;
    uint8_t has_nfc = 0u;
    uint8_t has_fp = 0u;
    const char *acc;
    const char *act;

    if(cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    act = (action != NULL && action[0] != '\0') ? action : "update";
    acc = (account != NULL && account[0] != '\0') ? account : "";
    if(acc[0] == '\0') {
        return;
    }

    if(strcmp(act, "delete") != 0) {
        cloud_user_cred_flags(acc, &is_admin, &has_pwd, &has_nfc, &has_fp);
    }

    (void)snprintf(topic, sizeof(topic), "/%s/%s%s",
                   APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
                   APP_BRIDGE_TOPIC_TERMINAL_PUSH);
    (void)snprintf(json, sizeof(json),
                   "{\"cmd\":\"user_changed\",\"action\":\"%s\",\"account\":\"%s\","
                   "\"isAdmin\":%u,\"hasPwd\":%u,\"hasNfc\":%u,\"hasFp\":%u,"
                   "\"id\":\"%lu\"}",
                   act,
                   acc,
                   (unsigned)is_admin,
                   (unsigned)has_pwd,
                   (unsigned)has_nfc,
                   (unsigned)has_fp,
                   (unsigned long)HAL_GetTick());
    for(attempt = 0u; attempt < 3u; attempt++) {
        if(cloud_aliyun_at_mqtt_publish_qos0(topic, json) != 0u) {
            return;
        }
        cloud_aliyun_at_pump_mqtt_ctrl();
    }
}

void cloud_ota_service_report_event(cloud_event_t evt, const char *account)
{
    const char *acc;

    acc = (account != NULL) ? account : "";
    switch(evt) {
    case CLOUD_EVT_USER_ADD_OK:
        cloud_ota_publish_user_changed("add", acc);
        break;
    case CLOUD_EVT_USER_DELETE_OK:
        cloud_ota_publish_user_changed("delete", acc);
        break;
    case CLOUD_EVT_USER_PWD_CHANGE_OK:
        cloud_ota_publish_user_changed("password", acc);
        break;
    case CLOUD_EVT_USER_ROLE_CHANGE:
        cloud_ota_publish_user_changed("role", acc);
        break;
    case CLOUD_EVT_NFC_BIND_OK:
        cloud_ota_publish_user_changed("nfc_bind", acc);
        break;
    case CLOUD_EVT_NFC_DELETE_OK:
        cloud_ota_publish_user_changed("nfc_delete", acc);
        break;
    default:
        break;
    }
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
    char time_txt[24];

    if(cloud_unlock_format_time_text(time_txt, sizeof(time_txt), unlock_time_sec, uptime_sec) == 0u) {
        if(device_code == 2u) {
            UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] slave try pub no time\r\n");
        } else {
            UNLOCK_CLOUD_TRACE_MSG("[UNLOCK] host try pub no time\r\n");
        }
        return 0u;
    }
    if(cloud_unlock_publish_live_fast(account, method_code, device_code, time_txt, seq) == 0u) {
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
    const char *pub_acc;
    const char *mtd = (method != NULL) ? method : "unknown";
    uint8_t method_code = cloud_method_to_code(mtd);
    uint8_t device_code;
    uint32_t unlock_time_sec;
    uint32_t uptime_sec = uptime_ms / 1000u;
    uint32_t seq;

    if(unlock_device == CLOUD_UNLOCK_DEVICE_PHONE) {
        device_code = 3u;
    } else if(unlock_device == CLOUD_UNLOCK_DEVICE_SLAVE) {
        device_code = 2u;
    } else {
        device_code = 1u;
    }

    pub_acc = (method_code == 5u) ? "temporary account" : acc;

    seq = ++s_unlock_seq;

#if (APP_HOST_SLAVE_UNLOCK_CLOUD_TRACE != 0)
    {
        char ent[96];
        (void)snprintf(ent, sizeof(ent),
                       "[UNLOCK] report dev=%s code=%u acc=%s mtd=%s seq=%lu\r\n",
                       (unlock_device == CLOUD_UNLOCK_DEVICE_SLAVE) ? "slave" : "host",
                       (unsigned)device_code, pub_acc, mtd, (unsigned long)seq);
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
        app_unlock_flash_append(pub_acc, method_code, device_code, 0u, uptime_sec, seq);
        cloud_aliyun_at_invalidate_unlock_flush();
        return;
    }

    unlock_time_sec = app_wall_clock_epoch_sec();

    if(cloud_aliyun_at_wifi_link_ready() != 0u &&
       cloud_aliyun_at_is_online() != 0u &&
       cloud_unlock_try_publish_now(pub_acc, method_code, device_code, seq, unlock_time_sec,
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
    app_unlock_flash_append(pub_acc, method_code, device_code, unlock_time_sec, uptime_sec, seq);
    cloud_aliyun_at_invalidate_unlock_flush();
}

