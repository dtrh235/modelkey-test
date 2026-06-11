#include "app_cloud_bind_cmd.h"

#include "app_config.h"
#include "app_pair_bind.h"
#include "app_temp_password.h"
#include "app_unlock_event.h"
#include "app_user_ops.h"
#include "user_auth_portable.h"
#include "cloud_aliyun_at.h"
#if (APP_RS485_ENABLE == 1)
#include "app_rs485_proto.h"
#endif

extern user_cred_t g_users[APP_USER_MAX];
extern uint8_t g_user_count;
extern uint8_t g_default_admin_deleted;
extern uint8_t g_default_admin_is_admin_role;
extern char g_default_admin_account[13];
extern char g_default_admin_password[11];
extern uint8_t g_default_admin_has_nfc;
extern uint8_t g_default_admin_has_fp;
#include "stm32f4xx_hal.h"
#include "SYSTEM/usart/usart.h"

#include <stdio.h>
#include <string.h>

#if (APP_CLOUD_COMMAND_ENABLE == 0)

uint8_t app_cloud_bind_handle_json(const char *json, uint16_t len)
{
    (void)json;
    (void)len;
    return 0u;
}

#else

#if (APP_CLOUD_COMMAND_TRACE != 0)
#define BIND_LOG(msg) usart_debug_tx_str(msg)
#define BIND_LOGF(...) do { \
        char _bind_log[160]; \
        (void)snprintf(_bind_log, sizeof(_bind_log), __VA_ARGS__); \
        usart_debug_tx_str(_bind_log); \
    } while(0)
#else
#define BIND_LOG(msg) ((void)0)
#define BIND_LOGF(...) ((void)0)
#endif

static uint8_t bind_json_extract_string(const char *json, const char *key, char *out, size_t out_sz)
{
    char needle[40];
    const char *p;
    const char *start;
    size_t n;

    if(json == NULL || key == NULL || out == NULL || out_sz == 0u) {
        return 0u;
    }
    out[0] = '\0';
    (void)snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(json, needle);
    if(p == NULL) {
        return 0u;
    }
    p = strchr(p + strlen(needle), ':');
    if(p == NULL) {
        return 0u;
    }
    p++;
    while(*p == ' ' || *p == '\t') {
        p++;
    }
    if(*p != '"') {
        return 0u;
    }
    start = p + 1;
    p = start;
    while(*p != '\0' && *p != '"') {
        p++;
    }
    if(*p != '"') {
        return 0u;
    }
    n = (size_t)(p - start);
    if(n >= out_sz) {
        n = out_sz - 1u;
    }
    memcpy(out, start, n);
    out[n] = '\0';
    return (out[0] != '\0') ? 1u : 0u;
}

static uint8_t bind_json_extract_u64(const char *json, const char *key, unsigned long long *out)
{
    char needle[40];
    const char *p;

    if(json == NULL || key == NULL || out == NULL) {
        return 0u;
    }
    *out = 0ull;
    (void)snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(json, needle);
    if(p == NULL) {
        return 0u;
    }
    p = strchr(p + strlen(needle), ':');
    if(p == NULL) {
        return 0u;
    }
    p++;
    while(*p == ' ' || *p == '\t') {
        p++;
    }
    if(sscanf(p, "%llu", out) != 1) {
        return 0u;
    }
    return 1u;
}

#define CLOUD_USER_LIST_DEBOUNCE_MS  8000u

static char s_user_list_pending_req[24];
static volatile uint8_t s_user_list_pending;
static uint32_t s_user_list_last_pub_ms;

