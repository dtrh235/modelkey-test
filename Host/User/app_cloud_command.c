#include "app_cloud_command.h"

#include "app_config.h"
#include "app_cloud_bind_cmd.h"
#include "cloud_aliyun_at.h"
#include "SYSTEM/usart/usart.h"
#include "stm32f4xx_hal.h"

#include <stdio.h>
#include <string.h>

#if (APP_CLOUD_COMMAND_ENABLE == 0)

void app_cloud_command_init(void) {}
void app_cloud_command_reset(void) {}
void app_cloud_command_poll(void) {}
void app_cloud_command_on_suback(void) {}
void app_cloud_command_on_suback_for_pkt(uint16_t pkt_id) { (void)pkt_id; }
uint8_t app_cloud_command_waiting_suback(void) { return 0u; }
uint8_t app_cloud_command_is_ready(void) { return 0u; }
uint8_t app_cloud_command_on_mqtt_publish(const uint8_t *buf, uint16_t len)
{
    (void)buf;
    (void)len;
    return 0u;
}
void app_cloud_command_diag_status(char *buf, size_t buf_sz)
{
    if(buf != NULL && buf_sz > 0u) {
        buf[0] = '\0';
    }
}

#else

#if (APP_CLOUD_COMMAND_TRACE != 0)
#define CMD_LOG(msg) usart_debug_tx_str(msg)
#define CMD_LOGF(...) do { \
        char _cmd_log[200]; \
        (void)snprintf(_cmd_log, sizeof(_cmd_log), __VA_ARGS__); \
        usart_debug_tx_str(_cmd_log); \
    } while(0)
#else
#define CMD_LOG(msg) ((void)0)
#define CMD_LOGF(...) ((void)0)
#endif

#define CMD_SUBACK_RETRY_MS       10000u
#define CMD_DIAG_INTERVAL_MS      2000u
/** 无业务上行时定期 presence，供后端/App 感知在线（非物模型 heartbeat） */
#define CMD_PRESENCE_IDLE_MS      25000u
/* NTP 未同步时最多等待此时长仍订阅 terminal/get（避免云端在线但永远不可配对） */
#define CMD_NTP_WAIT_MAX_MS       30000u
static char s_cmd_topic[96];
static char s_push_topic[96];
static uint8_t s_cmd_sub_sent;
static uint8_t s_cmd_sub_ok;
static uint16_t s_cmd_sub_pkt_id;
static uint32_t s_cmd_sub_sent_ms;
static uint32_t s_cmd_sub_diag_ms;
static uint32_t s_cmd_sub_fail_log_ms;
static uint16_t s_cmd_sub_fail_count;
static uint32_t s_cmd_ntp_wait_since_ms;
static uint32_t s_presence_idle_ms;

static uint8_t cmd_buf_has_ntp_json(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    const char key[] = "serverSendTime";

    if(buf == NULL || len < 16u) {
        return 0u;
    }
    for(i = 0u; i + sizeof(key) - 1u <= len; i++) {
        if(memcmp(buf + i, key, sizeof(key) - 1u) == 0) {
            return 1u;
        }
    }
    return 0u;
}

static void cmd_log_payload_preview(const uint8_t *payload, uint16_t payload_len)
{
    char line[168];
    uint16_t n;
    uint16_t i;

    if(payload == NULL || payload_len == 0u) {
        CMD_LOG("[CLOUD][CMD] recv empty payload\r\n");
        return;
    }
    n = payload_len;
    if(n > 96u) {
        n = 96u;
    }
    (void)snprintf(line, sizeof(line), "[CLOUD][CMD] recv len=%u preview=", (unsigned)payload_len);
    CMD_LOG(line);
    for(i = 0u; i < n; i++) {
        char ch[2];
        ch[0] = (char)((payload[i] >= 32 && payload[i] <= 126) ? payload[i] : '.');
        ch[1] = '\0';
        usart_debug_tx_str(ch);
    }
    usart_debug_tx_str("\r\n");
}

static const char *memstr(const uint8_t *hay, uint16_t hay_len, const char *needle, uint16_t needle_len)
{
    uint16_t i;

    if(hay == NULL || needle == NULL || needle_len == 0u || hay_len < needle_len) {
        return NULL;
    }
    for(i = 0u; i + needle_len <= hay_len; i++) {
        if(memcmp(hay + i, needle, needle_len) == 0) {
            return (const char *)(hay + i);
        }
    }
    return NULL;
}

static uint8_t cmd_mqtt_publish_is_downlink(const uint8_t *buf, uint16_t len)
{
    if(buf == NULL || len < 4u) {
        return 0u;
    }
    if(buf[0] != 0x30u && buf[0] != 0x31u) {
        return 0u;
    }
    if(cmd_buf_has_ntp_json(buf, len) != 0u) {
        return 0u;
    }
    if(memstr(buf, len, "property/post_reply", 19) != NULL ||
       memstr(buf, len, "thing/event/property", 20) != NULL) {
        return 0u;
    }
    return 1u;
}

static void cmd_build_push_topic(void)
{
    if(s_push_topic[0] == '\0') {
        (void)snprintf(s_push_topic, sizeof(s_push_topic),
                       "/%s/%s%s",
                       APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
                       APP_BRIDGE_TOPIC_TERMINAL_PUSH);
    }
}

static uint8_t cmd_publish_push_json(const char *json, const char *log_msg)
{
    if(json == NULL || cloud_aliyun_at_is_online() == 0u || s_cmd_sub_ok == 0u) {
        return 0u;
    }
    cmd_build_push_topic();
    if(cloud_aliyun_at_mqtt_publish_qos0(s_push_topic, json) != 0u) {
        if(log_msg != NULL) {
            CMD_LOG(log_msg);
        }
        cloud_aliyun_at_pump_mqtt_ctrl();
        return 1u;
    }
    return 0u;
}

static void cmd_publish_presence_online_msg(const char *log_msg)
{
    char json[80];
    uint32_t now = HAL_GetTick();

    (void)snprintf(json, sizeof(json),
                   "{\"cmd\":\"presence\",\"state\":\"online\",\"uptime\":%lu}",
                   (unsigned long)now);
    (void)cmd_publish_push_json(json, log_msg);
}

static void cmd_publish_presence_online(void)
{
    cmd_publish_presence_online_msg("[CLOUD][CMD] presence online\r\n");
}

void app_cloud_command_publish_offline_best_effort(void)
{
    char json[56];

    (void)snprintf(json, sizeof(json), "{\"cmd\":\"presence\",\"state\":\"offline\"}");
    (void)cmd_publish_push_json(json, "[CLOUD][CMD] presence offline\r\n");
}

static void cmd_diag_waiting(const char *why)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = (s_cmd_sub_sent_ms != 0u) ? (now - s_cmd_sub_sent_ms) : 0u;

    CMD_LOGF("[CLOUD][CMD] wait suback %s pkt=%u elapsed=%lums\r\n",
             (why != NULL) ? why : "-",
             (unsigned)s_cmd_sub_pkt_id,
             (unsigned long)elapsed);
    cloud_aliyun_at_cmd_diag_log("snap");
    cloud_aliyun_at_cmd_diag_rx_hex();
}

void app_cloud_command_init(void)
{
    (void)snprintf(s_cmd_topic, sizeof(s_cmd_topic),
                   "/%s/%s%s",
                   APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
                   APP_BRIDGE_TOPIC_TERMINAL_GET);
    app_cloud_command_reset();
    CMD_LOGF("[CLOUD][CMD] topic=%s\r\n", s_cmd_topic);
}