static void app_cloud_publish_user_list_now(const char *req_id)
{
    static char json[3600];
    char topic[96];
    const char *id;
    int pos;
    uint8_t i;
    uint8_t first = 1u;
    uint32_t now;

    if(cloud_aliyun_at_is_online() == 0u) {
        return;
    }

    now = HAL_GetTick();
    s_user_list_last_pub_ms = now;

    id = (req_id != NULL && req_id[0] != '\0') ? req_id : "";
    pos = snprintf(json, sizeof(json),
                   "{\"cmd\":\"user_list\",\"id\":\"%s\",\"users\":[", id);
    if(pos < 0 || (size_t)pos >= sizeof(json)) {
        return;
    }

    if(g_default_admin_deleted == 0u && g_default_admin_account[0] != '\0') {
        pos += snprintf(json + pos, sizeof(json) - (size_t)pos,
                        "%s{\"acc\":\"%s\",\"isAdmin\":%u,\"hasPwd\":%u,\"hasNfc\":%u,\"hasFp\":%u}",
                        first ? "" : ",",
                        g_default_admin_account,
                        g_default_admin_is_admin_role ? 1u : 0u,
                        (g_default_admin_password[0] != '\0') ? 1u : 0u,
                        g_default_admin_has_nfc ? 1u : 0u,
                        g_default_admin_has_fp ? 1u : 0u);
        first = 0u;
    }

    for(i = 0u; i < g_user_count; i++) {
        if(g_users[i].active == 0u) {
            continue;
        }
        if(g_default_admin_deleted == 0u &&
           strcmp(g_users[i].acc, g_default_admin_account) == 0) {
            continue;
        }
        if(pos < 0 || (size_t)pos >= sizeof(json) - 96u) {
            break;
        }
        pos += snprintf(json + pos, sizeof(json) - (size_t)pos,
                        "%s{\"acc\":\"%s\",\"isAdmin\":%u,\"hasPwd\":%u,\"hasNfc\":%u,\"hasFp\":%u}",
                        first ? "" : ",",
                        g_users[i].acc,
                        g_users[i].is_admin ? 1u : 0u,
                        (g_users[i].pwd[0] != '\0') ? 1u : 0u,
                        g_users[i].has_nfc ? 1u : 0u,
                        g_users[i].has_fp ? 1u : 0u);
        first = 0u;
    }

    if(pos < 0 || (size_t)pos + 4u >= sizeof(json)) {
        return;
    }
    (void)snprintf(json + pos, sizeof(json) - (size_t)pos, "]}");

    (void)snprintf(topic, sizeof(topic), "/%s/%s%s",
                   APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
                   APP_BRIDGE_TOPIC_TERMINAL_PUSH);
    (void)cloud_aliyun_at_mqtt_publish_qos0(topic, json);
    BIND_LOG("[CLOUD][USER] list published\r\n");
}