void app_cloud_command_reset(void)
{
    s_cmd_sub_sent = 0u;
    s_cmd_sub_ok = 0u;
    s_cmd_sub_pkt_id = 0u;
    s_cmd_sub_sent_ms = 0u;
    s_cmd_sub_diag_ms = 0u;
    s_cmd_sub_fail_log_ms = 0u;
    s_cmd_sub_fail_count = 0u;
    s_cmd_ntp_wait_since_ms = 0u;
    s_presence_idle_ms = 0u;
}

uint8_t app_cloud_command_waiting_suback(void)
{
    return (s_cmd_sub_sent != 0u && s_cmd_sub_ok == 0u) ? 1u : 0u;
}

uint8_t app_cloud_command_is_ready(void)
{
    return s_cmd_sub_ok;
}

void app_cloud_command_diag_status(char *buf, size_t buf_sz)
{
    if(buf == NULL || buf_sz == 0u) {
        return;
    }
    if(s_cmd_sub_ok != 0u) {
        (void)snprintf(buf, buf_sz, "ok");
    } else if(s_cmd_sub_sent != 0u) {
        (void)snprintf(buf, buf_sz, "sent");
    } else if(s_cmd_sub_fail_count > 0u) {
        (void)snprintf(buf, buf_sz, "fail");
    } else {
        (void)snprintf(buf, buf_sz, "idle");
    }
}

void app_cloud_command_on_suback(void)
{
    app_cloud_command_on_suback_for_pkt(s_cmd_sub_pkt_id);
}

void app_cloud_command_on_suback_for_pkt(uint16_t pkt_id)
{
    if(s_cmd_sub_sent != 0u && s_cmd_sub_ok == 0u &&
       pkt_id != 0u && pkt_id == s_cmd_sub_pkt_id) {
        s_cmd_sub_ok = 1u;
        CMD_LOGF("[CLOUD][CMD] subscribe ok pkt=%u\r\n", (unsigned)s_cmd_sub_pkt_id);
        CMD_LOG("[CLOUD][CMD] bridge ready\r\n");
        cmd_publish_presence_online();
        s_presence_idle_ms = HAL_GetTick();
        app_cloud_publish_user_list_snapshot();
    }
}

void app_cloud_command_poll(void)
{
    uint32_t now;

    if(cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    if(s_cmd_sub_ok != 0u) {
        now = HAL_GetTick();
        if(s_presence_idle_ms == 0u ||
           (now - s_presence_idle_ms) >= CMD_PRESENCE_IDLE_MS) {
            s_presence_idle_ms = now;
            cmd_publish_presence_online_msg(NULL);
        }
        app_cloud_bind_flush_pending_user_list();
        cloud_aliyun_at_pump_mqtt_ctrl();
        return;
    }
    if(s_cmd_sub_sent != 0u) {
        cloud_aliyun_at_pump_mqtt_ctrl();
        if(s_cmd_sub_ok != 0u) {
            return;
        }
        now = HAL_GetTick();
        if(s_cmd_sub_diag_ms == 0u ||
           (now - s_cmd_sub_diag_ms) >= CMD_DIAG_INTERVAL_MS) {
            s_cmd_sub_diag_ms = now;
            cmd_diag_waiting("poll");
        }
        if(s_cmd_sub_sent_ms != 0u &&
           (now - s_cmd_sub_sent_ms) >= CMD_SUBACK_RETRY_MS) {
            CMD_LOGF("[CLOUD][CMD] subscribe ack timeout pkt=%u, retry\r\n",
                     (unsigned)s_cmd_sub_pkt_id);
            cloud_aliyun_at_cmd_diag_log("timeout");
            cloud_aliyun_at_cmd_diag_rx_hex();
            s_cmd_sub_sent = 0u;
            s_cmd_sub_sent_ms = 0u;
            s_cmd_sub_diag_ms = 0u;
        }
        return;
    }
    if(cloud_aliyun_at_time_is_synced() == 0u &&
       cloud_aliyun_at_ntp_give_up() == 0u) {
        static uint32_t s_wait_sync_log_ms;

        now = HAL_GetTick();
        if(s_cmd_ntp_wait_since_ms == 0u) {
            s_cmd_ntp_wait_since_ms = now;
        }
        if((now - s_cmd_ntp_wait_since_ms) < CMD_NTP_WAIT_MAX_MS) {
            if(s_wait_sync_log_ms == 0u ||
               (now - s_wait_sync_log_ms) >= 3000u) {
                s_wait_sync_log_ms = now;
                CMD_LOG("[CLOUD][CMD] wait time sync before subscribe\r\n");
                cloud_aliyun_at_cmd_diag_log("wait-ntp");
            }
            return;
        }
        CMD_LOG("[CLOUD][CMD] NTP wait timeout, subscribe anyway\r\n");
    }
    s_cmd_ntp_wait_since_ms = 0u;
    {
        uint16_t pkt_id = 0u;

        now = HAL_GetTick();
        cloud_aliyun_at_cmd_diag_log("pre-sub");
        s_cmd_sub_fail_count = 0u;
        s_cmd_sub_ok = 0u;
        s_cmd_sub_diag_ms = 0u;
        /* SUBACK 可能在 subscribe_qos0 内部 pump 时即到达，须先登记 pkt 再发 SUBSCRIBE */
        pkt_id = cloud_aliyun_at_mqtt_peek_pkt_id();
        s_cmd_sub_pkt_id = pkt_id;
        s_cmd_sub_sent = 1u;
        s_cmd_sub_sent_ms = now;
        CMD_LOGF("[CLOUD][CMD] subscribe send topic=%s\r\n", s_cmd_topic);
        if(cloud_aliyun_at_mqtt_subscribe_qos0(s_cmd_topic, &pkt_id) == 0u) {
            s_cmd_sub_sent = 0u;
            s_cmd_sub_pkt_id = 0u;
            s_cmd_sub_sent_ms = 0u;
            s_cmd_sub_fail_count++;
            if(s_cmd_sub_fail_log_ms == 0u ||
               (now - s_cmd_sub_fail_log_ms) >= 2000u) {
                CMD_LOGF("[CLOUD][CMD] subscribe send fail x%u (UART2 busy?)\r\n",
                         (unsigned)s_cmd_sub_fail_count);
                cloud_aliyun_at_cmd_diag_log("send-fail");
                s_cmd_sub_fail_log_ms = now;
                s_cmd_sub_fail_count = 0u;
            }
            return;
        }
        if(pkt_id != s_cmd_sub_pkt_id) {
            s_cmd_sub_pkt_id = pkt_id;
        }
        if(s_cmd_sub_ok == 0u &&
           cloud_aliyun_at_mqtt_suback_matches(pkt_id) != 0u) {
            app_cloud_command_on_suback_for_pkt(pkt_id);
        }
        if(s_cmd_sub_ok != 0u) {
            CMD_LOGF("[CLOUD][CMD] subscribe sent pkt=%u (ack in send)\r\n", (unsigned)pkt_id);
        } else {
            CMD_LOGF("[CLOUD][CMD] subscribe sent pkt=%u\r\n", (unsigned)pkt_id);
            cmd_diag_waiting("just-sent");
        }
    }
}

uint8_t app_cloud_command_on_mqtt_publish(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    const char *json;

    if(s_cmd_sub_ok == 0u) {
        return 0u;
    }
    if(cmd_mqtt_publish_is_downlink(buf, len) == 0u) {
        return 0u;
    }
    cmd_log_payload_preview(buf, len);
    json = NULL;
    for(i = 0u; i < len; i++) {
        if(buf[i] == '{') {
            json = (const char *)(buf + i);
            break;
        }
    }
    if(json != NULL && app_cloud_bind_handle_json(json, (uint16_t)(len - (uint16_t)(json - (const char *)buf))) != 0u) {
        return 1u;
    }
    CMD_LOG("[CLOUD][CMD] downlink ignored\r\n");
    return (json != NULL) ? 1u : 0u;
}

#endif /* APP_CLOUD_COMMAND_ENABLE */