static void app_cloud_publish_user_list(const char *req_id)
{
    uint32_t now;

    if(cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    now = HAL_GetTick();
    if(s_user_list_last_pub_ms != 0u &&
       (now - s_user_list_last_pub_ms) < CLOUD_USER_LIST_DEBOUNCE_MS) {
        if(req_id != NULL && req_id[0] != '\0') {
            strncpy(s_user_list_pending_req, req_id, sizeof(s_user_list_pending_req) - 1u);
            s_user_list_pending_req[sizeof(s_user_list_pending_req) - 1u] = '\0';
        }
        s_user_list_pending = 1u;
        return;
    }
    app_cloud_publish_user_list_now(req_id);
}

void app_cloud_bind_flush_pending_user_list(void)
{
    uint32_t now;
    char req[sizeof(s_user_list_pending_req)];

    if(s_user_list_pending == 0u || cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    now = HAL_GetTick();
    if(s_user_list_last_pub_ms != 0u &&
       (now - s_user_list_last_pub_ms) < CLOUD_USER_LIST_DEBOUNCE_MS) {
        return;
    }
    strncpy(req, s_user_list_pending_req, sizeof(req) - 1u);
    req[sizeof(req) - 1u] = '\0';
    s_user_list_pending = 0u;
    s_user_list_pending_req[0] = '\0';
    app_cloud_publish_user_list_now(req);
}

void app_cloud_publish_user_list_snapshot(void)
{
    app_cloud_publish_user_list("");
}

static volatile uint8_t s_defer_temp_used_pending;
static char s_defer_temp_used_acc[13];

void app_cloud_queue_temp_password_used(const char *guest_account)
{
    if(guest_account == NULL || guest_account[0] == '\0') {
        return;
    }
    strncpy(s_defer_temp_used_acc, guest_account, sizeof(s_defer_temp_used_acc) - 1u);
    s_defer_temp_used_acc[sizeof(s_defer_temp_used_acc) - 1u] = '\0';
    s_defer_temp_used_pending = 1u;
}

void app_cloud_flush_deferred_actions(void)
{
    char acc[sizeof(s_defer_temp_used_acc)];

    app_cloud_bind_flush_pending_user_list();
    if(s_defer_temp_used_pending == 0u) {
        return;
    }
    strncpy(acc, s_defer_temp_used_acc, sizeof(acc) - 1u);
    acc[sizeof(acc) - 1u] = '\0';
    s_defer_temp_used_pending = 0u;
    app_cloud_publish_temp_password_used(acc);
}

void app_cloud_publish_temp_password_used(const char *guest_account)
{
    char topic[96];
    char json[128];

    if(guest_account == NULL || guest_account[0] == '\0' || cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    (void)snprintf(topic, sizeof(topic), "/%s/%s%s",
                   APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
                   APP_BRIDGE_TOPIC_TERMINAL_PUSH);
    (void)snprintf(json, sizeof(json),
                   "{\"cmd\":\"temp_password_used\",\"account\":\"%s\",\"id\":\"%lu\"}",
                   guest_account, (unsigned long)HAL_GetTick());
    if(cloud_aliyun_at_mqtt_publish_qos0(topic, json) != 0u) {
        BIND_LOGF("[CLOUD][TEMP] used acc=%s\r\n", guest_account);
        return;
    }
    BIND_LOGF("[CLOUD][TEMP] used pub fail acc=%s\r\n", guest_account);
}

static void cloud_publish_cmd_ack(const char *ack_cmd, uint8_t success, const char *req_id)
{
    char topic[96];
    char json[160];
    const char *id;
    uint8_t try;

    if(ack_cmd == NULL || cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    id = (req_id != NULL && req_id[0] != '\0') ? req_id : "";
    (void)snprintf(topic, sizeof(topic), "/%s/%s%s",
                   APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
                   APP_BRIDGE_TOPIC_TERMINAL_PUSH);
    (void)snprintf(json, sizeof(json),
                   "{\"cmd\":\"%s\",\"ok\":%u,\"id\":\"%s\"}",
                   ack_cmd,
                   success ? 1u : 0u,
                   id);
    for(try = 0u; try < 3u; try++) {
        if(cloud_aliyun_at_mqtt_publish_qos0(topic, json) != 0u) {
            return;
        }
        cloud_aliyun_at_pump_mqtt_ctrl();
    }
}

uint8_t app_cloud_bind_handle_json(const char *json, uint16_t len)
{
    char cmd[32];
    char pair_code[APP_PAIR_CODE_LEN + 1u];
    char app_account[13];
    char req_id[24];

    if(json == NULL || len == 0u) {
        return 0u;
    }
    if(bind_json_extract_string(json, "cmd", cmd, sizeof(cmd)) == 0u) {
        return 0u;
    }

    req_id[0] = '\0';
    (void)bind_json_extract_string(json, "id", req_id, sizeof(req_id));

    if(strcmp(cmd, "unbind") == 0) {
        app_pair_clear_bind();
        cloud_publish_cmd_ack("unbind_ack", 1u, req_id);
        BIND_LOG("[CLOUD][BIND] unbind ok\r\n");
        return 1u;
    }

    if(strcmp(cmd, "unlock") == 0) {
        app_account[0] = '\0';
        (void)bind_json_extract_string(json, "appAccount", app_account, sizeof(app_account));
        if(app_pair_remote_unlock_allowed(app_account) == 0u) {
            cloud_publish_cmd_ack("unlock_ack", 0u, req_id);
            BIND_LOGF("[CLOUD][UNLOCK] reject acc=%s\r\n",
                      (app_account[0] != '\0') ? app_account : "-");
            return 1u;
        }
        /* 先回 unlock_ack，勿被后续物模型 property/post 阻塞拖过 App 8s 超时 */
        cloud_publish_cmd_ack("unlock_ack", 1u, req_id);
        app_unlock_event_handle_success(APP_UNLOCK_POPUP_HOME, "0", "remote");
        BIND_LOGF("[CLOUD][UNLOCK] ok acc=%s\r\n", app_account);
        return 1u;
    }

    if(strcmp(cmd, "set_temp_password") == 0) {
        char password[APP_TEMP_PWD_MAX + 1u];
        unsigned long long valid_sec = 300ull;

        app_account[0] = '\0';
        password[0] = '\0';
        (void)bind_json_extract_string(json, "account", app_account, sizeof(app_account));
        (void)bind_json_extract_string(json, "password", password, sizeof(password));
        (void)bind_json_extract_u64(json, "validSeconds", &valid_sec);
        if(valid_sec < 60ull || valid_sec > 3600ull) {
            valid_sec = 300ull;
        }
        if(app_temp_password_add(app_account, password, (uint32_t)valid_sec) != 0u) {
#if (APP_RS485_ENABLE == 1)
            (void)app_rs485_proto_slave_temp_password_set(app_account, password, (uint32_t)valid_sec, 600u);
#endif
            cloud_publish_cmd_ack("temp_password_ack", 1u, req_id);
            BIND_LOGF("[CLOUD][TEMP] set ok acc=%s\r\n", app_account);
        } else {
            cloud_publish_cmd_ack("temp_password_ack", 0u, req_id);
            BIND_LOGF("[CLOUD][TEMP] set fail acc=%s\r\n", app_account);
        }
        return 1u;
    }

    if(strcmp(cmd, "user_add") == 0) {
        char password[11];
        unsigned long long is_admin = 0ull;

        app_account[0] = '\0';
        password[0] = '\0';
        (void)bind_json_extract_string(json, "account", app_account, sizeof(app_account));
        (void)bind_json_extract_string(json, "password", password, sizeof(password));
        (void)bind_json_extract_u64(json, "isAdmin", &is_admin);
        if(users_try_register(app_account, password, is_admin != 0ull)) {
            cloud_publish_cmd_ack("user_ack", 1u, req_id);
            BIND_LOGF("[CLOUD][USER] add ok acc=%s\r\n", app_account);
        } else {
            cloud_publish_cmd_ack("user_ack", 0u, req_id);
            BIND_LOGF("[CLOUD][USER] add fail acc=%s\r\n", app_account);
        }
        return 1u;
    }

    if(strcmp(cmd, "user_delete") == 0) {
        app_account[0] = '\0';
        (void)bind_json_extract_string(json, "account", app_account, sizeof(app_account));
        if(users_try_delete_by_acc(app_account)) {
            cloud_publish_cmd_ack("user_ack", 1u, req_id);
            BIND_LOGF("[CLOUD][USER] del ok acc=%s\r\n", app_account);
        } else {
            cloud_publish_cmd_ack("user_ack", 0u, req_id);
            BIND_LOGF("[CLOUD][USER] del fail acc=%s\r\n", app_account);
        }
        return 1u;
    }

    if(strcmp(cmd, "user_set_password") == 0) {
        char password[11];
        char old_password[11];
        uint8_t is_admin_acc = 0u;
        int idx;

        app_account[0] = '\0';
        password[0] = '\0';
        old_password[0] = '\0';
        (void)bind_json_extract_string(json, "account", app_account, sizeof(app_account));
        (void)bind_json_extract_string(json, "password", password, sizeof(password));
        (void)bind_json_extract_string(json, "oldPassword", old_password, sizeof(old_password));
        idx = users_find_index_by_acc(app_account);
        if(idx >= 0 && g_users[idx].is_admin != 0u) {
            is_admin_acc = 1u;
        } else if(g_default_admin_deleted == 0u &&
                  strcmp(app_account, g_default_admin_account) == 0) {
            is_admin_acc = 1u;
        }
        if(is_admin_acc != 0u) {
            if(old_password[0] == '\0' ||
               unlock_credentials_match_with_delete(app_account, old_password) == 0u) {
                cloud_publish_cmd_ack("user_ack", 0u, req_id);
                BIND_LOGF("[CLOUD][USER] pwd reject bad old acc=%s\r\n", app_account);
                return 1u;
            }
        }
        if(users_set_password_by_acc(app_account, password)) {
            cloud_publish_cmd_ack("user_ack", 1u, req_id);
            BIND_LOGF("[CLOUD][USER] pwd ok acc=%s\r\n", app_account);
        } else {
            cloud_publish_cmd_ack("user_ack", 0u, req_id);
            BIND_LOGF("[CLOUD][USER] pwd fail acc=%s\r\n", app_account);
        }
        return 1u;
    }

    if(strcmp(cmd, "sync_users") == 0) {
        app_cloud_publish_user_list(req_id);
        return 1u;
    }

    if(strcmp(cmd, "revoke_temp_password") == 0) {
        app_account[0] = '\0';
        (void)bind_json_extract_string(json, "account", app_account, sizeof(app_account));
        if(app_temp_password_revoke(app_account) != 0u) {
#if (APP_RS485_ENABLE == 1)
            (void)app_rs485_proto_slave_temp_password_revoke(app_account, 600u);
#endif
            cloud_publish_cmd_ack("temp_password_ack", 1u, req_id);
            BIND_LOGF("[CLOUD][TEMP] revoke ok acc=%s\r\n", app_account);
        } else {
            cloud_publish_cmd_ack("temp_password_ack", 0u, req_id);
            BIND_LOGF("[CLOUD][TEMP] revoke miss acc=%s\r\n", app_account);
        }
        return 1u;
    }

    if(strcmp(cmd, "bind") != 0) {
        return 0u;
    }

    if(cloud_aliyun_at_is_online() == 0u) {
        cloud_publish_cmd_ack("bind_ack", 0u, req_id);
        BIND_LOG("[CLOUD][BIND] reject cloud offline\r\n");
        return 1u;
    }

    app_account[0] = '\0';
    if(bind_json_extract_string(json, "pairCode", pair_code, sizeof(pair_code)) == 0u &&
       bind_json_extract_string(json, "pair_code", pair_code, sizeof(pair_code)) == 0u) {
        BIND_LOG("[CLOUD][BIND] missing pairCode\r\n");
        return 1u;
    }
    (void)bind_json_extract_string(json, "appAccount", app_account, sizeof(app_account));

    if(app_pair_try_bind(pair_code, app_account) != 0u) {
        app_pair_publish_bind_ack(1u, req_id);
        BIND_LOGF("[CLOUD][BIND] ok code=%s acc=%s\r\n", pair_code,
                  (app_account[0] != '\0') ? app_account : "-");
        return 1u;
    }

    app_pair_publish_bind_ack(0u, req_id);
    BIND_LOGF("[CLOUD][BIND] reject code=%s\r\n", pair_code);
    return 1u;
}

#endif /* APP_CLOUD_COMMAND_ENABLE */
