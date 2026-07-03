#include "cloud_aliyun_at.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_wifi_modem.h"
#include "app_wifi_cfg.h"
#include "app_wifi_policy.h"
#include "app_wifi_scan.h"
#include "app_wifi_remember.h"
#include "app_link_guard.h"
#include "cloud_ota_service.h"
#include "app_unlock_flash_queue.h"
#include "app_screen_wifi_flow.h"
#include "app_cloud_session.h"
#include "app_pair_bind.h"
#include "app_wifi_diag.h"
#include "app_screen.h"
#if (APP_CLOUD_COMMAND_ENABLE != 0)
#include "app_cloud_command.h"
#endif
#if (APP_HOST_SILENCE_ALIYUN_TERMINAL == 1)
#undef printf
#define printf(...) ((void)0)
#endif
#if (APP_WIFI_CWJAP_TRACE != 0)
#include "./SYSTEM/usart/usart.h"
#define CWJAP_TRACE_MSG(s)  usart_debug_tx_str(s)
#else
#define CWJAP_TRACE_MSG(s)  ((void)0)
#endif
#if (APP_ALIYUN_AT_ENABLE == 1) || (APP_ALIYUN_SNTP_ENABLE == 1)
#include "app_wall_clock.h"
#include "app_ccm_ram.h"
#endif
#include "stm32f4xx_hal.h"
#include "./SYSTEM/sys/sys.h"
#include "app_state.h"
#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#endif

void cloud_aliyun_at_cwlap_scan_async_abort(void);
void cloud_aliyun_cwlap_scan_abort(void);
uint8_t cloud_aliyun_at_cwlap_scan_async_active(void);
uint8_t cloud_aliyun_at_cwlap_scan_async_pos(uint16_t *out_len);
int cloud_aliyun_at_cwlap_scan_async_poll(void);

static void uart2_init_once(void);
static int uart2_send_text(const char *txt);
static int uart2_send_bytes(const uint8_t *buf, uint16_t len);
static int uart2_wait_text(const char *needle, uint32_t timeout_ms);
static int uart2_read_byte_timeout(uint8_t *out, uint32_t timeout_ms);
static void cloud_uart2_rx_push_char(char ch);
static void cloud_uart2_quiet_for_cwlap(void);
static void cloud_uart2_copy_cwjap_ssid(char *out, uint16_t out_sz);
static void cloud_uart2_hw_drain_ms(uint32_t ms);
static void cloud_uart2_drain_until_idle(uint32_t max_ms, uint32_t quiet_ms);
static void aliyun_mqtt_close_link0(void);
static void aliyun_mqtt_schedule_reconnect(const char *reason);
static void aliyun_mqtt_uart_prep(void);
static void cloud_aliyun_at_mqtt_kick_after_wifi(void);
static void cloud_uart2_wait_quiet_after_scan_abort(uint32_t quiet_ms);
static void cloud_uart2_flush_rx(void);
static uint8_t cloud_uart2_wait_modem_idle(uint32_t timeout_ms);
static uint8_t uart2_rx_has_ok_reply(void);
static void cwjap_modem_purge(uint8_t hard_reset);
static uint8_t cloud_uart2_wait_esp_ready(uint32_t timeout_ms);
static uint8_t cwjap_scr11_radio_prep(void);
static uint8_t uart2_at_cmd_wait_ok(const char *cmd, uint32_t timeout_ms);
static void uart2_pump_rx_core(uint32_t duration_ms, uint8_t ignore_ui_busy);
static int aliyun_thing_model_code_from_buf(const char *buf, uint16_t len);

typedef enum {
    ALIYUN_STEP_IDLE = 0,
    ALIYUN_STEP_AT,
    ALIYUN_STEP_ATE0,
    ALIYUN_STEP_CWMODE,
    ALIYUN_STEP_CIPMODE,
    ALIYUN_STEP_CIPMUX,
    ALIYUN_STEP_CWJAPQ,
    ALIYUN_STEP_CWJAP_SET,
    ALIYUN_STEP_WIFI_IDLE,
    ALIYUN_STEP_CIFSR,
#if (APP_ALIYUN_SNTP_ENABLE == 1)
    ALIYUN_STEP_SNTP_CFG,
    ALIYUN_STEP_SNTP_WAIT,
    ALIYUN_STEP_SNTP_TIME,
#endif
    ALIYUN_STEP_CIPSTART,
    ALIYUN_STEP_CIPSEND,
    ALIYUN_STEP_SEND_CONNECT,
    ALIYUN_STEP_WAIT_CONNACK,
    ALIYUN_STEP_ONLINE
} aliyun_step_t;

static UART_HandleTypeDef s_uart2;
static uint8_t s_uart2_inited = 0u;
static uint32_t s_uart2_baud = APP_ALIYUN_UART_BAUD;
static uint8_t s_at_fail_cnt = 0u;
static uint32_t s_modem_key_recover_ms = 0u;
static uint8_t s_cipmode_fail_cnt = 0u;
static aliyun_step_t s_step = ALIYUN_STEP_IDLE;
static uint32_t s_step_ms = 0u;

static char s_host[96];
static char s_client_id[96];
static char s_user_name[96];
static char s_password[96];
static char s_prop_post_topic[128];
static char s_prop_reply_topic[128];
static uint8_t s_prop_reply_sub_sent = 0u;
static volatile uint8_t s_prop_post_reply_ok = 0u;
static volatile uint8_t s_prop_post_awaiting = 0u;
static char s_ntp_req_topic[96];
static char s_ntp_resp_topic[96];
static uint8_t s_mqtt_connect_pkt[384];
static char s_mqtt_will_topic[96];
static char s_mqtt_will_payload[64];
static uint16_t s_mqtt_connect_pkt_len = 0u;
static aliyun_step_t s_last_step_log = ALIYUN_STEP_IDLE;
static uint8_t s_rst_gpio_inited = 0u;
static char s_rx_win[256];
static uint16_t s_rx_win_pos = 0u;

#define UART2_RX_RING_SZ 4096u
static volatile uint16_t s_uart2_ring_head = 0u;
static volatile uint16_t s_uart2_ring_tail = 0u;
static APP_CCM_DATA uint8_t s_uart2_ring[UART2_RX_RING_SZ];
static uint8_t s_uart2_irq_on = 0u;

/* -------- UART2 互斥量：保护所有 AT 命令发送/等待，防止 GuiTask(CWLAP) 和
 *           CloudTask(CWJAP/MQTT) 并发抢占 UART2 -------- */
#if (APP_USE_FREERTOS == 1)
static SemaphoreHandle_t s_uart2_mutex = NULL;

static void uart2_mutex_init_once(void)
{
    if(s_uart2_mutex != NULL) {
        return;
    }
    if(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }
    s_uart2_mutex = xSemaphoreCreateRecursiveMutex();
    configASSERT(s_uart2_mutex != NULL);
}

/* 持有互斥量期间调用；timeout=0 表示不等待直接返回失败 */
static uint8_t uart2_mutex_take(uint32_t timeout_ms)
{
    if(s_uart2_mutex == NULL) return 1u;
    return (xSemaphoreTakeRecursive(s_uart2_mutex,
                                    (timeout_ms == 0u) ? 0u : pdMS_TO_TICKS(timeout_ms)) == pdTRUE) ? 1u : 0u;
}

static void uart2_mutex_give(void)
{
    if(s_uart2_mutex != NULL) {
        (void)xSemaphoreGiveRecursive(s_uart2_mutex);
    }
}
#else
static uint8_t uart2_mutex_take(uint32_t timeout_ms) { (void)timeout_ms; return 1u; }
static void    uart2_mutex_give(void) {}
#endif

static void uart2_ring_push(uint8_t b)
{
    uint16_t next = (uint16_t)((s_uart2_ring_head + 1u) % UART2_RX_RING_SZ);

    if(next != s_uart2_ring_tail) {
        s_uart2_ring[s_uart2_ring_head] = b;
        s_uart2_ring_head = next;
    }
}

static uint8_t uart2_ring_pop(uint8_t *b)
{
    if(b == NULL || s_uart2_ring_tail == s_uart2_ring_head) {
        return 0u;
    }
    *b = s_uart2_ring[s_uart2_ring_tail];
    s_uart2_ring_tail = (uint16_t)((s_uart2_ring_tail + 1u) % UART2_RX_RING_SZ);
    return 1u;
}

static void uart2_ring_clear(void)
{
    s_uart2_ring_head = 0u;
    s_uart2_ring_tail = 0u;
}

void USART2_IRQHandler(void)
{
    uint32_t isr;

    if(s_uart2_inited == 0u) {
        return;
    }
    isr = READ_REG(s_uart2.Instance->SR);
    if((isr & USART_SR_RXNE) != 0u) {
        uart2_ring_push((uint8_t)(READ_REG(s_uart2.Instance->DR) & 0xFFu));
    }
    if((isr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) != 0u) {
        (void)READ_REG(s_uart2.Instance->DR);
    }
}

static void uart2_irq_enable(void)
{
    if(s_uart2_inited == 0u || s_uart2_irq_on != 0u) {
        return;
    }
    uart2_ring_clear();
    __HAL_UART_DISABLE_IT(&s_uart2, UART_IT_RXNE);
    while(__HAL_UART_GET_FLAG(&s_uart2, UART_FLAG_RXNE) != RESET) {
        uart2_ring_push((uint8_t)(READ_REG(s_uart2.Instance->DR) & 0xFFu));
    }
    __HAL_UART_ENABLE_IT(&s_uart2, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    s_uart2_irq_on = 1u;
}
static uint32_t s_last_ping_ms = 0u;
static uint32_t s_mqtt_online_ms = 0u;
static uint8_t s_unlock_flush_done = 0u;
static uint32_t s_unlock_flush_last_ms = 0u;
#define ALIYUN_UNLOCK_FLUSH_RETRY_MS 3000u
#if (APP_ALIYUN_SNTP_ENABLE == 1)
static uint32_t s_sntp_wait_until_ms = 0u;
static uint8_t s_sntp_try_cnt = 0u;
static uint8_t s_sntp_empty_cnt = 0u;
static uint32_t s_sntp_next_query_ms = 0u;
static uint32_t s_sntp_deadline_ms = 0u;
static uint8_t s_sntp_synced = 0u;
static uint8_t s_sntp_give_up = 0u;
static uint8_t s_sntp_time_updated_seen = 0u;
static uint32_t s_sntp_time_updated_ms = 0u;
static uint32_t s_sntp_query_start_ms = 0u;
static uint32_t s_sntp_retry_online_ms = 0u;
static uint8_t s_http_date_tried = 0u;
#endif
static uint32_t s_cipstart_next_ms = 0u;
#if (APP_ALIYUN_SNTP_ENABLE == 1)
static uint8_t s_sntp_cfg_sent = 0u;
static uint32_t s_sntp_online_query_ms = 0u;
#endif
static uint8_t s_ntp_synced = 0u;
static uint8_t s_ntp_sub_ok = 0u;
static uint32_t s_ntp_sub_ms = 0u;
static uint32_t s_ntp_req_dev_ms = 0u;
static uint32_t s_ntp_next_request_ms = 0u;
static uint32_t s_ntp_await_until_ms = 0u;
static uint8_t s_ntp_req_pending = 0u;
static uint32_t s_ntp_req_sent_ms = 0u;
static uint8_t s_ntp_sub_sess = 0u;
static uint16_t s_ntp_sub_pkt_id = 0u;
static uint8_t s_ntp_give_up = 0u;
static uint32_t s_ntp_next_try_ms = 0u;
static uint8_t s_ntp_sync_running = 0u;
static uint8_t s_ntp_connect_tried = 0u;
static uint8_t s_ntp_mqtt_fail_cnt = 0u;
#if (APP_TIME_TRACE != 0)
static uint8_t s_ntp_suback_logged = 0u;
static uint32_t s_ntp_pend_diag_ms = 0u;
#endif
/* ~2025-06-01 UTC+8 秒级起点 + HAL_GetTick 组成 deviceSendTime 字符串 */
#define NTP_DEVTIME_SEC_BASE  1748736000UL
static uint8_t s_ping_fail_streak = 0u;
static uint8_t s_mqtt_tx_fail_streak = 0u;
static uint16_t s_mqtt_pkt_id = 1u;
static uint32_t s_last_mqtt_activity_ms = 0u;
static uint8_t s_uart_ui_busy = 0u;
static uint8_t s_modem_at_ok = 0u;
static uint8_t s_cipmux1_ok = 0u;
static uint8_t s_mqtt_fast_join = 0u;
static uint8_t s_cipsend_retry_cnt = 0u;
static uint8_t s_scr11_mqtt_paused = 0u;
static aliyun_step_t s_scr11_resume_step = ALIYUN_STEP_IDLE;
static uint8_t s_wifi_sta_ip_ok = 0u;
static uint8_t s_wifi_ip_verify_pending = 0u;
static uint8_t s_wifi_ever_up = 0u;
static volatile uint8_t s_user_wifi_join_st = 0u; /* 0 idle 1 pending 2 ok 3 fail */
static volatile uint8_t s_wifi_join_fail_latch = 0u;
static uint8_t s_cwjap_busy_retry = 0u;
static uint8_t s_cwjap_uart_settle_done = 0u;
static uint8_t s_cwjap_resend_only = 0u;
static uint32_t s_cwjap_quiet_until_ms = 0u;
static uint32_t s_user_join_fail_log_ms = 0u;
static char s_connected_sta_ssid[33];
static uint8_t s_sta_radio_prep_done = 0u;
static uint8_t s_cwjap_esp_idle_ok = 0u;
static uint8_t s_cwjap_light_join = 0u;
static uint8_t s_cwjap_light_need_prep = 0u;
static uint8_t s_cwjap_try_channel = 0u;
#if (APP_WIFI_MODEM_WF24 != 0)
static uint8_t s_cwjap_wf24_need_cwqap = 0u;
#endif
static uint8_t s_cloud_scan_kick_done = 0u;
static uint32_t s_wifi_boot_retry_ms = 0u;
static uint8_t s_wifi_boot_fail_cnt = 0u;
static uint32_t s_sta_probe_ms = 0u;
static uint8_t s_cifsr_retry_cnt = 0u;
static uint32_t s_cifsr_last_try_ms = 0u;
static uint32_t s_wifi_got_ip_ms = 0u;
#define WIFI_DISCONNECT_GRACE_MS  10000u
#define CIFSR_RETRY_MAX       20u
#define CIFSR_RETRY_DELAY_MS  400u
#define CWJAP_GOT_IP_WAIT_MS    5000u
#define CWJAP_CONNECT_WAIT_MS   18000u
#define CWJAP_BOOT_RETRY_MS     45000u
#define CWJAP_STA_PROBE_MS      3000u
#define CWJAP_JOIN_DEFER_FAIL_MS 4000u
/* rev-66 可连逻辑不变；仅缩短 connect 前已 teardown_idle 的重复等待 */
#define CWJAP_LIGHT_QUIET_MS       600u   /* was 2000 */
#define CWJAP_LIGHT_HOLD_MS        350u   /* was 4000 quiet_until */
#define CWJAP_LIGHT_BEFORE_SEND_MS 500u  /* was 1500 hw_drain */
#define CWJAP_FAST_SETTLE_QUIET_MS 700u  /* was 1200 */
#define CWJAP_FAST_SETTLE_AT_MS    1800u /* was 2500 */
#define CWJAP_ESP_IDLE_HOLD_MS     350u  /* was 600 */
#define CWJAP_PREP_HOLD_MS         600u  /* was 1200 */
#define CWJAP_BUSY_RESEND_QUIET_MS 2500u /* was 3500 */
#define CWJAP_CWQAP_HOLD_MS        800u  /* was 1500 */
#define CWJAP_TEARDOWN_QUIET_MS    1800u /* teardown_idle 首段 */
#define CWJAP_TEARDOWN_IDLE_MS     2000u /* teardown_idle 探活段 */

#if (APP_USE_FREERTOS == 1)
static void uart2_coop_yield(void)
{
    if(g_wifi_exclusive != 0u || s_uart_ui_busy != 0u) {
        taskYIELD();
    }
}
#else
static void uart2_coop_yield(void)
{
}
#endif

static void uart2_drain_rx_ms(uint32_t ms);
static void aliyun_mqtt_touch_activity(void);
static int uart2_recv_ipd_payload(uint8_t *payload, uint16_t payload_sz, uint16_t *out_len, uint32_t timeout_ms);
static uint8_t aliyun_ntp_try_parse_any(void);
static uint8_t aliyun_ntp_apply_from_mqtt_chunk(const uint8_t *chunk, uint16_t len);
static uint8_t aliyun_ntp_handle_ipd(const uint8_t *buf, uint16_t len);

static uint8_t cwlap_scan_should_stop(void)
{
    return (g_wifi_scan_abort != 0u || g_wifi_exclusive == 0u) ? 1u : 0u;
}

#if (APP_WIFI_MODEM_WF24 != 0)
#define ALIYUN_ESP_RST_PORT GPIOE
#define ALIYUN_ESP_RST_PIN  GPIO_PIN_12
#else
#define ALIYUN_ESP_RST_PORT GPIOA
#define ALIYUN_ESP_RST_PIN  GPIO_PIN_8
#endif

#if (APP_WIFI_MODEM_WF24 != 0)
typedef enum {
    WF24_MQTT_ST_IDLE = 0,
    WF24_MQTT_ST_CLEAN,
    WF24_MQTT_ST_CFG,
    WF24_MQTT_ST_CLIENT,
    WF24_MQTT_ST_USER,
    WF24_MQTT_ST_PASS,
    WF24_MQTT_ST_CONN,
    WF24_MQTT_ST_WAIT
} wf24_mqtt_st_t;

static wf24_mqtt_st_t s_wf24_mqtt_st = WF24_MQTT_ST_IDLE;
static uint8_t s_wf24_poll_busy = 0u;
static uint32_t s_wf24_mqtt_wait_ms = 0u;
static uint8_t s_wf24_mqtt_conn_ok = 0u;
static uint32_t s_wf24_cwstate_query_ms = 0u;
static const char *wf24_find_last_cwstate(const char *buf);
static const char *wf24_find_last_cwjap_ok(const char *buf);
static uint8_t wf24_cwjap_fail_in_rx(void);
static uint8_t wf24_cwjap_dhcp_pending(void);
static uint8_t wf24_parse_cwstate_line(const char *p, uint8_t *out_st, char *out_ssid, uint16_t ssid_sz);
static uint8_t wf24_query_cwstate(uint8_t *out_st, char *out_ssid, uint16_t ssid_sz);
static const char *wf24_mqtt_st_label(wf24_mqtt_st_t st);
static uint8_t modem_wifi_got_ip_in_rx(void);
static void modem_at_echo_off(void);
static uint8_t modem_cwlap_frame_valid(const char *dst, uint16_t pos);
static uint8_t wf24_mqtt_publish_raw(const char *topic, const uint8_t *data, uint16_t len);
static void wf24_poll_mqtt_urc(void);
static void wf24_mqtt_reset_state(void);
static uint8_t wf24_mqtt_connect_step(char *cmd, size_t cmd_sz);
static uint8_t wf24_mqtt_subscribe_topic(const char *topic, uint16_t *out_pkt_id);
#else
static uint8_t modem_wifi_got_ip_in_rx(void);
#endif
#define ALIYUN_KEEPALIVE_PING_MS 35000u
#define ALIYUN_MQTT_PING_WAIT_MS   1200u
#define ALIYUN_POLL_INTERVAL_MS 30u
#define ALIYUN_WAIT_SHORT_MS 120u
#define ALIYUN_WAIT_MID_MS 300u
#define ALIYUN_WAIT_LONG_MS 800u
/* 用户选 WiFi 连上后快路径：短超时 + 快重试（对齐文件夹1 思路，保留选网 UI） */
#define ALIYUN_CIPSTART_WAIT_MS      2500u
#define ALIYUN_CIPSTART_RETRY_MS     800u
#define ALIYUN_CONNACK_WAIT_MS       5000u
#define ALIYUN_CIPSEND_PROMPT_MS     2000u
#define ALIYUN_POST_TCP_MQTT_MS      90u
#define ALIYUN_POST_TCP_MQTT_FAST_MS 25u
#define ALIYUN_MQTT_DRAIN_MAX_MS     110u
#define ALIYUN_MQTT_DRAIN_QUIET_MS   14u
#define ALIYUN_CIPSEND_RETRY_MAX     3u
#define ALIYUN_CIPSEND_RETRY_MS      100u
#define ALIYUN_MQTT_RECONNECT_MS     180u
#define ALIYUN_SNTP_CMD_OK_MS 800u
#define ALIYUN_SNTP_QUERY_MS 2500u
#define ALIYUN_SNTP_POSTCFG_DELAY_MS 2000u
#define ALIYUN_SNTP_POSTCFG_MIN_MS 2000u
#define ALIYUN_SNTP_QUERY_GAP_MS 2000u
#define ALIYUN_SNTP_MAX_TRY 5u
#define ALIYUN_SNTP_PHASE_MS 25000u
#define ALIYUN_SNTP_HTTP_AFTER_TRY 2u
#define ALIYUN_SNTP_ONLINE_RETRY_MS 30000u
#define ALIYUN_HTTP_DATE_HOST "www.baidu.com"
#define ALIYUN_MQTT_NTP_RSP_WAIT_MS   8000u
#define ALIYUN_MQTT_NTP_SUB_SETTLE_MS 300u
#define ALIYUN_MQTT_NTP_RETRY_ONLINE_MS 4000u
#define ALIYUN_MQTT_NTP_FIRST_DELAY_MS  0u
#define ALIYUN_MQTT_NTP_REQ_PENDING_MS  12000u
#if (APP_WIFI_MODEM_WF24 != 0)
/* WF24 DHCP/URC 收尾较慢；略等再 MQTTCONN 可减少 ERROR=102 首连失败 */
#define ALIYUN_MQTT_POST_JOIN_CIPSTART_MS 1200u
#define WF24_MQTT_WAIT_MS               15000u
#define WF24_MQTT_WAIT_FAST_MS          4500u
#define WF24_SNTP_POLL_MS               30000u
#define WF24_SNTP_QUERY_GAP_MS          2000u
#else
#define ALIYUN_MQTT_POST_JOIN_CIPSTART_MS 200u
#endif
#define ALIYUN_MQTT_NTP_POLL_SLICE_MS 80u

#if (APP_ALIYUN_SNTP_ENABLE == 1)
static int month_abbr_to_i(const char *m)
{
    static const char mons[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    uint8_t i;
    if(m == NULL) return 1;
    for(i = 0u; i < 12u; i++) {
        if(strncmp(m, mons[i], 3u) == 0) return (int)(i + 1u);
    }
    return 1;
}

static uint8_t is_leap_year(int y)
{
    if((y % 400) == 0) return 1u;
    if((y % 100) == 0) return 0u;
    return ((y % 4) == 0) ? 1u : 0u;
}

static int days_in_month(int y, int m)
{
    static const uint8_t dm[12] = {31u,28u,31u,30u,31u,30u,31u,31u,30u,31u,30u,31u};
    if(m < 1 || m > 12) return 30;
    if(m == 2 && is_leap_year(y)) return 29;
    return (int)dm[m - 1];
}

static void add_hours_to_datetime(int *y, int *mo, int *d, int *h, int add_h)
{
    int hh = *h + add_h;
    while(hh >= 24) {
        hh -= 24;
        (*d)++;
        if(*d > days_in_month(*y, *mo)) {
            *d = 1;
            (*mo)++;
            if(*mo > 12) {
                *mo = 1;
                (*y)++;
            }
        }
    }
    while(hh < 0) {
        hh += 24;
        (*d)--;
        if(*d < 1) {
            (*mo)--;
            if(*mo < 1) {
                *mo = 12;
                (*y)--;
            }
            *d = days_in_month(*y, *mo);
        }
    }
    *h = hh;
}

static uint8_t aliyun_ensure_sta_ip(void)
{
#if (APP_WIFI_MODEM_WF24 != 0)
    uint8_t st = 0u;
    char ssid_live[40];
    uint32_t now;

    if(cloud_aliyun_at_mqtt_connecting() != 0u) {
        return s_wifi_sta_ip_ok;
    }
    now = HAL_GetTick();
    if(s_wifi_sta_ip_ok != 0u && s_wf24_cwstate_query_ms != 0u &&
       (now - s_wf24_cwstate_query_ms) < 15000u) {
        return 1u;
    }
    if(wf24_query_cwstate(&st, ssid_live, (uint16_t)sizeof(ssid_live)) == 0u) {
        s_wifi_sta_ip_ok = 0u;
        return 0u;
    }
    s_wf24_cwstate_query_ms = now;
    if(st == 2u) {
        s_wifi_sta_ip_ok = 1u;
        s_wifi_ever_up = 1u;
        if(ssid_live[0] != '\0') {
            strncpy(s_connected_sta_ssid, ssid_live, sizeof(s_connected_sta_ssid) - 1u);
            s_connected_sta_ssid[sizeof(s_connected_sta_ssid) - 1u] = '\0';
        }
        return 1u;
    }
    printf("[ALIYUN] ensure_sta_ip: CWSTATE=%u ssid='%s' (need 2)\r\n",
           (unsigned)st, ssid_live);
    s_wifi_sta_ip_ok = 0u;
    return 0u;
#else
    if(s_wifi_sta_ip_ok != 0u) {
        return 1u;
    }
    if(uart2_send_text("AT+CIFSR\r\n") &&
       (uart2_wait_text("STAIP", ALIYUN_WAIT_SHORT_MS) ||
        uart2_wait_text("+CIFSR:STAIP", ALIYUN_WAIT_MID_MS))) {
        s_wifi_sta_ip_ok = 1u;
        s_wifi_ever_up = 1u;
        return 1u;
    }
    s_wifi_sta_ip_ok = 0u;
    return 0u;
#endif
}

static uint8_t aliyun_rx_has_no_ip(void)
{
    return (strstr(s_rx_win, "no ip") != NULL) ? 1u : 0u;
}

static uint8_t aliyun_http_date_finish(uint8_t ok, uint8_t used_mux1)
{
    if(used_mux1 != 0u) {
        uart2_send_text("AT+CIPCLOSE=1\r\n");
    } else {
        uart2_send_text("AT+CIPCLOSE\r\n");
    }
    (void)cloud_uart2_pump_rx(40u);
    return ok;
}

#if (APP_ALIYUN_HTTP_DATE_ENABLE != 0)
static uint8_t aliyun_try_fetch_http_date_once(void)
{
    char cmd[96];
    const char *req = "HEAD / HTTP/1.1\r\nHost: " ALIYUN_HTTP_DATE_HOST "\r\nConnection: close\r\n\r\n";
    char *p;
    char wk[4];
    char mon[4];
    int d = 1, y = 0, hh = 0, mm = 0, ss = 0, mo = 1;
    uint8_t used_mux1 = 0u;
    uint16_t req_len = (uint16_t)strlen(req);

    memset(s_rx_win, 0, sizeof(s_rx_win));
    s_rx_win_pos = 0u;
    if(aliyun_ensure_sta_ip() == 0u) {
        return 0u;
    }
    (void)uart2_at_cmd_wait_ok("AT+CIPCLOSE\r\n", 500u);
    (void)uart2_at_cmd_wait_ok("AT+CIPCLOSE=0\r\n", 400u);
    (void)uart2_at_cmd_wait_ok("AT+CIPCLOSE=1\r\n", 400u);
    (void)uart2_at_cmd_wait_ok("AT+CIPMUX=0\r\n", 600u);
    cloud_uart2_rx_clear();

    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",80\r\n", ALIYUN_HTTP_DATE_HOST);
    if(!uart2_send_text(cmd) ||
       !(uart2_wait_text("CONNECT", 2000u) || uart2_wait_text("ALREADY CONNECTED", 400u))) {
        memset(s_rx_win, 0, sizeof(s_rx_win));
        s_rx_win_pos = 0u;
        if(!uart2_send_text("AT+CIPMUX=1\r\n") || !uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS)) {
            return 0u;
        }
        used_mux1 = 1u;
        snprintf(cmd, sizeof(cmd), "AT+CIPSTART=1,\"TCP\",\"%s\",80\r\n", ALIYUN_HTTP_DATE_HOST);
        if(!uart2_send_text(cmd) ||
           !(uart2_wait_text("CONNECT", 2000u) || uart2_wait_text("ALREADY CONNECTED", 400u))) {
            return 0u;
        }
    }

    if(used_mux1 != 0u) {
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=1,%u\r\n", (unsigned)req_len);
    } else {
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", (unsigned)req_len);
    }
    if(!uart2_send_text(cmd) || !uart2_wait_text(">", 600u)) {
        return aliyun_http_date_finish(0u, used_mux1);
    }
    if(!uart2_send_text(req) || !uart2_wait_text("SEND OK", 1200u)) {
        return aliyun_http_date_finish(0u, used_mux1);
    }

    if(!uart2_wait_text("Date:", 2500u)) {
        return aliyun_http_date_finish(0u, used_mux1);
    }

    p = strstr(s_rx_win, "Date:");
    if(p != NULL && sscanf(p, "Date: %3s, %d %3s %d %d:%d:%d", wk, &d, mon, &y, &hh, &mm, &ss) == 7) {
        mo = month_abbr_to_i(mon);
        add_hours_to_datetime(&y, &mo, &d, &hh, APP_ALIYUN_SNTP_TZ_HOURS);
        if(y >= 2020 && mo >= 1 && mo <= 12) {
            app_home_wall_clock_set(y, mo, d, hh, mm, ss);
            TIME_TRACE_MSG("[TIME] sync ok (HTTP date)\r\n");
            LOG_ESSENTIAL("[TIME] sync ok\r\n");
            cloud_aliyun_at_time_sync_done();
            return aliyun_http_date_finish(1u, used_mux1);
        }
    }
    return aliyun_http_date_finish(0u, used_mux1);
}
#endif

/* +CIPSNTPTIME: 后是否仍无有效时间（勿用 strncmp(...,13) 误判整行） */
static uint8_t sntp_body_is_empty(const char *body)
{
    if(body == NULL) {
        return 1u;
    }
    while(*body == ' ' || *body == '\r' || *body == '\n' || *body == '"') {
        body++;
    }
    return (*body == '\0') ? 1u : 0u;
}

static const char *sntp_body_ptr(const char *p)
{
    const char *tag;
    const char *body;
    uint16_t skip;

    if(p == NULL) {
        return NULL;
    }
    tag = strstr(p, "+CIPSNTPTIME:");
    if(tag != NULL) {
        skip = 13u;
    } else {
        tag = strstr(p, "+CIPSNTP:");
        if(tag == NULL) {
            return NULL;
        }
        skip = 9u;
    }
    body = tag + skip;
    while(*body == ' ' || *body == '"') {
        body++;
    }
    return body;
}

static const char *sntp_find_last_line(const char *buf)
{
    const char *last = NULL;
    const char *p;

    if(buf == NULL) {
        return NULL;
    }
    /* 勿匹配 "+CIPSNTP:" 子串（会把 +CIPSNTPTIME 的 body 弄成 "TIME..."） */
    p = buf;
    while((p = strstr(p, "+CIPSNTPTIME:")) != NULL) {
        last = p;
        p += 14u;
    }
    return last;
}

static uint8_t sntp_body_is_tz_only(const char *body)
{
    unsigned v = 0u;
    const char *e;

    if(body == NULL) {
        return 0u;
    }
    if(sscanf(body, "%u", &v) != 1 || v >= 1000000u) {
        return 0u;
    }
    e = body;
    while(*e >= '0' && *e <= '9') {
        e++;
    }
    while(*e == ' ' || *e == '\r' || *e == '\n' || *e == '"') {
        e++;
    }
    return (*e == '\0') ? 1u : 0u;
}

/* 跳过 "<idx>,\"...\"," 前缀，指向真实日期时间字段 */
static const char *sntp_body_time_tail(const char *body)
{
    const char *p;
    const char *e;

    if(body == NULL) {
        return NULL;
    }
    p = body;
    while(*p >= '0' && *p <= '9') {
        p++;
    }
    if(p == body) {
        return body;
    }
    if(*p == ',') {
        p++;
    }
    while(*p == ' ' || *p == '"') {
        if(*p == '"') {
            e = strchr(p + 1, '"');
            if(e == NULL) {
                return p;
            }
            p = e + 1;
        } else {
            p++;
        }
        if(*p == ',') {
            p++;
        }
    }
    while(*p == ' ') {
        p++;
    }
    return p;
}

/* 模组仅回报 STA MAC（SNTP 未完成），勿当 Ebd */
static uint8_t sntp_body_is_mac_only(const char *body)
{
    const char *tail;

    tail = sntp_body_time_tail(body);
    if(tail == NULL || *tail == '\0') {
        return 0u;
    }
    if(strchr(tail, '-') != NULL) {
        return 0u;
    }
    if(strstr(tail, "Jan") != NULL || strstr(tail, "Feb") != NULL ||
       strstr(tail, "Mar") != NULL || strstr(tail, "Apr") != NULL ||
       strstr(tail, "May") != NULL || strstr(tail, "Jun") != NULL ||
       strstr(tail, "Jul") != NULL || strstr(tail, "Aug") != NULL ||
       strstr(tail, "Sep") != NULL || strstr(tail, "Oct") != NULL ||
       strstr(tail, "Nov") != NULL || strstr(tail, "Dec") != NULL) {
        return 0u;
    }
    return (strchr(tail, ':') != NULL) ? 1u : 0u;
}

/* ESP 尚未给出完整时间（仅 "1"/时区/MAC 等），应继续等而非当 Ebd */
static uint8_t sntp_body_not_ready(const char *body)
{
    uint8_t blen = 0u;
    const char *tail;

    if(body == NULL || sntp_body_is_empty(body) != 0u) {
        return 1u;
    }
    if(sntp_body_is_tz_only(body) != 0u) {
        return 1u;
    }
    if(sntp_body_is_mac_only(body) != 0u) {
        return 1u;
    }
    tail = sntp_body_time_tail(body);
    if(tail != NULL && tail != body) {
        body = tail;
    }
    while(body[blen] != '\0' && body[blen] != '\r' && body[blen] != '\n' && blen < 48u) {
        blen++;
    }
    if(blen < 8u && strchr(body, '-') == NULL && strchr(body, ':') == NULL) {
        return 1u;
    }
    return 0u;
}

static uint8_t epoch_to_local_datetime(uint64_t epoch_sec, int tz_hour, int *y, int *mo, int *d, int *h, int *mi, int *s);
static uint8_t sntp_apply_parsed_time(int year, int month, int day, int hh, int mm, int ss);

static uint8_t sntp_parse_epoch_digits(const char *s)
{
    const char *p = s;
    uint64_t v = 0u;
    uint8_t nd = 0u;
    uint64_t sec;
    int year, month, day, hh, mm, ss;

    if(s == NULL) {
        return 0u;
    }
    while(*p != '\0') {
        if(*p >= '0' && *p <= '9') {
            if(nd == 0u) {
                v = 0u;
            }
            v = v * 10u + (uint64_t)(*p - '0');
            nd++;
            if(nd > 13u) {
                nd = 0u;
            }
        } else {
            if(nd >= 10u) {
                sec = v;
                if(v > 2000000000000ull) {
                    sec = v / 1000u;
                }
                if(sec > 1600000000ull &&
                   epoch_to_local_datetime(sec, APP_ALIYUN_SNTP_TZ_HOURS, &year, &month, &day, &hh, &mm, &ss) != 0u) {
                    return sntp_apply_parsed_time(year, month, day, hh, mm, ss);
                }
            }
            nd = 0u;
        }
        p++;
    }
    if(nd >= 10u) {
        sec = v;
        if(v > 2000000000000ull) {
            sec = v / 1000u;
        }
        if(sec > 1600000000ull &&
           epoch_to_local_datetime(sec, APP_ALIYUN_SNTP_TZ_HOURS, &year, &month, &day, &hh, &mm, &ss) != 0u) {
            return sntp_apply_parsed_time(year, month, day, hh, mm, ss);
        }
    }
    return 0u;
}

static uint8_t sntp_apply_parsed_time(int year, int month, int day, int hh, int mm, int ss)
{
    int lag_sec = 0;
    uint32_t now_ms;

    if(year >= 2020 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
        now_ms = HAL_GetTick();
        if(s_sntp_time_updated_ms != 0u && now_ms >= s_sntp_time_updated_ms) {
            lag_sec = (int)((now_ms - s_sntp_time_updated_ms) / 1000u);
        } else if(s_sntp_query_start_ms != 0u && now_ms >= s_sntp_query_start_ms) {
            lag_sec = (int)((now_ms - s_sntp_query_start_ms) / 1000u);
        }
        if(lag_sec < 0) {
            lag_sec = 0;
        }
        if(lag_sec > 120) {
            lag_sec = 120;
        }
        app_wall_clock_set_local(year, month, day, hh, mm, ss, lag_sec);
        app_home_wall_clock_refresh_ui();
        TIME_TRACE_MSG("[TIME] sync ok (ESP SNTP)\r\n");
        LOG_ESSENTIAL("[TIME] sync ok\r\n");
        cloud_aliyun_at_time_sync_done();
        return 1u;
    }
    return 0u;
}

static void time_trace_ebd_maybe(void)
{
#if (APP_TIME_TRACE != 0)
    static uint32_t s_ebd_ms;
    uint32_t now = HAL_GetTick();
    if(s_ebd_ms != 0u && (now - s_ebd_ms) < 5000u) {
        return;
    }
    s_ebd_ms = now;
    TIME_TRACE_MSG("[TIME]Ebd\r\n");
#else
    (void)0;
#endif
}

static uint8_t sntp_try_parse_line(const char *p)
{
    const char *body;
    char mon[8];
    int day = 1;
    int month = 1;
    int hh = 0;
    int mm = 0;
    int ss = 0;
    int year = 0;

    if(p == NULL) {
        return 0u;
    }
    body = sntp_body_ptr(p);
    if(body == NULL || sntp_body_is_empty(body) != 0u) {
        return 0u;
    }
    if(strstr(body, "1970") != NULL || strncmp(body, "NULL", 4) == 0) {
        return 0u;
    }
    if(sntp_body_not_ready(body) != 0u) {
        return 0u;
    }

    {
        const char *tail = sntp_body_time_tail(body);
        if(tail == NULL) {
            tail = body;
        }

        /* 先解析 ESP 已按 CIPSNTPCFG 时区换算过的本地时间字符串，再尝试 Unix epoch */
        if(sscanf(p, "+CIPSNTPTIME:%*u,\"%*[^\"]\",%*3s %3s %d %d:%d:%d %d",
                  mon, &day, &hh, &mm, &ss, &year) == 6 ||
           sscanf(p, "+CIPSNTPTIME:%*u,\"%*[^\"]\",%d-%d-%d,%d:%d:%d",
                  &year, &month, &day, &hh, &mm, &ss) == 6) {
            if(strstr(p, "+CIPSNTPTIME:") != NULL && year >= 2020) {
                if(month < 1 || month > 12) {
                    month = month_abbr_to_i(mon);
                }
                return sntp_apply_parsed_time(year, month, day, hh, mm, ss);
            }
        }
        /* "1,Thu May..." / "1,2026-05-28,..." 须在 %*3s 星期格式之前 */
        if(sscanf(tail, "%*d,%*3s %3s %d %d:%d:%d %d", mon, &day, &hh, &mm, &ss, &year) == 6) {
            month = month_abbr_to_i(mon);
            return sntp_apply_parsed_time(year, month, day, hh, mm, ss);
        }
        if(sscanf(tail, "%*d,%d-%d-%d,%d:%d:%d", &year, &month, &day, &hh, &mm, &ss) == 6 ||
           sscanf(tail, "%d-%d-%d,%d:%d:%d", &year, &month, &day, &hh, &mm, &ss) == 6) {
            return sntp_apply_parsed_time(year, month, day, hh, mm, ss);
        }
        if(sscanf(tail, "%*3s %3s %d %d:%d:%d %d", mon, &day, &hh, &mm, &ss, &year) == 6) {
            month = month_abbr_to_i(mon);
            return sntp_apply_parsed_time(year, month, day, hh, mm, ss);
        }
        if(sscanf(tail, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hh, &mm, &ss) == 6) {
            return sntp_apply_parsed_time(year, month, day, hh, mm, ss);
        }
        if(sntp_parse_epoch_digits(tail) != 0u) {
            return 1u;
        }
    }

#if (APP_TIME_TRACE != 0)
    {
        static uint8_t s_ebd_body_once;
        if(s_ebd_body_once == 0u) {
            const char *tail = sntp_body_time_tail(body);
            char tb[24];
            uint8_t i;
            uint8_t n = 0u;
            s_ebd_body_once = 1u;
            TIME_TRACE_MSG("[TIME]b:");
            if(tail == NULL) {
                tail = body;
            }
            for(i = 0u; tail[i] != '\0' && n < 16u; i++) {
                char c = tail[i];
                if(c == '\r' || c == '\n') {
                    break;
                }
                tb[n++] = (c >= 0x20 && c < 0x7f) ? c : '.';
            }
            tb[n++] = '\r';
            tb[n++] = '\n';
            tb[n] = '\0';
            time_trace_tx(tb);
        }
    }
#endif
    time_trace_ebd_maybe();
    return 0u;
}

#if (APP_ALIYUN_SNTP_ENABLE == 1)
static uint8_t cloud_aliyun_sntp_step_active(void)
{
    return (s_step == ALIYUN_STEP_SNTP_CFG ||
            s_step == ALIYUN_STEP_SNTP_WAIT ||
            s_step == ALIYUN_STEP_SNTP_TIME) ? 1u : 0u;
}

/* SNTP 阶段收 ESP 上报（+TIME_UPDATED）；勿重复 push（read_byte 已写入 s_rx_win） */
static void sntp_uart_pump(uint32_t duration_ms)
{
    uint8_t ch = 0u;
    uint32_t end;

    if(!s_uart2_inited || duration_ms == 0u) {
        return;
    }
    end = HAL_GetTick() + duration_ms;
    while((HAL_GetTick() < end) != 0u) {
        (void)uart2_read_byte_timeout(&ch, 8u);
        uart2_coop_yield();
    }
}
#endif

/* r110：短查询，清 RX 后读 +CIPSNTPTIME */
static uint8_t aliyun_try_fetch_sntp_once(void)
{
    char *p;

    if(s_prop_post_awaiting != 0u) {
        return 0u;
    }
    cloud_uart2_rx_clear();
    s_sntp_query_start_ms = HAL_GetTick();
    if(!uart2_send_text("AT+CIPSNTPTIME?\r\n")) {
        TIME_TRACE_MSG("[TIME]Esf\r\n");
        return 0u;
    }
    if(!uart2_wait_text("+CIPSNTPTIME:", ALIYUN_SNTP_QUERY_MS) &&
       !uart2_wait_text("+CIPSNTP:", 400u)) {
        if(strstr(s_rx_win, "ERROR") != NULL) {
            TIME_TRACE_MSG("[TIME]Eer\r\n");
        } else {
            TIME_TRACE_MSG("[TIME]Enl\r\n");
        }
        return 0u;
    }
    p = (char *)sntp_find_last_line(s_rx_win);
    if(p == NULL) {
        TIME_TRACE_MSG("[TIME]Enl\r\n");
        return 0u;
    }
    if(sntp_try_parse_line(p) != 0u) {
        s_sntp_empty_cnt = 0u;
        return 1u;
    }
    s_sntp_empty_cnt++;
    time_trace_ebd_maybe();
    return 0u;
}

#if (APP_WIFI_MODEM_WF24 != 0)
/* WF24：CWJAP 后仅下发 CIPSNTPCFG（模组后台 SNTP，最长约 30s）；云端在线后再查 CIPSNTPTIME */
static uint8_t s_wf24_sntp_active;
static uint32_t s_wf24_sntp_t0;
static uint32_t s_wf24_sntp_last_ms;
static uint32_t s_wf24_sntp_retry_ms;

static void wf24_sntp_cfg_once_before_mqtt(void)
{
    char cmd[96];

    if(s_sntp_cfg_sent != 0u || s_sntp_synced != 0u || s_ntp_synced != 0u ||
       app_wall_clock_valid() != 0u) {
        return;
    }
    app_wifi_scan_abort_for_mqtt();
    cloud_uart2_rx_clear();
    cloud_uart2_hw_drain_ms(80u);
    snprintf(cmd, sizeof(cmd),
             "AT+CIPSNTPCFG=1,%d,\"ntp1.aliyun.com\",\"cn.ntp.org.cn\"\r\n",
             APP_ALIYUN_SNTP_TZ_HOURS);
    cloud_uart2_rx_clear();
    if(uart2_at_cmd_wait_ok(cmd, 3000u)) {
        s_sntp_cfg_sent = 1u;
        LOG_ESSENTIAL("[TIME] SNTP cfg ok\r\n");
    } else {
        LOG_ESSENTIAL("[TIME] SNTP cfg fail\r\n");
    }
    cloud_uart2_hw_drain_ms(100u);
}

static void wf24_modem_sntp_kick(void)
{
    if(s_sntp_synced != 0u || s_ntp_synced != 0u || app_wall_clock_valid() != 0u) {
        return;
    }
    if(s_wf24_sntp_active != 0u) {
        return;
    }
    s_wf24_sntp_active = 1u;
    s_wf24_sntp_t0 = HAL_GetTick();
    s_wf24_sntp_last_ms = 0u;
    s_wf24_sntp_retry_ms = 0u;
    LOG_ESSENTIAL("[TIME] SNTP query start\r\n");
}

/** MQTT online 后立刻 cfg+首查（勿等 post_reply 订阅 / poll 间隔） */
static uint8_t wf24_modem_sntp_online_fast_sync(void)
{
    if(s_sntp_synced != 0u || s_ntp_synced != 0u || app_wall_clock_valid() != 0u) {
        return 1u;
    }
    if(s_wf24_sntp_active == 0u) {
        wf24_modem_sntp_kick();
    }
    if(s_sntp_cfg_sent == 0u) {
        wf24_sntp_cfg_once_before_mqtt();
    }
    if(s_sntp_cfg_sent != 0u) {
        s_wf24_sntp_last_ms = HAL_GetTick();
        if(aliyun_try_fetch_sntp_once() != 0u) {
            s_wf24_sntp_active = 0u;
            s_wf24_sntp_retry_ms = 0u;
            LOG_ESSENTIAL("[TIME] sync ok\r\n");
            cloud_aliyun_at_time_sync_done();
            return 1u;
        }
    }
    return 0u;
}

static void wf24_modem_sntp_poll(void)
{
    uint32_t now;

    if(s_sntp_synced != 0u || s_ntp_synced != 0u || app_wall_clock_valid() != 0u) {
        s_wf24_sntp_active = 0u;
        return;
    }
    if(s_wf24_sntp_active == 0u) {
        if(s_step == ALIYUN_STEP_ONLINE && s_wf24_sntp_retry_ms != 0u &&
           (int32_t)(HAL_GetTick() - (int32_t)s_wf24_sntp_retry_ms) >= 0) {
            s_wf24_sntp_retry_ms = 0u;
            wf24_modem_sntp_kick();
        }
        return;
    }
    if(s_prop_post_awaiting != 0u) {
        return;
    }
    /* 建 MQTT 期间 UART 被占用，仅在线后查询 */
    if(s_step != ALIYUN_STEP_ONLINE) {
        return;
    }
    now = HAL_GetTick();
    if((now - s_wf24_sntp_t0) >= WF24_SNTP_POLL_MS) {
        s_wf24_sntp_active = 0u;
        s_wf24_sntp_retry_ms = now + 5000u;
        LOG_ESSENTIAL("[TIME] SNTP timeout\r\n");
        return;
    }
    if(s_sntp_cfg_sent == 0u) {
        wf24_sntp_cfg_once_before_mqtt();
        return;
    }
    if(s_wf24_sntp_last_ms != 0u && (now - s_wf24_sntp_last_ms) < WF24_SNTP_QUERY_GAP_MS) {
        return;
    }
    s_wf24_sntp_last_ms = now;
    if(strstr(s_rx_win, "NETWORK TIME") != NULL ||
       strstr(s_rx_win, "NETWORK TIME SUCC") != NULL) {
        cloud_uart2_hw_drain_ms(100u);
    }
    if(aliyun_try_fetch_sntp_once() != 0u) {
        s_wf24_sntp_active = 0u;
        s_wf24_sntp_retry_ms = 0u;
        LOG_ESSENTIAL("[TIME] sync ok\r\n");
        cloud_aliyun_at_time_sync_done();
    }
}
#endif /* APP_WIFI_MODEM_WF24 */
#endif /* APP_ALIYUN_SNTP_ENABLE */

static uint8_t aliyun_connack_scan_bytes(const uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if(buf == NULL || len < 4u) {
        return 0xFFu;
    }
    for(i = 0u; (uint16_t)(i + 3u) < len; i++) {
        if(buf[i] == 0x20u && buf[i + 1u] == 0x02u) {
            if(buf[i + 3u] == 0u) {
                return 1u;
            }
            printf("[ALIYUN] CONNACK denied code=%u (keepalive需30~1200s)\r\n", (unsigned)buf[i + 3u]);
            CLOUD_TRACE_MSG("[CLOUD] MQTT auth fail\r\n");
            app_link_guard_mqtt_end(0u);
            return 0u;
        }
    }
    return 0xFFu;
}

/* 等待 MQTT CONNACK（+IPD 二进制 0x20 0x02 ...） */
static uint8_t aliyun_wait_mqtt_connack(uint32_t timeout_ms)
{
    uint8_t ack[64];
    uint16_t ack_len = 0u;
    uint8_t hit;
    uint8_t ch = 0u;
    uint32_t deadline = HAL_GetTick() + timeout_ms;

    hit = aliyun_connack_scan_bytes((const uint8_t *)s_rx_win, s_rx_win_pos);
    if(hit <= 1u) {
        return hit;
    }

    while((int32_t)(HAL_GetTick() - (int32_t)deadline) < 0) {
        while(uart2_read_byte_timeout(&ch, 6u) != 0) {
            hit = aliyun_connack_scan_bytes((const uint8_t *)s_rx_win, s_rx_win_pos);
            if(hit <= 1u) {
                return hit;
            }
        }
        if(uart2_recv_ipd_payload(ack, sizeof(ack), &ack_len, 80u) != 0) {
            hit = aliyun_connack_scan_bytes(ack, ack_len);
            if(hit <= 1u) {
                return hit;
            }
        }
        uart2_coop_yield();
    }

    hit = aliyun_connack_scan_bytes((const uint8_t *)s_rx_win, s_rx_win_pos);
    return (hit == 1u) ? 1u : 0u;
}

static void aliyun_mqtt_touch_activity(void)
{
    s_last_mqtt_activity_ms = HAL_GetTick();
}

static uint8_t mqtt_send_packet(const uint8_t *pkt, uint16_t pkt_len)
{
#if (APP_WIFI_MODEM_WF24 != 0)
    (void)pkt;
    (void)pkt_len;
    return 0u;
#else
    char cmd[40];
    uint8_t try;

    if(pkt == NULL || pkt_len == 0u) {
        return 0u;
    }
    for(try = 0u; try < 3u; try++) {
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%u\r\n", (unsigned)pkt_len);
        if(uart2_send_text(cmd) && uart2_wait_text(">", ALIYUN_WAIT_MID_MS) &&
           uart2_send_bytes(pkt, pkt_len) &&
           uart2_wait_text("SEND OK", ALIYUN_WAIT_MID_MS)) {
            s_mqtt_tx_fail_streak = 0u;
            aliyun_mqtt_touch_activity();
            return 1u;
        }
        (void)cloud_uart2_pump_rx(40u);
    }
    s_mqtt_tx_fail_streak++;
    if(s_mqtt_tx_fail_streak >= 8u) {
        s_mqtt_tx_fail_streak = 0u;
        aliyun_mqtt_schedule_reconnect("tx fail");
    }
    return 0u;
#endif
}

static void esp_rst_gpio_init_once(void)
{
    GPIO_InitTypeDef gpio = {0};
    if(s_rst_gpio_inited) return;
#if (APP_WIFI_MODEM_WF24 != 0)
    __HAL_RCC_GPIOE_CLK_ENABLE();
#else
    __HAL_RCC_GPIOA_CLK_ENABLE();
#endif
    gpio.Pin = ALIYUN_ESP_RST_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ALIYUN_ESP_RST_PORT, &gpio);
    s_rst_gpio_inited = 1u;
}

static void esp_rst_set(uint8_t high_level)
{
    if(!s_rst_gpio_inited) return;
    HAL_GPIO_WritePin(ALIYUN_ESP_RST_PORT, ALIYUN_ESP_RST_PIN,
                      high_level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void uart2_push_rx_char(char ch)
{
    if(s_rx_win_pos < (sizeof(s_rx_win) - 1u)) {
        s_rx_win[s_rx_win_pos++] = (char)ch;
    } else {
        memmove(s_rx_win, s_rx_win + 1, sizeof(s_rx_win) - 2u);
        s_rx_win[sizeof(s_rx_win) - 2u] = (char)ch;
    }
    s_rx_win[sizeof(s_rx_win) - 1u] = '\0';
}

static void uart2_drain_rx_ms(uint32_t ms)
{
    uint8_t ch = 0u;
    uint32_t end;

    if(!s_uart2_inited) {
        return;
    }
    end = HAL_GetTick() + ms;
    while((HAL_GetTick() < end) != 0u) {
        if(uart2_ring_pop(&ch) != 0u) {
            uart2_push_rx_char((char)ch);
        } else if(s_uart2_irq_on == 0u &&
                  HAL_UART_Receive(&s_uart2, &ch, 1u, 1u) == HAL_OK) {
            uart2_push_rx_char((char)ch);
        } else {
            uart2_coop_yield();
        }
    }
}

static void uart2_log_rx_snapshot(const char *tag)
{
    printf("[ALIYUN] %s rx_bytes=%u", (tag != NULL) ? tag : "uart", (unsigned)s_rx_win_pos);
    if(s_rx_win_pos > 0u) {
        printf(" data=\"%s\"\r\n", s_rx_win);
    } else {
        printf(" (MCU got nothing: check ESP power, PA2/PA3, TX/RX swap)\r\n");
    }
}

#if (APP_WIFI_MODEM_WF24 != 0)
static uint8_t wf24_modem_at_probe(uint8_t tries, uint32_t gap_ms, uint32_t wait_ms)
{
    uint8_t i;

    for(i = 0u; i < tries; i++) {
        cloud_uart2_rx_clear();
        if(uart2_send_text("AT\r\n") && uart2_wait_text("OK", wait_ms)) {
            return 1u;
        }
        if(gap_ms != 0u && (i + 1u) < tries) {
            uart2_drain_rx_ms(gap_ms);
        }
    }
    return 0u;
}

static uint8_t wf24_modem_key_recover(const char *tag)
{
    const char *lbl = (tag != NULL) ? tag : "recover";

    printf("[ALIYUN] %s: KEY reset\r\n", lbl);
    cloud_uart2_esp_hw_reset(2500u);
    cloud_uart2_flush_rx();
    (void)cloud_uart2_wait_esp_ready(6000u);
    if(cloud_uart2_try_modem_ready() != 0u) {
        modem_at_echo_off();
        printf("[ALIYUN] %s: modem AT OK after KEY reset\r\n", lbl);
        return 1u;
    }
    uart2_log_rx_snapshot(lbl);
    return 0u;
}
#endif

static void uart2_init_once(void)
{
    GPIO_InitTypeDef gpio = {0};

    if(s_uart2_inited) return;

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    s_uart2.Instance = USART2;
    s_uart2.Init.BaudRate = s_uart2_baud;
    s_uart2.Init.WordLength = UART_WORDLENGTH_8B;
    s_uart2.Init.StopBits = UART_STOPBITS_1;
    s_uart2.Init.Parity = UART_PARITY_NONE;
    s_uart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_uart2.Init.Mode = UART_MODE_TX_RX;
    if(HAL_UART_Init(&s_uart2) == HAL_OK) {
        s_uart2_inited = 1u;
        uart2_irq_enable();
        printf("[ALIYUN] UART2 init OK baud=%lu\r\n", (unsigned long)s_uart2_baud);
    } else {
        printf("[ALIYUN] UART2 HAL_Init FAIL\r\n");
    }
}

static void uart2_set_baud(uint32_t baud)
{
    if(!s_uart2_inited) return;
    if(s_uart2_baud == baud) return;
    HAL_UART_DeInit(&s_uart2);
    s_uart2_baud = baud;
    s_uart2.Init.BaudRate = s_uart2_baud;
    if(HAL_UART_Init(&s_uart2) != HAL_OK) {
        s_uart2_inited = 0u;
    } else {
        s_uart2_irq_on = 0u;
        uart2_irq_enable();
    }
}

static int uart2_send_text(const char *txt)
{
    if(!s_uart2_inited || txt == NULL) return 0;
    return (HAL_UART_Transmit(&s_uart2, (uint8_t *)txt, (uint16_t)strlen(txt), 1200u) == HAL_OK) ? 1 : 0;
}

static int uart2_send_bytes(const uint8_t *buf, uint16_t len)
{
    if(!s_uart2_inited || buf == NULL || len == 0u) return 0;
    return (HAL_UART_Transmit(&s_uart2, (uint8_t *)buf, len, 1500u) == HAL_OK) ? 1 : 0;
}

static int uart2_wait_text(const char *needle, uint32_t timeout_ms)
{
    uint8_t ch = 0u;
    uint32_t start = HAL_GetTick();
    uint16_t nlen;

    if(!s_uart2_inited || needle == NULL || needle[0] == '\0') return 0;
    nlen = (uint16_t)strlen(needle);
    if(nlen >= sizeof(s_rx_win)) return 0;
    if(timeout_ms == 0u) return 0;
    if(strstr(s_rx_win, needle) != NULL) return 1;

    while((HAL_GetTick() - start) < timeout_ms) {
        if(strstr(s_rx_win, needle) != NULL) return 1;
#if (MODEM_USE_NATIVE_MQTT != 0)
        if(s_step == ALIYUN_STEP_ONLINE) {
            wf24_poll_mqtt_urc();
        }
#endif
        if(uart2_read_byte_timeout(&ch, 1u) != 0) {
            if(strstr(s_rx_win, needle) != NULL) {
                return 1;
            }
        }
        uart2_coop_yield();
    }

    if(strstr(s_rx_win, needle) != NULL) return 1;
    return 0;
}

/* 1=GOT IP  -1=FAIL/+CWJAP  0=超时（仅 CONNECTED 未拿到 IP 也算失败） */
static void cwjap_trace_rx_tail(void)
{
    uint16_t n;
    uint16_t start;
    uint16_t i;
    static char line[88];

    if(s_rx_win_pos == 0u) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP rx: (empty)\r\n");
        return;
    }
    n = s_rx_win_pos;
    start = (n > 56u) ? (uint16_t)(n - 56u) : 0u;
    line[0] = '[';
    line[1] = 'W';
    line[2] = 'i';
    line[3] = 'F';
    line[4] = 'i';
    line[5] = ']';
    line[6] = ' ';
    line[7] = 'r';
    line[8] = 'x';
    line[9] = ':';
    line[10] = ' ';
    i = 11u;
    while(start < n && i < (sizeof(line) - 3u)) {
        char c = s_rx_win[start++];
        if(c == '\r' || c == '\n') {
            c = '.';
        }
        line[i++] = c;
    }
    line[i++] = '\r';
    line[i++] = '\n';
    line[i] = '\0';
    CWJAP_TRACE_MSG(line);
}

static void cwjap_build_cmd(char *cmd, size_t cmd_sz)
{
    const char *ssid = app_wifi_cfg_get_ssid();
    const char *pwd = app_wifi_cfg_get_password();
    uint8_t ch;
    size_t n = 0u;

    if(cmd == NULL || cmd_sz < 24u) {
        return;
    }
    if(ssid == NULL) {
        ssid = "";
    }
    if(pwd == NULL) {
        pwd = "";
    }
    ch = app_wifi_scan_get_ssid_channel(ssid);
#if (APP_WIFI_MODEM_WF24 != 0)
    memcpy(cmd, "AT+CWJAP=", 9u);
    n = 9u;
    while(*ssid != '\0' && (n + 8u) < cmd_sz) {
        cmd[n++] = *ssid++;
    }
    cmd[n++] = ',';
    while(*pwd != '\0' && (n + 6u) < cmd_sz) {
        cmd[n++] = *pwd++;
    }
#else
    memcpy(cmd, "AT+CWJAP=\"", 10u);
    n = 10u;
    while(*ssid != '\0' && (n + 8u) < cmd_sz) {
        cmd[n++] = *ssid++;
    }
    cmd[n++] = '"';
    cmd[n++] = ',';
    cmd[n++] = '"';
    while(*pwd != '\0' && (n + 6u) < cmd_sz) {
        cmd[n++] = *pwd++;
    }
    cmd[n++] = '"';
#endif
    if(s_cwjap_try_channel != 0u && ch >= 1u && ch <= 14u && (n + 4u) < cmd_sz) {
        cmd[n++] = ',';
        if(ch >= 10u) {
            cmd[n++] = (char)('0' + (ch / 10u));
        }
        cmd[n++] = (char)('0' + (ch % 10u));
    }
    cmd[n++] = '\r';
    cmd[n++] = '\n';
    cmd[n] = '\0';
}

/* connect 前已 teardown_idle：勿再连发十几条 AT，否则 CWJAP 易 busy p */
static void cwjap_uart_settle_fast(void)
{
    if(s_cwjap_esp_idle_ok != 0u) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP fast settle skip (idle ok)\r\n");
        s_modem_at_ok = 1u;
        return;
    }
    CWJAP_TRACE_MSG("[WiFi] CWJAP fast settle\r\n");
    g_wifi_scan_abort = 1u;
    g_wifi_scan_pending = 0u;
    cloud_uart2_flush_rx();
    cloud_uart2_wait_quiet_after_scan_abort(CWJAP_FAST_SETTLE_QUIET_MS);
    cloud_uart2_rx_clear();
    if(uart2_at_cmd_wait_ok("AT\r\n", CWJAP_FAST_SETTLE_AT_MS) != 0u) {
        s_modem_at_ok = 1u;
    } else {
        s_modem_at_ok = 0u;
    }
    cloud_uart2_rx_clear();
}

static void cwjap_uart_settle(void)
{
    /* connect 前已 teardown_idle 时勿再跑 modem_purge(3.5s+多轮 AT)，易误杀 CWJAP */
    cwjap_uart_settle_fast();
}

static void cwjap_trace_fail_reason(void)
{
#if (APP_WIFI_CWJAP_TRACE != 0)
    if(strstr(s_rx_win, "+CWJAP:2") != NULL) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: wrong password\r\n");
    } else if(strstr(s_rx_win, "+CWJAP:3") != NULL ||
              cloud_uart2_rx_has("NO AP") != 0 ||
              cloud_uart2_rx_has("no ap") != 0) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: No AP\r\n");
    } else if(strstr(s_rx_win, "+CWJAP:4") != NULL) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: assoc fail\r\n");
    } else if(cloud_uart2_rx_has("busy p") != 0) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: busy p\r\n");
    } else if(cloud_uart2_rx_has("WIFI DISCONNECT") != 0) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: disconnect\r\n");
    } else if(strstr(s_rx_win, "+CWJAP:1,") == NULL &&
              strstr(s_rx_win, "+CWJAP:1") != NULL) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: timeout\r\n");
    } else if(cloud_uart2_rx_has("+CWJAP:") != 0) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: auth/assoc fail\r\n");
    } else if(cloud_uart2_rx_has("FAIL") != 0) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: FAIL\r\n");
    } else {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: timeout\r\n");
    }
    cwjap_trace_rx_tail();
#elif (APP_LOG_ESSENTIAL != 0)
    if(strstr(s_rx_win, "+CWJAP:2") != NULL) {
        LOG_ESSENTIAL("[WiFi] CWJAP fail: wrong password\r\n");
    } else if(strstr(s_rx_win, "+CWJAP:3") != NULL ||
              cloud_uart2_rx_has("NO AP") != 0 ||
              cloud_uart2_rx_has("no ap") != 0) {
        LOG_ESSENTIAL("[WiFi] CWJAP fail: no AP\r\n");
    } else if(cloud_uart2_rx_has("busy p") != 0) {
        LOG_ESSENTIAL("[WiFi] CWJAP fail: busy\r\n");
    } else {
        LOG_ESSENTIAL("[WiFi] CWJAP fail\r\n");
    }
#else
    (void)0;
#endif
}

static int uart2_wait_wifi_sta_connected(uint32_t timeout_ms);

#if (APP_WIFI_MODEM_WF24 != 0)
static uint8_t wf24_cwjap_poll_status(void)
{
    const char *p;
    const char *c1;
    const char *c2;
    const char *ip;
    uint8_t st = 0u;

    if(wf24_query_cwstate(&st, NULL, 0u) != 0u && st == 2u) {
        return 1u;
    }
    if(!uart2_send_text("AT+CWJAP?\r\n") ||
       !uart2_wait_text("+CWJAP:", 2500u)) {
        return 0u;
    }
    p = wf24_find_last_cwjap_ok(s_rx_win);
    if(p == NULL) {
        return 0u;
    }
    c1 = strchr(p, ',');
    if(c1 == NULL) {
        return 0u;
    }
    c2 = strchr(c1 + 1u, ',');
    if(c2 == NULL) {
        return 0u;
    }
    ip = c2 + 1u;
    while(*ip == ' ' || *ip == '\t') {
        ip++;
    }
    if(*ip == '\'' || *ip == '\"') {
        ip++;
    }
    return (*ip >= '1' && *ip <= '9') ? 1u : 0u;
}
#endif

/* 发 CWJAP；若立即 busy p 则静默后重发（不再插 AT 探活） */
static int cwjap_send_cmd_wait(const char *cmd, uint32_t timeout_ms)
{
    uint8_t n;

    if(cmd == NULL || cmd[0] == '\0') {
        return 0;
    }
    for(n = 0u; n < 3u; n++) {
        cloud_uart2_rx_clear();
        if(!uart2_send_text(cmd)) {
            continue;
        }
        {
            int r = uart2_wait_wifi_sta_connected(timeout_ms);

            if(r > 0) {
                return 1;
            }
            if(strstr(s_rx_win, "busy p") != NULL && n < 2u) {
                CWJAP_TRACE_MSG("[WiFi] CWJAP busy wait\r\n");
                cloud_uart2_wait_quiet_after_scan_abort(CWJAP_BUSY_RESEND_QUIET_MS);
                continue;
            }
            return r;
        }
    }
    return 0;
}

static void cloud_aliyun_at_wifi_link_lost(const char *reason);

static int uart2_wait_wifi_sta_connected(uint32_t timeout_ms)
{
    uint8_t ch = 0u;
    uint32_t start = HAL_GetTick();
    uint32_t last_poll = start;

    if(timeout_ms == 0u) {
        return 0;
    }
    /* 勿在此 clear：会擦掉 CWJAP 刚返回的 busy p / FAIL */
    while((HAL_GetTick() - start) < timeout_ms) {
        if(uart2_read_byte_timeout(&ch, 1u) != 0) {
            if(modem_wifi_got_ip_in_rx() != 0u) {
                return 1;
            }
#if (APP_WIFI_MODEM_WF24 != 0)
            if(wf24_cwjap_fail_in_rx() != 0u) {
                printf("[ALIYUN] CWJAP wait fail token rx=%s\r\n", s_rx_win);
                return -1;
            }
#else
            if(strstr(s_rx_win, "+CWJAP:") != NULL ||
               strstr(s_rx_win, "FAIL") != NULL ||
               strstr(s_rx_win, "busy p") != NULL ||
               strstr(s_rx_win, "no ap") != NULL ||
               strstr(s_rx_win, "NO AP") != NULL) {
                printf("[ALIYUN] CWJAP wait fail token rx=%s\r\n", s_rx_win);
                return -1;
            }
#endif
        }
#if (APP_WIFI_MODEM_WF24 != 0)
        if((HAL_GetTick() - last_poll) >= 2000u &&
           strstr(s_rx_win, "OK") != NULL &&
           strstr(s_rx_win, "+CWJAP:") == NULL) {
            last_poll = HAL_GetTick();
            if(wf24_cwjap_poll_status() != 0u) {
                return 1;
            }
        }
#endif
        uart2_coop_yield();
    }
    if(modem_wifi_got_ip_in_rx() != 0u) {
        return 1;
    }
#if (APP_WIFI_MODEM_WF24 != 0)
    if(wf24_cwjap_fail_in_rx() != 0u) {
        printf("[ALIYUN] CWJAP wait fail(last) rx=%s\r\n", s_rx_win);
        return -1;
    }
    if(wf24_cwjap_dhcp_pending() != 0u) {
        uint32_t extra_start = HAL_GetTick();

        while((HAL_GetTick() - extra_start) < CWJAP_GOT_IP_WAIT_MS) {
            if(uart2_read_byte_timeout(&ch, 1u) != 0) {
                if(modem_wifi_got_ip_in_rx() != 0u) {
                    return 1;
                }
                if(wf24_cwjap_fail_in_rx() != 0u) {
                    printf("[ALIYUN] CWJAP wait fail(dhcp) rx=%s\r\n", s_rx_win);
                    return -1;
                }
            }
            uart2_coop_yield();
        }
        if(modem_wifi_got_ip_in_rx() != 0u) {
            return 1;
        }
    }
#else
    if(strstr(s_rx_win, "+CWJAP:") != NULL ||
       strstr(s_rx_win, "FAIL") != NULL ||
       strstr(s_rx_win, "busy p") != NULL ||
       strstr(s_rx_win, "no ap") != NULL ||
       strstr(s_rx_win, "NO AP") != NULL) {
        printf("[ALIYUN] CWJAP wait fail(last) rx=%s\r\n", s_rx_win);
        return -1;
    }
    if(modem_wifi_got_ip_in_rx() == 0u &&
       strstr(s_rx_win, "WIFI DISCONNECT") != NULL &&
       strstr(s_rx_win, "WIFI GOT IP") == NULL &&
       strstr(s_rx_win, "WIFI CONNECTED") == NULL) {
        printf("[ALIYUN] CWJAP wait disconnect rx=%s\r\n", s_rx_win);
        cloud_aliyun_at_wifi_link_lost("WIFI DISCONNECT during CWJAP");
        return -1;
    }
#endif
    if(modem_wifi_got_ip_in_rx() != 0u) {
        return 1;
    }
#if (APP_WIFI_MODEM_WF24 != 0)
    if(wf24_cwjap_poll_status() != 0u) {
        return 1;
    }
#endif
    printf("[ALIYUN] CWJAP wait timeout %lums (no GOT IP) rx=%s\r\n",
           (unsigned long)timeout_ms, s_rx_win);
    return 0;
}

/* busy p 后等待串口安静，避免立刻重发 CWJAP 继续撞上 CWLAP */
static void cloud_uart2_wait_quiet_after_scan_abort(uint32_t quiet_ms)
{
    uint32_t t0 = HAL_GetTick();
    uint8_t ch = 0u;

    cloud_uart2_rx_clear();
    while((HAL_GetTick() - t0) < quiet_ms) {
        if(uart2_read_byte_timeout(&ch, 3u) != 0) {
            /* 仍在吐扫描数据，重新计时 */
            t0 = HAL_GetTick();
        } else {
            uart2_coop_yield();
        }
    }
}

void cloud_uart2_wait_scan_tail_quiet(uint32_t quiet_ms)
{
    cloud_uart2_wait_quiet_after_scan_abort(quiet_ms);
}

static int uart2_read_byte_timeout(uint8_t *out, uint32_t timeout_ms)
{
    uint32_t end;

    if(out == NULL) {
        return 0;
    }
    if(timeout_ms == 0u) {
        if(uart2_ring_pop(out) != 0u) {
            uart2_push_rx_char((char)(*out));
            return 1;
        }
        return 0;
    }
    end = HAL_GetTick() + timeout_ms;
    while((int32_t)(HAL_GetTick() - (int32_t)end) < 0) {
        if(uart2_ring_pop(out) != 0u) {
            uart2_push_rx_char((char)(*out));
            return 1;
        }
        if(s_uart2_irq_on == 0u) {
            if(HAL_UART_Receive(&s_uart2, out, 1u, 0u) == HAL_OK) {
                uart2_push_rx_char((char)(*out));
                return 1;
            }
        }
        uart2_coop_yield();
    }
    return 0;
}

static uint16_t ipd_parse_len_field(const char *hdr, const char *colon)
{
    uint16_t last_num = 0u;
    uint16_t curr_num = 0u;
    uint8_t saw_digit = 0u;
    const char *q;

    if(hdr == NULL || colon == NULL || colon <= hdr) {
        return 0u;
    }
    for(q = hdr + 5; q < colon; q++) {
        if(*q >= '0' && *q <= '9') {
            curr_num = (uint16_t)(curr_num * 10u + (uint16_t)(*q - '0'));
            saw_digit = 1u;
            if(curr_num > 2048u) {
                return 0u;
            }
        } else if(*q == ',') {
            if(saw_digit != 0u) {
                last_num = curr_num;
                curr_num = 0u;
                saw_digit = 0u;
            }
        }
    }
    if(saw_digit != 0u) {
        last_num = curr_num;
    }
    return last_num;
}

static void rx_win_consume(uint16_t nbytes)
{
    if(nbytes == 0u) {
        return;
    }
    if(nbytes >= s_rx_win_pos) {
        s_rx_win_pos = 0u;
        s_rx_win[0] = '\0';
        return;
    }
    memmove(s_rx_win, s_rx_win + nbytes, (size_t)(s_rx_win_pos - nbytes));
    s_rx_win_pos = (uint16_t)(s_rx_win_pos - nbytes);
    s_rx_win[s_rx_win_pos] = '\0';
}

static int uart2_ipd_take_from_rx_win(uint8_t *payload, uint16_t payload_sz, uint16_t *out_len)
{
    char *hdr;
    char *colon;
    uint16_t len;
    uint16_t hdr_end;
    uint16_t in_win;
    uint16_t copy_n;
    uint16_t i;

    hdr = strstr(s_rx_win, "+IPD,");
    if(hdr == NULL) {
        return 0;
    }
    colon = strchr(hdr, ':');
    if(colon == NULL) {
        return 0;
    }
    len = ipd_parse_len_field(hdr, colon);
    if(len == 0u) {
        return 0;
    }
    hdr_end = (uint16_t)((colon + 1) - s_rx_win);
    in_win = (s_rx_win_pos > hdr_end) ? (uint16_t)(s_rx_win_pos - hdr_end) : 0u;
    copy_n = (len < payload_sz) ? len : payload_sz;
    if(payload != NULL && copy_n > 0u) {
        for(i = 0u; i < copy_n; i++) {
            payload[i] = (uint8_t)s_rx_win[hdr_end + i];
        }
    }
    if(in_win < len) {
        return 0;
    }
    rx_win_consume((uint16_t)(hdr_end + len));
    if(out_len != NULL) {
        *out_len = copy_n;
    }
    return 1;
}

static int uart2_recv_ipd_payload(uint8_t *payload, uint16_t payload_sz, uint16_t *out_len, uint32_t timeout_ms)
{
    uint8_t ch = 0u;
    uint32_t start = HAL_GetTick();
    uint16_t last_num = 0u;
    uint16_t curr_num = 0u;
    uint8_t saw_digit = 0u;
    uint16_t len = 0u;
    uint16_t i;
    uint16_t copy_n = 0u;
    char *hdr;
    char *colon;

    if(out_len != NULL) {
        *out_len = 0u;
    }
    (void)cloud_uart2_pump_rx(12u);
    if(uart2_ipd_take_from_rx_win(payload, payload_sz, out_len) != 0) {
        return 1;
    }

    if(!uart2_wait_text("+IPD,", timeout_ms)) {
        return 0;
    }

    hdr = strstr(s_rx_win, "+IPD,");
    colon = (hdr != NULL) ? strchr(hdr, ':') : NULL;
    if(hdr != NULL && colon != NULL) {
        if(uart2_ipd_take_from_rx_win(payload, payload_sz, out_len) != 0) {
            return 1;
        }
    }

    start = HAL_GetTick();
    while((HAL_GetTick() - start) < timeout_ms) {
        if(!uart2_read_byte_timeout(&ch, 2u)) {
            continue;
        }
        if(ch == ':') {
            if(saw_digit != 0u) {
                last_num = curr_num;
            }
            break;
        }
        if(ch >= '0' && ch <= '9') {
            saw_digit = 1u;
            curr_num = (uint16_t)(curr_num * 10u + (uint16_t)(ch - '0'));
            if(curr_num > 2048u) {
                return 0;
            }
        } else if(ch == ',') {
            if(saw_digit != 0u) {
                last_num = curr_num;
                curr_num = 0u;
                saw_digit = 0u;
            }
        }
    }
    if(ch != ':') {
        return 0;
    }
    len = last_num;
    if(len == 0u) {
        return 0;
    }
    copy_n = (len < payload_sz) ? len : payload_sz;

    for(i = 0u; i < len; i++) {
        if(!uart2_read_byte_timeout(&ch, 20u)) {
            return 0;
        }
        if(i < copy_n && payload != NULL) {
            payload[i] = ch;
        }
    }
    if(out_len != NULL) {
        *out_len = copy_n;
    }
    return 1;
}

static uint8_t aliyun_mqtt_ipd_means_alive(const uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if(buf == NULL || len < 2u) {
        return 0u;
    }
    if(buf[0] == 0x30u || buf[0] == 0x31u || buf[0] == 0x90u) {
        return 1u;
    }
    if(buf[0] == 0xD0u && buf[1] == 0x00u) {
        return 1u;
    }
    for(i = 0u; (uint16_t)(i + 1u) < len; i++) {
        if(buf[i] == 0xD0u && buf[i + 1u] == 0x00u) {
            return 1u;
        }
    }
    return 0u;
}

static uint8_t aliyun_send_ping_and_check(void)
{
#if (APP_WIFI_MODEM_WF24 != 0)
    /* WF24 模组内部 MQTT keepalive */
    aliyun_mqtt_touch_activity();
    return 1u;
#else
    static const uint8_t ping_pkt[2] = {0xC0u, 0x00u}; /* MQTT PINGREQ */
    uint8_t resp[192];
    uint16_t rlen = 0u;
    uint32_t deadline;

    if(!uart2_send_text("AT+CIPSEND=0,2\r\n")) return 0u;
    if(!uart2_wait_text(">", ALIYUN_WAIT_MID_MS)) return 0u;
    if(!uart2_send_bytes(ping_pkt, 2u)) return 0u;
    if(!uart2_wait_text("SEND OK", ALIYUN_WAIT_MID_MS)) return 0u;

    /* 轮询 +IPD：须分发下行 PUBLISH/SUBACK，勿误判超时重连 */
    deadline = HAL_GetTick() + ALIYUN_MQTT_PING_WAIT_MS;
    while((int32_t)(HAL_GetTick() - deadline) < 0) {
        uart2_drain_rx_ms(40u);
        if(s_ntp_synced == 0u && aliyun_ntp_try_parse_any() != 0u) {
            return 1u;
        }
        if(strstr(s_rx_win, "\xD0\x00") != NULL) {
            return 1u;
        }
        if(!uart2_recv_ipd_payload(resp, sizeof(resp), &rlen, 60u)) {
            continue;
        }
        (void)aliyun_ntp_handle_ipd(resp, rlen);
        if(aliyun_mqtt_ipd_means_alive(resp, rlen) != 0u) {
            return 1u;
        }
    }
    uart2_pump_rx_core(40u, 1u);
    if(strstr(s_rx_win, "\xD0\x00") != NULL) {
        return 1u;
    }
    return 0u;
#endif
}

static const char *aliyun_step_name(aliyun_step_t s)
{
    switch(s) {
    case ALIYUN_STEP_AT: return "AT";
    case ALIYUN_STEP_ATE0: return "ATE0";
    case ALIYUN_STEP_CWMODE: return "CWMODE";
    case ALIYUN_STEP_CIPMODE: return "CIPMODE";
    case ALIYUN_STEP_CIPMUX: return "CIPMUX";
    case ALIYUN_STEP_CWJAPQ: return "CWJAP?";
    case ALIYUN_STEP_CWJAP_SET: return "CWJAP_SET";
    case ALIYUN_STEP_WIFI_IDLE: return "WIFI_IDLE";
    case ALIYUN_STEP_CIFSR: return "CIFSR";
#if (APP_ALIYUN_SNTP_ENABLE == 1)
    case ALIYUN_STEP_SNTP_CFG: return "SNTP_CFG";
    case ALIYUN_STEP_SNTP_WAIT: return "SNTP_WAIT";
    case ALIYUN_STEP_SNTP_TIME: return "SNTP_TIME";
#endif
    case ALIYUN_STEP_CIPSTART: return "CIPSTART";
    case ALIYUN_STEP_CIPSEND: return "CIPSEND";
    case ALIYUN_STEP_SEND_CONNECT: return "SEND_MQTT_CONNECT";
    case ALIYUN_STEP_WAIT_CONNACK: return "WAIT_CONNACK";
    case ALIYUN_STEP_ONLINE: return "ONLINE";
    default: return "IDLE";
    }
}

static uint16_t mqtt_write_utf8(uint8_t *dst, const char *s)
{
    uint16_t len = (uint16_t)strlen(s);
    dst[0] = (uint8_t)(len >> 8);
    dst[1] = (uint8_t)(len & 0xFFu);
    memcpy(&dst[2], s, len);
    return (uint16_t)(len + 2u);
}

static uint16_t mqtt_encode_rem_len(uint8_t *dst, uint16_t len)
{
    uint16_t pos = 0u;
    do {
        uint8_t digit = (uint8_t)(len % 128u);
        len = (uint16_t)(len / 128u);
        if(len > 0u) digit |= 0x80u;
        dst[pos++] = digit;
    } while(len > 0u && pos < 4u);
    return pos;
}

static uint16_t build_mqtt_connect(uint8_t *out, uint16_t out_sz,
                                    const char *client_id, const char *user_name, const char *password,
                                    const char *will_topic, const char *will_payload)
{
    uint8_t vh[16];
    uint16_t vh_len = 0u;
    uint8_t payload[300];
    uint16_t payload_len = 0u;
    uint8_t rem_enc[4];
    uint16_t rem_enc_len;
    uint16_t rem_len;
    uint16_t pos = 0u;
    uint8_t connect_flags = 0xC2u; /* clean session + username + password */

    if(out == NULL || client_id == NULL || user_name == NULL || password == NULL) {
        return 0u;
    }
    if(will_topic != NULL && will_payload != NULL &&
       will_topic[0] != '\0' && will_payload[0] != '\0') {
        connect_flags |= 0x04u; /* Will flag, QoS0, no retain */
    }

    vh_len += mqtt_write_utf8(vh + vh_len, "MQTT");
    vh[vh_len++] = 0x04u;
    vh[vh_len++] = connect_flags;
    vh[vh_len++] = 0x00u;
    vh[vh_len++] = 50u; /* keep alive 50s（阿里云要求 30~1200s，<30 会 CONNACK 拒绝） */

    payload_len += mqtt_write_utf8(payload + payload_len, client_id);
    if((connect_flags & 0x04u) != 0u) {
        payload_len += mqtt_write_utf8(payload + payload_len, will_topic);
        payload_len += mqtt_write_utf8(payload + payload_len, will_payload);
    }
    payload_len += mqtt_write_utf8(payload + payload_len, user_name);
    payload_len += mqtt_write_utf8(payload + payload_len, password);

    rem_len = (uint16_t)(vh_len + payload_len);
    rem_enc_len = mqtt_encode_rem_len(rem_enc, rem_len);
    if(rem_enc_len == 0u) {
        return 0u;
    }
    if((uint16_t)(1u + rem_enc_len + rem_len) > out_sz) {
        return 0u;
    }

    out[pos++] = 0x10u;
    memcpy(out + pos, rem_enc, rem_enc_len);
    pos = (uint16_t)(pos + rem_enc_len);
    memcpy(out + pos, vh, vh_len);
    pos = (uint16_t)(pos + vh_len);
    memcpy(out + pos, payload, payload_len);
    pos = (uint16_t)(pos + payload_len);
    return pos;
}

static uint16_t build_mqtt_publish_qos0(uint8_t *out, uint16_t out_sz,
                                        const char *topic, const char *payload)
{
    uint16_t topic_len;
    uint16_t payload_len;
    uint16_t rem_len;
    uint8_t rem_enc[4];
    uint16_t rem_enc_len;
    uint16_t pos = 0u;

    if(out == NULL || topic == NULL || payload == NULL) return 0u;
    topic_len = (uint16_t)strlen(topic);
    payload_len = (uint16_t)strlen(payload);
    rem_len = (uint16_t)(2u + topic_len + payload_len);
    rem_enc_len = mqtt_encode_rem_len(rem_enc, rem_len);
    if(rem_enc_len == 0u) return 0u;
    if((uint16_t)(1u + rem_enc_len + rem_len) > out_sz) return 0u;

    out[pos++] = 0x30u; /* PUBLISH QoS0 */
    memcpy(out + pos, rem_enc, rem_enc_len);
    pos = (uint16_t)(pos + rem_enc_len);
    out[pos++] = (uint8_t)(topic_len >> 8);
    out[pos++] = (uint8_t)(topic_len & 0xFFu);
    memcpy(out + pos, topic, topic_len);
    pos = (uint16_t)(pos + topic_len);
    memcpy(out + pos, payload, payload_len);
    pos = (uint16_t)(pos + payload_len);
    return pos;
}

static uint16_t build_mqtt_subscribe_qos0(uint8_t *out, uint16_t out_sz, uint16_t pkt_id, const char *topic)
{
    uint16_t topic_len;
    uint16_t rem_len;
    uint8_t rem_enc[4];
    uint16_t rem_enc_len;
    uint16_t pos = 0u;

    if(out == NULL || topic == NULL) return 0u;
    topic_len = (uint16_t)strlen(topic);
    rem_len = (uint16_t)(2u + 2u + topic_len + 1u); /* pkt id + topic utf8 + req qos */
    rem_enc_len = mqtt_encode_rem_len(rem_enc, rem_len);
    if(rem_enc_len == 0u) return 0u;
    if((uint16_t)(1u + rem_enc_len + rem_len) > out_sz) return 0u;

    out[pos++] = 0x82u; /* SUBSCRIBE */
    memcpy(out + pos, rem_enc, rem_enc_len);
    pos = (uint16_t)(pos + rem_enc_len);
    out[pos++] = (uint8_t)(pkt_id >> 8);
    out[pos++] = (uint8_t)(pkt_id & 0xFFu);
    out[pos++] = (uint8_t)(topic_len >> 8);
    out[pos++] = (uint8_t)(topic_len & 0xFFu);
    memcpy(out + pos, topic, topic_len);
    pos = (uint16_t)(pos + topic_len);
    out[pos++] = 0x00u; /* requested qos0 */
    return pos;
}

static uint8_t parse_ntp_json_u64_ms(const uint8_t *buf, uint16_t len, const char *key, uint64_t *out_ms)
{
    uint16_t i;
    uint16_t klen;

    if(buf == NULL || key == NULL || out_ms == NULL) {
        return 0u;
    }
    klen = (uint16_t)strlen(key);
    for(i = 0u; i + klen < len; i++) {
        if(memcmp(&buf[i], key, klen) == 0) {
            uint16_t j = (uint16_t)(i + klen);
            uint64_t v = 0u;
            uint8_t hit_digit = 0u;
            while(j < len && (buf[j] == ' ' || buf[j] == '"' || buf[j] == ':' || buf[j] == '\t')) {
                j++;
            }
            while(j < len && buf[j] >= '0' && buf[j] <= '9') {
                v = v * 10u + (uint64_t)(buf[j] - '0');
                hit_digit = 1u;
                j++;
            }
            if(hit_digit != 0u) {
                *out_ms = v;
                return 1u;
            }
        }
    }
    return 0u;
}

static uint8_t epoch_to_local_datetime(uint64_t epoch_sec, int tz_hour, int *y, int *mo, int *d, int *h, int *mi, int *s)
{
    static const uint8_t mdays_norm[12] = {31u,28u,31u,30u,31u,30u,31u,31u,30u,31u,30u,31u};
    uint64_t days;
    uint32_t sod;
    int year = 1970;
    int month;
    int day;
    int hour;
    int minute;
    int sec;
    int leap;
    uint32_t day_sec;

    if(y == NULL || mo == NULL || d == NULL || h == NULL || mi == NULL || s == NULL) return 0u;
    day_sec = (uint32_t)(tz_hour * 3600);
    epoch_sec += day_sec;
    days = epoch_sec / 86400u;
    sod = (uint32_t)(epoch_sec % 86400u);

    while(1) {
        leap = ((year % 400 == 0) || ((year % 4 == 0) && (year % 100 != 0))) ? 1 : 0;
        if(days >= (uint64_t)(leap ? 366 : 365)) days -= (uint64_t)(leap ? 366 : 365);
        else break;
        year++;
        if(year > 2099) return 0u;
    }
    month = 1;
    while(month <= 12) {
        int dm = mdays_norm[month - 1];
        if(month == 2 && leap) dm = 29;
        if(days >= (uint64_t)dm) {
            days -= (uint64_t)dm;
            month++;
        } else break;
    }
    day = (int)days + 1;
    hour = (int)(sod / 3600u);
    minute = (int)((sod % 3600u) / 60u);
    sec = (int)(sod % 60u);

    *y = year; *mo = month; *d = day; *h = hour; *mi = minute; *s = sec;
    return 1u;
}

#if (APP_ALIYUN_AT_ENABLE == 1)
static uint64_t ntp_norm_server_ms(uint64_t v)
{
    if(v >= 1000000000000ull) {
        return v;
    }
    if(v >= 1000000000ull) {
        return v * 1000ull;
    }
    return 0u;
}

static uint8_t aliyun_ntp_apply_from_buf(const uint8_t *buf, uint16_t len)
{
    uint64_t server_recv = 0u;
    uint64_t server_send = 0u;
    uint64_t server_recv_ms = 0u;
    uint64_t server_send_ms = 0u;
    uint64_t device_recv;
    uint64_t device_send;
    uint64_t accurate_ms;
    uint16_t i;
    int y, mo, d, h, mi, s;

    if(buf == NULL || len == 0u) {
        return 0u;
    }
    if(parse_ntp_json_u64_ms(buf, len, "serverSendTime", &server_send) == 0u &&
       parse_ntp_json_u64_ms(buf, len, "serverRecvTime", &server_send) == 0u) {
        for(i = 0u; i < len; i++) {
            if(buf[i] == '{') {
                uint16_t sub = (uint16_t)(len - i);
                if(parse_ntp_json_u64_ms(buf + i, sub, "serverSendTime", &server_send) != 0u ||
                   parse_ntp_json_u64_ms(buf + i, sub, "serverRecvTime", &server_send) != 0u) {
                    break;
                }
                return 0u;
            }
        }
        if(server_send == 0u) {
            return 0u;
        }
    }
    server_send_ms = ntp_norm_server_ms(server_send);
    if(server_send_ms == 0u) {
        return 0u;
    }
    device_send = (uint64_t)s_ntp_req_dev_ms;
    device_recv = (uint64_t)HAL_GetTick();
    /* 勿把 HAL_GetTick 与 Unix ms 直接相加平均（会得到 1998 等错误年份） */
    accurate_ms = server_send_ms;
    if(parse_ntp_json_u64_ms(buf, len, "serverRecvTime", &server_recv) != 0u) {
        server_recv_ms = ntp_norm_server_ms(server_recv);
        if(server_recv_ms != 0u) {
            accurate_ms = (server_send_ms + server_recv_ms) / 2u;
        }
    }
    if(device_recv > device_send) {
        accurate_ms += (device_recv - device_send) / 2u;
    }
    if(accurate_ms < 1000000000000ull) {
        TIME_TRACE_MSG("[TIME] NTP bad epoch ms\r\n");
        return 0u;
    }
    if(epoch_to_local_datetime(accurate_ms / 1000u, APP_ALIYUN_SNTP_TZ_HOURS, &y, &mo, &d, &h, &mi, &s) == 0u) {
        TIME_TRACE_MSG("[TIME] NTP epoch to local fail\r\n");
        return 0u;
    }
    if(y < 2020) {
        TIME_TRACE_MSG("[TIME] NTP year invalid\r\n");
        return 0u;
    }
    app_home_wall_clock_set(y, mo, d, h, mi, s);
    app_home_wall_clock_refresh_ui();
    s_ntp_await_until_ms = 0u;
    TIME_TRACE_MSG("[TIME] sync ok (MQTT NTP)\r\n");
    LOG_ESSENTIAL("[TIME] sync ok\r\n");
    cloud_aliyun_at_time_sync_done();
    return 1u;
}

/* MQTT PUBLISH 二进制帧内查找 JSON 再解析 serverSendTime */
static uint8_t aliyun_ntp_apply_from_mqtt_chunk(const uint8_t *chunk, uint16_t len)
{
    uint16_t i;

    if(chunk == NULL || len == 0u) {
        return 0u;
    }
    if(aliyun_ntp_apply_from_buf(chunk, len) != 0u) {
        return 1u;
    }
    for(i = 0u; i < len; i++) {
        if(chunk[i] == '{') {
            if(aliyun_ntp_apply_from_buf(chunk + i, (uint16_t)(len - i)) != 0u) {
                return 1u;
            }
        }
    }
    return 0u;
}

#define ALIYUN_NTP_IPD_SUBACK  2u
#define ALIYUN_NTP_IPD_SYNCED  1u

/* 扫描 s_rx_win 中全部 +IPD；SUBACK 与 NTP 分路处理 */
static uint8_t aliyun_ntp_try_all_ipd_in_rx_win(void)
{
    char *hdr;
    char *colon;
    char *next;
    uint16_t len;
    uint16_t hdr_end;
    uint16_t in_win;
    uint16_t i;
    uint8_t chunk[384];

    hdr = s_rx_win;
    while(hdr != NULL && (hdr = strstr(hdr, "+IPD,")) != NULL) {
        colon = strchr(hdr, ':');
        if(colon == NULL) {
            break;
        }
        len = ipd_parse_len_field(hdr, colon);
        if(len == 0u || len > sizeof(chunk)) {
            hdr = colon + 1;
            continue;
        }
        hdr_end = (uint16_t)((colon + 1) - s_rx_win);
        in_win = (s_rx_win_pos > hdr_end) ? (uint16_t)(s_rx_win_pos - hdr_end) : 0u;
        if(in_win < len) {
            return 0u;
        }
        for(i = 0u; i < len; i++) {
            chunk[i] = (uint8_t)s_rx_win[hdr_end + i];
        }
        {
            uint8_t ipd_r = aliyun_ntp_handle_ipd(chunk, len);

            if(ipd_r != 0u) {
                rx_win_consume((uint16_t)(hdr_end + len));
                if(ipd_r == ALIYUN_NTP_IPD_SYNCED) {
                    return 1u;
                }
                hdr = s_rx_win;
                continue;
            }
        }
        next = colon + 1;
        if(next >= &s_rx_win[s_rx_win_pos]) {
            break;
        }
        hdr = next;
    }
    return 0u;
}

static uint8_t aliyun_ntp_try_parse_any(void)
{
#if (MODEM_USE_NATIVE_MQTT != 0)
    wf24_poll_mqtt_urc();
#endif
    if(strstr(s_rx_win, "serverSendTime") != NULL &&
       aliyun_ntp_apply_from_buf((const uint8_t *)s_rx_win, s_rx_win_pos) != 0u) {
        return 1u;
    }
    return aliyun_ntp_try_all_ipd_in_rx_win();
}

static uint8_t aliyun_mqtt_suback_pkt_id_in_buf(const uint8_t *buf, uint16_t len, uint16_t *out_pkt_id)
{
    uint16_t i;
    uint16_t rem;
    uint16_t id;

    if(out_pkt_id != NULL) {
        *out_pkt_id = 0u;
    }
    if(buf == NULL || len < 4u || out_pkt_id == NULL) {
        return 0u;
    }
    /* +IPD 单帧：SUBACK 很短；勿在长 PUBLISH 载荷里扫 0x90 */
    if(len <= 16u) {
        for(i = 0u; (uint16_t)(i + 4u) < len; i++) {
            if(buf[i] != 0x90u) {
                continue;
            }
            rem = (uint16_t)buf[i + 1u];
            if(rem < 2u || (uint16_t)(i + 2u + rem) > len) {
                continue;
            }
            id = (uint16_t)(((uint16_t)buf[i + 2u] << 8) | (uint16_t)buf[i + 3u]);
            *out_pkt_id = id;
            return 1u;
        }
        return 0u;
    }
    if(buf[0] != 0x90u) {
        return 0u;
    }
    rem = (uint16_t)buf[1u];
    if(rem < 2u || (uint16_t)(2u + rem) > len) {
        return 0u;
    }
    id = (uint16_t)(((uint16_t)buf[2u] << 8) | (uint16_t)buf[3u]);
    *out_pkt_id = id;
    return 1u;
}

static uint8_t aliyun_mqtt_suback_in_buf(const uint8_t *buf, uint16_t len)
{
    uint16_t dummy = 0u;
    return aliyun_mqtt_suback_pkt_id_in_buf(buf, len, &dummy);
}

static uint8_t aliyun_mqtt_suback_in_rx(void)
{
    uint16_t dummy = 0u;
    return aliyun_mqtt_suback_pkt_id_in_buf((const uint8_t *)s_rx_win, s_rx_win_pos, &dummy);
}

#if (APP_CLOUD_COMMAND_ENABLE != 0)
static void aliyun_dispatch_mqtt_suback(uint16_t pkt_id)
{
    if(s_ntp_sub_sess != 0u && s_ntp_sub_ok == 0u && pkt_id == s_ntp_sub_pkt_id) {
        s_ntp_sub_ok = 1u;
#if (APP_TIME_TRACE != 0)
        s_ntp_suback_logged = 1u;
        TIME_TRACE_MSG("[TIME] NTP SUBACK ok (pkt)\r\n");
#endif
    }
    if(pkt_id != 0u) {
        app_cloud_command_on_suback_for_pkt(pkt_id);
    }
}
#endif

#if (APP_WIFI_MODEM_WF24 != 0)
static const char *wf24_find_last_cwjap_ok(const char *buf)
{
    const char *p;
    const char *last = NULL;

    if(buf == NULL) {
        return NULL;
    }
    p = buf;
    while((p = strstr(p, "+CWJAP:")) != NULL) {
        if(p[7] == '1' && p[8] == ',') {
            last = p;
        }
        p += 7;
    }
    return last;
}

static uint8_t wf24_cwjap_fail_in_rx(void)
{
    if(strstr(s_rx_win, "+CWJAP:2") != NULL ||
       strstr(s_rx_win, "+CWJAP:3") != NULL ||
       strstr(s_rx_win, "+CWJAP:4") != NULL ||
       strstr(s_rx_win, "FAIL") != NULL ||
       strstr(s_rx_win, "busy p") != NULL ||
       strstr(s_rx_win, "no ap") != NULL ||
       strstr(s_rx_win, "NO AP") != NULL) {
        return 1u;
    }
    return 0u;
}

/* 1=已关联但 DHCP 尚未给出非 0 IP（WF24 常先发 0.0.0.0） */
static uint8_t wf24_cwjap_dhcp_pending(void)
{
    const char *p;
    const char *c1;
    const char *c2;
    const char *ip;

    p = wf24_find_last_cwjap_ok(s_rx_win);
    if(p == NULL) {
        return 0u;
    }
    c1 = strchr(p, ',');
    if(c1 == NULL) {
        return 0u;
    }
    c2 = strchr(c1 + 1u, ',');
    if(c2 == NULL) {
        return 0u;
    }
    ip = c2 + 1u;
    while(*ip == ' ' || *ip == '\t') {
        ip++;
    }
    if(*ip == '\'' || *ip == '\"') {
        ip++;
    }
    return (strncmp(ip, "0.0.0.0", 7u) == 0) ? 1u : 0u;
}

static uint8_t modem_wifi_got_ip_in_rx(void)
{
    const char *p;
    const char *c1;
    const char *c2;
    const char *ip;
    const char *cw;
    uint8_t st = 0u;

    if(strstr(s_rx_win, "WIFI GOT IP") != NULL) {
        return 1u;
    }
    /* WF24 常先发 +CWJAP:1,'ssid',0.0.0.0，DHCP 完成后再发真实 IP；取最后一条。 */
    p = wf24_find_last_cwjap_ok(s_rx_win);
    if(p != NULL) {
        c1 = strchr(p, ',');
        if(c1 != NULL) {
            c2 = strchr(c1 + 1u, ',');
            if(c2 != NULL) {
                ip = c2 + 1u;
                while(*ip == ' ' || *ip == '\t') {
                    ip++;
                }
                if(*ip == '\'' || *ip == '\"') {
                    ip++;
                }
                if(*ip >= '1' && *ip <= '9') {
                    return 1u;
                }
            }
        }
    }
    cw = wf24_find_last_cwstate(s_rx_win);
    if(cw != NULL &&
       wf24_parse_cwstate_line(cw, &st, NULL, 0u) != 0u &&
       st == 2u) {
        return 1u;
    }
    return 0u;
}

static void modem_at_echo_off(void)
{
#if (MODEM_SKIP_ATE0 == 0)
    (void)uart2_send_text("ATE0\r\n");
    (void)uart2_wait_text("OK", 800u);
#endif
}

static uint8_t modem_cwlap_frame_valid(const char *dst, uint16_t pos)
{
    if(dst == NULL || pos < 12u) {
        return 0u;
    }
    if(strstr(dst, "+CWLAP:") == NULL) {
        return 0u;
    }
    if(strstr(dst, "\r\nOK\r\n") != NULL || strstr(dst, "\nOK\r\n") != NULL) {
        return 1u;
    }
    return 0u;
}

static const char *wf24_find_last_cwstate(const char *buf)
{
    const char *p;
    const char *last = NULL;

    if(buf == NULL) {
        return NULL;
    }
    p = buf;
    while((p = strstr(p, "+CWSTATE:")) != NULL) {
        last = p;
        p++;
    }
    return last;
}

static uint8_t wf24_parse_cwstate_line(const char *p, uint8_t *out_st, char *out_ssid, uint16_t ssid_sz)
{
    const char *comma;
    const char *end;
    uint16_t n;
    uint8_t st;

    if(p == NULL || strncmp(p, "+CWSTATE:", 9) != 0) {
        return 0u;
    }
    p += 9u;
    while(*p == ' ') {
        p++;
    }
    if(*p < '0' || *p > '4') {
        return 0u;
    }
    st = (uint8_t)(*p - '0');
    if(p[1] >= '0' && p[1] <= '9') {
        return 0u;
    }
    if(out_st != NULL) {
        *out_st = st;
    }
    if(out_ssid != NULL && ssid_sz > 0u) {
        out_ssid[0] = '\0';
    }
    comma = strchr(p, ',');
    if(comma == NULL || out_ssid == NULL || ssid_sz < 2u) {
        return 1u;
    }
    p = comma + 1u;
    while(*p == ' ') {
        p++;
    }
    if(*p == '\'') {
        p++;
        end = strchr(p, '\'');
    } else if(*p == '"') {
        p++;
        end = strchr(p, '"');
    } else {
        end = strchr(p, '\r');
        if(end == NULL) {
            end = strchr(p, '\n');
        }
    }
    if(end == NULL || end <= p) {
        return 1u;
    }
    n = (uint16_t)(end - p);
    if(n >= ssid_sz) {
        n = (uint16_t)(ssid_sz - 1u);
    }
    memcpy(out_ssid, p, n);
    out_ssid[n] = '\0';
    return 1u;
}

static uint8_t wf24_query_cwstate(uint8_t *out_st, char *out_ssid, uint16_t ssid_sz)
{
    const char *p;

    if(out_st != NULL) {
        *out_st = 0u;
    }
    if(out_ssid != NULL && ssid_sz > 0u) {
        out_ssid[0] = '\0';
    }
    if(!uart2_send_text("AT+CWSTATE?\r\n") ||
       !uart2_wait_text("+CWSTATE:", ALIYUN_WAIT_MID_MS)) {
        return 0u;
    }
    p = wf24_find_last_cwstate(s_rx_win);
    if(p == NULL) {
        return 0u;
    }
    return wf24_parse_cwstate_line(p, out_st, out_ssid, ssid_sz);
}

/** 上电自动连：模组已在记住的 SSID 上则跳过 CWQAP+CWJAP */
static uint8_t wf24_boot_try_reuse_saved_wifi(void)
{
    const char *cfg;
    uint8_t st = 0u;
    char ssid_live[40];

    if(s_user_wifi_join_st != 0u || s_cwjap_resend_only != 0u) {
        return 0u;
    }
    cfg = app_wifi_cfg_get_ssid();
    if(cfg == NULL || cfg[0] == '\0') {
        return 0u;
    }
    if(wf24_query_cwstate(&st, ssid_live, (uint16_t)sizeof(ssid_live)) == 0u) {
        return 0u;
    }
    if(st != 2u || strcmp(cfg, ssid_live) != 0) {
        return 0u;
    }
    strncpy(s_connected_sta_ssid, ssid_live, sizeof(s_connected_sta_ssid) - 1u);
    s_connected_sta_ssid[sizeof(s_connected_sta_ssid) - 1u] = '\0';
    if(aliyun_ensure_sta_ip() == 0u) {
        s_step = ALIYUN_STEP_CIFSR;
        s_cifsr_retry_cnt = 0u;
        return 1u;
    }
    s_wifi_sta_ip_ok = 1u;
    s_wifi_ever_up = 1u;
    s_wifi_got_ip_ms = HAL_GetTick();
    LOG_ESSENTIAL("[WiFi] connected\r\n");
    app_wifi_cfg_clear_reconnect_request();
    cloud_aliyun_at_mqtt_kick_after_wifi();
    s_step = ALIYUN_STEP_WIFI_IDLE;
    return 1u;
}

static uint8_t wf24_mqtt_connected_in_rx(void)
{
    return (strstr(s_rx_win, "+MQTTCONNECTED") != NULL ||
            strstr(s_rx_win, "MQTTCONNECTED:") != NULL) ? 1u : 0u;
}

static uint8_t wf24_rx_mqtt_fatal_error(void)
{
    const char *p;

    if(wf24_mqtt_connected_in_rx() != 0u) {
        return 0u;
    }
    /* 勿把 +MQTTDISCONNECTED 当致命：MQTTCONN 后 WF24 常先清旧会话再 +MQTTCONNECTED */
    p = s_rx_win;
    while((p = strstr(p, "ERROR")) != NULL) {
        if(strncmp(p, "ERROR=104", 9) == 0) {
            p += 9;
            continue;
        }
        /* WF24: ERROR=102 常见于状态切换/忙，后续仍可能正常收到 +MQTTCONNECTED；不要当致命错误。 */
        if(strncmp(p, "ERROR=102", 9) == 0) {
            p += 9;
            continue;
        }
        /* ERROR=203：WiFi URC 未收完时 MQTTCONN 可能先报一次失败，随后仍 +MQTTCONNECTED */
        if(strncmp(p, "ERROR=203", 9) == 0) {
            p += 9;
            continue;
        }
        return 1u;
    }
    return 0u;
}

static const char *wf24_mqtt_st_label(wf24_mqtt_st_t st)
{
    switch(st) {
    case WF24_MQTT_ST_IDLE:   return "IDLE";
    case WF24_MQTT_ST_CLEAN:  return "CLEAN";
    case WF24_MQTT_ST_CFG:    return "CFG";
    case WF24_MQTT_ST_CLIENT: return "CLIENT";
    case WF24_MQTT_ST_USER:   return "USER";
    case WF24_MQTT_ST_PASS:   return "PASS";
    case WF24_MQTT_ST_CONN:   return "CONN";
    case WF24_MQTT_ST_WAIT:   return "WAIT";
    default:                  return "?";
    }
}

static uint8_t wf24_mqtt_publish_raw(const char *topic, const uint8_t *data, uint16_t len)
{
    char cmd[160];
    uint8_t try;

    if(topic == NULL || data == NULL || len == 0u) {
        return 0u;
    }
    for(try = 0u; try < 3u; try++) {
        snprintf(cmd, sizeof(cmd), "AT+MQTTPUBRAW=%s,%u,0,0\r\n", topic, (unsigned)len);
#if (MODEM_USE_NATIVE_MQTT != 0)
        /* 上行 publish 前先把待发下行吃掉，避免 cloud_uart2_rx_clear 抹掉 +MQTTSUBRECV */
        if(s_step == ALIYUN_STEP_ONLINE) {
            wf24_poll_mqtt_urc();
        }
#endif
        cloud_uart2_rx_clear();
        if(uart2_send_text(cmd) && uart2_wait_text(">", ALIYUN_WAIT_MID_MS) &&
           uart2_send_bytes(data, len) &&
           uart2_wait_text("OK", ALIYUN_WAIT_MID_MS)) {
            s_mqtt_tx_fail_streak = 0u;
            aliyun_mqtt_touch_activity();
#if (MODEM_USE_NATIVE_MQTT != 0)
            if(s_step == ALIYUN_STEP_ONLINE) {
                wf24_poll_mqtt_urc();
            }
#endif
            return 1u;
        }
        (void)cloud_uart2_pump_rx(40u);
#if (MODEM_USE_NATIVE_MQTT != 0)
        if(s_step == ALIYUN_STEP_ONLINE) {
            wf24_poll_mqtt_urc();
        }
#endif
    }
    s_mqtt_tx_fail_streak++;
    if(s_mqtt_tx_fail_streak >= 8u) {
        s_mqtt_tx_fail_streak = 0u;
        aliyun_mqtt_schedule_reconnect("tx fail");
    }
    return 0u;
}

static void wf24_dispatch_plain_payload(const char *topic, const uint8_t *data, uint16_t len)
{
    if(data == NULL || len == 0u) {
        return;
    }
    aliyun_mqtt_touch_activity();
    if(topic != NULL &&
       (strstr(topic, "property/post_reply") != NULL ||
        strstr(topic, "thing/event/property/post_reply") != NULL)) {
        if(aliyun_thing_model_code_from_buf((const char *)data, len) == 200) {
            s_prop_post_reply_ok = 1u;
        }
        return;
    }
#if (APP_CLOUD_COMMAND_ENABLE != 0)
    if(app_cloud_command_on_mqtt_plain((const uint8_t *)data, len) != 0u) {
        return;
    }
#endif
    (void)aliyun_ntp_apply_from_mqtt_chunk(data, len);
}

static void wf24_poll_mqtt_urc(void)
{
    char *hdr;
    char *topic_end;
    char *len_end;
    char topic[96];
    unsigned long pay_len;
    uint16_t n;
    uint16_t hdr_end;
    uint16_t hdr_off;
    uint16_t msg_end;
    uint16_t tail_len;

    if(s_wf24_poll_busy != 0u) {
        return;
    }
    s_wf24_poll_busy = 1u;
    hdr = s_rx_win;
    while(hdr != NULL && (hdr = strstr(hdr, "+MQTTSUBRECV:")) != NULL) {
        const char *tp = hdr + 13u;
        const char *topic_start;
        const char *topic_close;

        topic_start = tp;
        if(*tp == '"') {
            topic_start = tp + 1u;
            topic_close = strchr(topic_start, '"');
            if(topic_close == NULL) {
                break;
            }
            topic_end = strchr(topic_close + 1u, ',');
        } else {
            topic_end = strchr(tp, ',');
        }
        if(topic_end == NULL) {
            break;
        }
        n = (uint16_t)(topic_end - topic_start);
        if(n == 0u || n >= sizeof(topic)) {
            hdr = topic_end + 1;
            continue;
        }
        memcpy(topic, topic_start, n);
        topic[n] = '\0';
        len_end = strchr(topic_end + 1, ',');
        if(len_end == NULL) {
            break;
        }
        pay_len = strtoul(topic_end + 1, NULL, 10);
        if(pay_len == 0u || pay_len > 512u) {
            hdr = len_end + 1;
            continue;
        }
        hdr_end = (uint16_t)((len_end + 1) - s_rx_win);
        if(s_rx_win_pos < (uint16_t)(hdr_end + pay_len)) {
            break;
        }
        wf24_dispatch_plain_payload(topic, (const uint8_t *)(s_rx_win + hdr_end), (uint16_t)pay_len);
        hdr_off = (uint16_t)(hdr - s_rx_win);
        msg_end = (uint16_t)(hdr_end + pay_len);
        tail_len = (uint16_t)(s_rx_win_pos - msg_end);
        memmove(s_rx_win + hdr_off, s_rx_win + msg_end, tail_len);
        s_rx_win_pos = (uint16_t)(hdr_off + tail_len);
        s_rx_win[s_rx_win_pos] = '\0';
        hdr = s_rx_win + hdr_off;
    }
    s_wf24_poll_busy = 0u;
}

static void wf24_mqtt_reset_state(void)
{
    s_wf24_mqtt_st = WF24_MQTT_ST_IDLE;
    s_wf24_mqtt_wait_ms = 0u;
}

static void wf24_mqtt_prepare_uart(void)
{
    uint32_t t0;
    uint32_t quiet_ms = 0u;
    uint8_t ch = 0u;

    cloud_uart2_hw_drain_ms(150u);
    t0 = HAL_GetTick();
    while((HAL_GetTick() - t0) < 900u) {
        if(uart2_read_byte_timeout(&ch, 8u) != 0) {
            quiet_ms = 0u;
        } else {
            quiet_ms += 8u;
            if(quiet_ms >= 120u) {
                break;
            }
        }
        uart2_coop_yield();
    }
    cloud_uart2_drain_until_idle(120u, 30u);
    cloud_uart2_rx_clear();
}

static uint8_t wf24_mqtt_connect_step(char *cmd, size_t cmd_sz)
{
    static wf24_mqtt_st_t s_wf24_mqtt_log_st = WF24_MQTT_ST_IDLE;

    if(s_wf24_mqtt_st != s_wf24_mqtt_log_st) {
        printf("[ALIYUN] WF24 MQTT step %s\r\n", wf24_mqtt_st_label(s_wf24_mqtt_st));
        s_wf24_mqtt_log_st = s_wf24_mqtt_st;
    }
    switch(s_wf24_mqtt_st) {
    case WF24_MQTT_ST_IDLE:
        wf24_mqtt_prepare_uart();
        s_wf24_mqtt_st = WF24_MQTT_ST_CLEAN;
        return 0u;
    case WF24_MQTT_ST_CLEAN:
        (void)uart2_at_cmd_wait_ok("AT+MQTTCLEAN\r\n", ALIYUN_WAIT_MID_MS);
        cloud_uart2_hw_drain_ms(80u);
        cloud_uart2_rx_clear();
        s_wf24_mqtt_st = WF24_MQTT_ST_CFG;
        return 0u;
    case WF24_MQTT_ST_CFG:
        snprintf(cmd, cmd_sz, "AT+MQTTCONNCFG=120,1,%s,offline,0,0\r\n",
                 s_mqtt_will_topic);
        if(!uart2_at_cmd_wait_ok(cmd, ALIYUN_WAIT_MID_MS)) {
            return 2u;
        }
        s_wf24_mqtt_st = WF24_MQTT_ST_CLIENT;
        return 0u;
    case WF24_MQTT_ST_CLIENT:
        snprintf(cmd, cmd_sz, "AT+MQTTLONGCLIENTID=%s\r\n", s_client_id);
        if(!uart2_at_cmd_wait_ok(cmd, ALIYUN_WAIT_MID_MS)) {
            return 2u;
        }
        s_wf24_mqtt_st = WF24_MQTT_ST_USER;
        return 0u;
    case WF24_MQTT_ST_USER:
        snprintf(cmd, cmd_sz, "AT+MQTTLONGUSERNAME=%s\r\n", s_user_name);
        if(!uart2_at_cmd_wait_ok(cmd, ALIYUN_WAIT_MID_MS)) {
            return 2u;
        }
        s_wf24_mqtt_st = WF24_MQTT_ST_PASS;
        return 0u;
    case WF24_MQTT_ST_PASS:
        snprintf(cmd, cmd_sz, "AT+MQTTLONGPASSWORD=%s\r\n", s_password);
        if(!uart2_at_cmd_wait_ok(cmd, ALIYUN_WAIT_MID_MS)) {
            return 2u;
        }
        s_wf24_mqtt_st = WF24_MQTT_ST_CONN;
        return 0u;
    case WF24_MQTT_ST_CONN:
        cloud_uart2_rx_clear();
        snprintf(cmd, cmd_sz, "AT+MQTTCONN=%s,1883,1\r\n", s_host);
        if(!uart2_send_text(cmd)) {
            return 2u;
        }
        (void)uart2_wait_text("OK", ALIYUN_WAIT_MID_MS);
        s_wf24_mqtt_st = WF24_MQTT_ST_WAIT;
        s_wf24_mqtt_wait_ms = HAL_GetTick();
        return 0u;
    case WF24_MQTT_ST_WAIT:
        wf24_poll_mqtt_urc();
        (void)cloud_uart2_pump_rx(80u);
        if(wf24_mqtt_connected_in_rx() != 0u) {
            wf24_mqtt_reset_state();
            return 1u;
        }
        {
            uint32_t wait_lim = (s_mqtt_fast_join != 0u) ?
                                WF24_MQTT_WAIT_FAST_MS : WF24_MQTT_WAIT_MS;
            uint32_t fatal_ms = (s_mqtt_fast_join != 0u) ? 4500u : 8000u;

            if(s_wf24_mqtt_wait_ms != 0u &&
               (HAL_GetTick() - s_wf24_mqtt_wait_ms) > wait_lim) {
                if(wf24_mqtt_connected_in_rx() == 0u) {
                    printf("[ALIYUN] WF24 MQTT WAIT timeout\r\n");
                    uart2_log_rx_snapshot("WF24 MQTT WAIT to");
                    wf24_mqtt_reset_state();
                    return 2u;
                }
            }
            if(s_wf24_mqtt_wait_ms != 0u &&
               (HAL_GetTick() - s_wf24_mqtt_wait_ms) > fatal_ms &&
               wf24_rx_mqtt_fatal_error() != 0u) {
                printf("[ALIYUN] WF24 MQTT WAIT fail\r\n");
                uart2_log_rx_snapshot("WF24 MQTT WAIT fail");
                wf24_mqtt_reset_state();
                return 2u;
            }
        }
        if(uart2_wait_text("MQTTCONNECTED", 1500u) != 0) {
            if(wf24_mqtt_connected_in_rx() != 0u) {
                wf24_mqtt_reset_state();
                return 1u;
            }
        }
        return 0u;
    default:
        wf24_mqtt_reset_state();
        return 2u;
    }
}

static uint8_t wf24_mqtt_subscribe_topic(const char *topic, uint16_t *out_pkt_id)
{
    char cmd[128];
    uint16_t pkt_id;

    if(topic == NULL || topic[0] == '\0') {
        return 0u;
    }
    pkt_id = s_mqtt_pkt_id++;
    if(out_pkt_id != NULL) {
        *out_pkt_id = pkt_id;
    }
    snprintf(cmd, sizeof(cmd), "AT+MQTTSUB=%s,0\r\n", topic);
    if(uart2_at_cmd_wait_ok(cmd, ALIYUN_WAIT_MID_MS) == 0u &&
       strstr(s_rx_win, "ALREADY SUBSCRIBE") == NULL) {
        return 0u;
    }
#if (APP_CLOUD_COMMAND_ENABLE != 0)
    aliyun_dispatch_mqtt_suback(pkt_id);
#else
    if(s_ntp_sub_sess != 0u && s_ntp_sub_ok == 0u) {
        s_ntp_sub_ok = 1u;
    }
#endif
    return 1u;
}
#endif /* APP_WIFI_MODEM_WF24 */

#if (APP_WIFI_MODEM_WF24 == 0)
static uint8_t modem_wifi_got_ip_in_rx(void)
{
    return (strstr(s_rx_win, "WIFI GOT IP") != NULL) ? 1u : 0u;
}
#endif

static int aliyun_thing_model_code_from_buf(const char *buf, uint16_t len);

/* 分类处理 +IPD 负载：SUBACK / PINGRESP / NTP PUBLISH */
static uint8_t aliyun_ntp_handle_ipd(const uint8_t *buf, uint16_t len)
{
#if (APP_TIME_TRACE != 0)
    char line[56];
#endif

    if(buf == NULL || len == 0u) {
        return 0u;
    }
    aliyun_mqtt_touch_activity();
    /* 须在 SUBACK 扫描之前：长 PUBLISH 帧内可能含 0x90，误当 SUBACK 会吞掉 post_reply */
    if(strstr((const char *)buf, "property/post_reply") != NULL) {
        if(aliyun_thing_model_code_from_buf((const char *)buf, len) == 200) {
            s_prop_post_reply_ok = 1u;
        }
        return ALIYUN_NTP_IPD_SUBACK;
    }
    {
        uint16_t suback_pkt = 0u;
        if(aliyun_mqtt_suback_pkt_id_in_buf(buf, len, &suback_pkt) != 0u) {
#if (APP_CLOUD_COMMAND_ENABLE != 0)
            aliyun_dispatch_mqtt_suback(suback_pkt);
#else
            if(s_ntp_sub_sess != 0u && s_ntp_sub_ok == 0u && suback_pkt == s_ntp_sub_pkt_id) {
                s_ntp_sub_ok = 1u;
            }
#endif
#if (APP_TIME_TRACE != 0)
            (void)snprintf(line, sizeof(line), "[TIME] NTP SUBACK ok len=%u pkt=%u\r\n",
                            (unsigned)len, (unsigned)suback_pkt);
            TIME_TRACE_MSG(line);
            s_ntp_suback_logged = 1u;
#endif
            return ALIYUN_NTP_IPD_SUBACK;
        }
    }
    /* 0x20=CONNACK 残留，0xD0=PINGRESP；勿当 SUBACK/NTP */
    if(len >= 2u && (buf[0] == 0xD0u || buf[0] == 0x20u)) {
        return ALIYUN_NTP_IPD_SUBACK;
    }
#if (APP_CLOUD_COMMAND_ENABLE != 0)
    if(len >= 2u && (buf[0] == 0x30u || buf[0] == 0x31u)) {
#if (APP_CLOUD_TRACE != 0)
        {
            char tr[52];
            (void)snprintf(tr, sizeof(tr), "[CLOUD][CMD] ipd publish len=%u\r\n", (unsigned)len);
            cloud_trace_tx(tr);
        }
#endif
        if(app_cloud_command_on_mqtt_publish(buf, len) != 0u) {
            return ALIYUN_NTP_IPD_SUBACK;
        }
    }
#endif
    if(aliyun_ntp_apply_from_mqtt_chunk(buf, len) != 0u) {
        return ALIYUN_NTP_IPD_SYNCED;
    }
#if (APP_TIME_TRACE != 0)
    if(len <= 16u) {
        (void)snprintf(line, sizeof(line),
                       "[TIME] NTP ipd other len=%u b0=%02X\r\n",
                       (unsigned)len, (unsigned)buf[0]);
        TIME_TRACE_MSG(line);
    }
#endif
    return 0u;
}

static void aliyun_ntp_pump_after_recv_hint(void)
{
    if(strstr(s_rx_win, "Recv") == NULL) {
        return;
    }
    uart2_pump_rx_core(120u, 1u);
    (void)aliyun_ntp_try_parse_any();
}

static void aliyun_ntp_poll_ipd_once(uint8_t *rx, uint16_t rx_sz, uint16_t *out_len)
{
    if(out_len != NULL) {
        *out_len = 0u;
    }
    if(uart2_recv_ipd_payload(rx, rx_sz, out_len, 80u) != 0 && out_len != NULL && *out_len > 0u) {
        if(aliyun_ntp_handle_ipd(rx, *out_len) == ALIYUN_NTP_IPD_SYNCED) {
            return;
        }
    }
    (void)aliyun_ntp_try_parse_any();
    {
        uint16_t suback_pkt = 0u;
        if(aliyun_mqtt_suback_pkt_id_in_buf((const uint8_t *)s_rx_win, s_rx_win_pos, &suback_pkt) != 0u) {
#if (APP_CLOUD_COMMAND_ENABLE != 0)
            aliyun_dispatch_mqtt_suback(suback_pkt);
#else
            if(s_ntp_sub_sess != 0u && s_ntp_sub_ok == 0u && suback_pkt == s_ntp_sub_pkt_id) {
                s_ntp_sub_ok = 1u;
            }
#endif
#if (APP_TIME_TRACE != 0)
            s_ntp_suback_logged = 1u;
            TIME_TRACE_MSG("[TIME] NTP SUBACK ok (rx_win)\r\n");
#endif
        }
    }
}

static void aliyun_ntp_poll_suback(uint8_t *rx, uint16_t rx_sz, uint8_t rounds)
{
    uint8_t n;
    uint16_t rx_len;

    for(n = 0u; n < rounds && s_ntp_sub_ok == 0u; n++) {
        rx_len = 0u;
        uart2_pump_rx_core(40u, 1u);
        aliyun_ntp_pump_after_recv_hint();
        aliyun_ntp_poll_ipd_once(rx, rx_sz, &rx_len);
    }
}

/* 超时诊断：看 UART 是否收到 +IPD / NTP JSON */
static void aliyun_ntp_rx_diag_trace(void)
{
    if(strstr(s_rx_win, "serverSendTime") != NULL) {
        TIME_TRACE_MSG("[TIME] rx has NTP JSON unparsed\r\n");
    } else if(strstr(s_rx_win, "+IPD") != NULL) {
        TIME_TRACE_MSG("[TIME] rx +IPD no NTP JSON\r\n");
    } else if(s_rx_win_pos > 0u) {
        TIME_TRACE_MSG("[TIME] rx data no +IPD\r\n");
    } else {
        TIME_TRACE_MSG("[TIME] rx empty (no +IPD)\r\n");
    }
}

#if (APP_TIME_TRACE != 0)
static void aliyun_ntp_rx_tail_trace(void)
{
    uint16_t i;
    uint16_t n = 0u;
    uint16_t start;
    char tb[40];

    if(s_rx_win_pos == 0u) {
        return;
    }
    start = (s_rx_win_pos > 32u) ? (uint16_t)(s_rx_win_pos - 32u) : 0u;
    TIME_TRACE_MSG("[TIME] NTP rx tail:");
    for(i = start; i < s_rx_win_pos && n < 32u; i++) {
        char c = s_rx_win[i];
        if(c == '\r' || c == '\n') {
            break;
        }
        tb[n++] = (c >= 0x20 && c < 0x7f) ? c : '.';
    }
    tb[n++] = '\r';
    tb[n++] = '\n';
    tb[n] = '\0';
    time_trace_tx(tb);
}

static void aliyun_ntp_rx_diag_detail(const char *tag, uint32_t pend_age_ms)
{
    char line[112];
    uint8_t has_suback = aliyun_mqtt_suback_in_rx();
    uint8_t has_json = (strstr(s_rx_win, "serverSendTime") != NULL) ? 1u : 0u;
    uint8_t has_ipd = (strstr(s_rx_win, "+IPD") != NULL) ? 1u : 0u;
    uint8_t has_disc = 0u;
    uint8_t has_err = 0u;

    if(strstr(s_rx_win, "CLOSED") != NULL || strstr(s_rx_win, "DISCONNECT") != NULL) {
        has_disc = 1u;
    }
    if(strstr(s_rx_win, "ERROR") != NULL || strstr(s_rx_win, "FAIL") != NULL) {
        has_err = 1u;
    }
    (void)snprintf(line, sizeof(line),
                   "[TIME] NTP diag %s win=%u suback=%u ipd=%u json=%u disc=%u err=%u pend=%u age=%lu\r\n",
                   (tag != NULL) ? tag : "?",
                   (unsigned)s_rx_win_pos,
                   (unsigned)has_suback,
                   (unsigned)has_ipd,
                   (unsigned)has_json,
                   (unsigned)has_disc,
                   (unsigned)has_err,
                   (unsigned)s_ntp_req_pending,
                   (unsigned long)pend_age_ms);
    TIME_TRACE_MSG(line);
    aliyun_ntp_rx_diag_trace();
    if(has_json == 0u && s_rx_win_pos > 0u) {
        aliyun_ntp_rx_tail_trace();
    }
}
#endif

static void aliyun_ntp_format_req_payload(char *buf, size_t bufsz, uint32_t tick_ms)
{
    unsigned long sec;
    unsigned long ms3 = (unsigned long)(tick_ms % 1000u);

    if(buf == NULL || bufsz < 32u) {
        return;
    }
    if(app_wall_clock_valid() != 0u) {
        sec = (unsigned long)app_wall_clock_epoch_sec();
    } else {
        sec = NTP_DEVTIME_SEC_BASE + (unsigned long)(tick_ms / 1000u);
    }
    (void)snprintf(buf, bufsz,
                   "{\"deviceSendTime\":\"%lu%03lu\"}",
                   sec, ms3);
}

static void aliyun_ntp_fallback_to_esp_sntp(void)
{
#if (APP_ALIYUN_SNTP_ENABLE == 1)
#if (APP_ALIYUN_SNTP_BEFORE_MQTT != 0)
    TIME_TRACE_MSG("[TIME] MQTT NTP fail -> ESP SNTP\r\n");
    aliyun_mqtt_close_link0();
    s_ntp_sub_sess = 0u;
    s_ntp_sub_ok = 0u;
    s_ntp_await_until_ms = 0u;
    s_ntp_next_request_ms = 0u;
    s_step = ALIYUN_STEP_SNTP_CFG;
    s_step_ms = HAL_GetTick();
#else
    /* SNTP_BEFORE_MQTT=0：保持 MQTT 在线，重置 NTP 状态下轮重试 */
    TIME_TRACE_MSG("[TIME] MQTT NTP retry later\r\n");
    s_ntp_sub_sess = 0u;
    s_ntp_sub_ok = 0u;
    s_ntp_await_until_ms = 0u;
    s_ntp_next_request_ms = 0u;
    s_ntp_mqtt_fail_cnt = 0u;
#endif
#else
    (void)0;
#endif
}

static void aliyun_ntp_reset_bridge_wait(void)
{
}

static uint8_t aliyun_ntp_may_start_subscribe(uint32_t now)
{
    (void)now;
    return 1u;
}

/* MQTT 在线后非阻塞 NTP：subscribe 一次 → 周期 publish → 每 tick 轮询 +IPD */
static void aliyun_ntp_try_sync_online(void)
{
    uint8_t pkt[256];
    uint16_t pkt_len;
    uint8_t rx[384];
    uint16_t rx_len = 0u;
    char req_payload[56];
    uint32_t now = HAL_GetTick();

    if(s_prop_post_awaiting != 0u) {
        return;
    }
    if(s_ntp_synced != 0u || app_wall_clock_valid() != 0u) {
        s_ntp_synced = 1u;
        return;
    }
#if (APP_WIFI_MODEM_WF24 != 0)
    if(s_sntp_synced != 0u) {
        s_ntp_synced = 1u;
        return;
    }
#endif
    if(s_ntp_connect_tried == 0u || s_step != ALIYUN_STEP_ONLINE) {
        return;
    }

    if(s_ntp_req_pending != 0u &&
       (int32_t)(now - (int32_t)s_ntp_req_sent_ms) >= (int32_t)ALIYUN_MQTT_NTP_REQ_PENDING_MS) {
#if (APP_TIME_TRACE != 0)
        aliyun_ntp_rx_diag_detail("pend_to", now - s_ntp_req_sent_ms);
#endif
        s_ntp_req_pending = 0u;
    }

    if(s_ntp_req_pending != 0u) {
#if (APP_TIME_TRACE != 0)
        if(s_ntp_pend_diag_ms == 0u ||
           (now - s_ntp_pend_diag_ms) >= 4000u) {
            s_ntp_pend_diag_ms = now;
            aliyun_ntp_rx_diag_detail("pend_wait", now - s_ntp_req_sent_ms);
        }
#endif
        uart2_pump_rx_core(60u, 1u);
        aliyun_ntp_pump_after_recv_hint();
        (void)aliyun_ntp_try_parse_any();
    } else {
#if (APP_TIME_TRACE != 0)
        s_ntp_pend_diag_ms = 0u;
#endif
    }

    uart2_pump_rx_core(12u, 1u);
    aliyun_ntp_pump_after_recv_hint();
    if(aliyun_ntp_try_parse_any() != 0u) {
        s_ntp_mqtt_fail_cnt = 0u;
        s_ntp_req_pending = 0u;
        return;
    }

    if(s_ntp_sub_sess == 0u) {
        if(aliyun_ntp_may_start_subscribe(now) == 0u) {
            return;
        }
        LOG_ESSENTIAL("[TIME] NTP start\r\n");
        /* 清 MQTT 建连后残留的 CONNACK(0x20) 等控制帧，避免误判为 SUBACK */
        aliyun_ntp_poll_suback(rx, (uint16_t)sizeof(rx), 4u);
        if(s_ntp_synced != 0u) {
            s_ntp_req_pending = 0u;
            return;
        }
        s_ntp_sub_pkt_id = s_mqtt_pkt_id;
#if (MODEM_USE_NATIVE_MQTT != 0)
        if(wf24_mqtt_subscribe_topic(s_ntp_resp_topic, &s_ntp_sub_pkt_id) == 0u) {
            TIME_TRACE_MSG("[TIME] NTP subscribe WF24 fail\r\n");
#if (APP_TIME_TRACE != 0)
            aliyun_ntp_rx_diag_detail("sub_fail", 0u);
#endif
            return;
        }
        s_ntp_sub_ok = 1u;
#else
        pkt_len = build_mqtt_subscribe_qos0(pkt, sizeof(pkt), s_mqtt_pkt_id++, s_ntp_resp_topic);
        if(pkt_len == 0u) {
            TIME_TRACE_MSG("[TIME] NTP subscribe build fail\r\n");
            return;
        }
        if(mqtt_send_packet(pkt, pkt_len) == 0u) {
            TIME_TRACE_MSG("[TIME] NTP subscribe CIPSEND fail\r\n");
#if (APP_TIME_TRACE != 0)
            aliyun_ntp_rx_diag_detail("sub_fail", 0u);
#endif
            return;
        }
        s_ntp_sub_ok = 0u;
#endif
        s_ntp_sub_sess = 1u;
        s_ntp_sub_ms = now;
#if (APP_TIME_TRACE != 0)
        s_ntp_suback_logged = 0u;
#endif
        TIME_TRACE_MSG("[TIME] NTP subscribe sent\r\n");
        aliyun_ntp_poll_suback(rx, (uint16_t)sizeof(rx), 6u);
        if(s_ntp_synced != 0u) {
            s_ntp_req_pending = 0u;
            return;
        }
    }

    if(s_ntp_sub_sess != 0u && s_ntp_sub_ok == 0u) {
        aliyun_ntp_poll_suback(rx, (uint16_t)sizeof(rx), 3u);
        if(s_ntp_synced != 0u) {
            s_ntp_req_pending = 0u;
            return;
        }
        if((int32_t)(now - (int32_t)s_ntp_sub_ms) >= 5000) {
#if (APP_TIME_TRACE != 0)
            if(s_ntp_suback_logged == 0u) {
                s_ntp_suback_logged = 1u;
                TIME_TRACE_MSG("[TIME] NTP SUBACK missing\r\n");
                aliyun_ntp_rx_diag_detail("no_suback", 0u);
            }
#endif
        }
    }

    if(s_ntp_sub_sess != 0u && s_ntp_sub_ok != 0u && s_ntp_req_pending == 0u) {
        uint8_t may_pub = 0u;

        if(s_ntp_next_request_ms == 0u) {
            if((int32_t)(now - (int32_t)s_mqtt_online_ms) >= (int32_t)ALIYUN_MQTT_NTP_FIRST_DELAY_MS) {
                may_pub = 1u;
            }
#if (APP_TIME_TRACE != 0)
            else if(s_ntp_next_request_ms == 0u && s_ntp_suback_logged != 0u) {
                static uint32_t s_ntp_wait_first_log_ms;
                if(s_ntp_wait_first_log_ms == 0u ||
                   (now - s_ntp_wait_first_log_ms) >= 2000u) {
                    char line[72];
                    s_ntp_wait_first_log_ms = now;
                    (void)snprintf(line, sizeof(line),
                                   "[TIME] NTP wait first pub %lums/%u\r\n",
                                   (unsigned long)(now - s_mqtt_online_ms),
                                   (unsigned)ALIYUN_MQTT_NTP_FIRST_DELAY_MS);
                    TIME_TRACE_MSG(line);
                }
            }
#endif
        } else if((int32_t)(now - (int32_t)s_ntp_next_request_ms) >=
                  (int32_t)ALIYUN_MQTT_NTP_RETRY_ONLINE_MS) {
            may_pub = 1u;
        }
        if(may_pub != 0u) {
            char plog[64];

            s_ntp_req_dev_ms = now;
            aliyun_ntp_format_req_payload(req_payload, sizeof(req_payload), now);
            pkt_len = build_mqtt_publish_qos0(pkt, sizeof(pkt), s_ntp_req_topic, req_payload);
            s_ntp_next_request_ms = now;
            if(pkt_len == 0u) {
                TIME_TRACE_MSG("[TIME] NTP pub build fail\r\n");
            } else {
#if (APP_TIME_TRACE != 0)
                (void)snprintf(plog, sizeof(plog), "[TIME] NTP pub %s\r\n", req_payload);
                TIME_TRACE_MSG(plog);
#endif
#if (MODEM_USE_NATIVE_MQTT != 0)
                if(wf24_mqtt_publish_raw(s_ntp_req_topic,
                                         (const uint8_t *)req_payload,
                                         (uint16_t)strlen(req_payload)) != 0u) {
#else
                if(mqtt_send_packet(pkt, pkt_len) != 0u) {
#endif
                    s_ntp_req_pending = 1u;
                    s_ntp_req_sent_ms = now;
                    TIME_TRACE_MSG("[TIME] NTP request sent\r\n");
                    uart2_pump_rx_core(80u, 1u);
                    if(aliyun_ntp_try_parse_any() != 0u) {
                        s_ntp_mqtt_fail_cnt = 0u;
                        s_ntp_req_pending = 0u;
                        return;
                    }
                    if(uart2_recv_ipd_payload(rx, sizeof(rx), &rx_len, 60u) != 0) {
#if (APP_TIME_TRACE != 0)
                        {
                            char ilog[48];
                            (void)snprintf(ilog, sizeof(ilog),
                                           "[TIME] NTP ipd after pub len=%u\r\n",
                                           (unsigned)rx_len);
                            TIME_TRACE_MSG(ilog);
                        }
#endif
                        if(aliyun_ntp_handle_ipd(rx, rx_len) == ALIYUN_NTP_IPD_SYNCED) {
                            s_ntp_req_pending = 0u;
                            return;
                        }
                    }
                    aliyun_ntp_pump_after_recv_hint();
                    (void)aliyun_ntp_try_parse_any();
                    if(s_ntp_synced != 0u) {
                        s_ntp_req_pending = 0u;
                        return;
                    }
                } else {
                    TIME_TRACE_MSG("[TIME] NTP pub CIPSEND fail\r\n");
#if (APP_TIME_TRACE != 0)
                    aliyun_ntp_rx_diag_detail("pub_fail", 0u);
#endif
                }
            }
        }
    }

    if(s_ntp_sub_sess != 0u) {
        rx_len = 0u;
        aliyun_ntp_poll_ipd_once(rx, (uint16_t)sizeof(rx), &rx_len);
#if (APP_TIME_TRACE != 0)
        if(rx_len > 0u) {
            char ilog[48];
            (void)snprintf(ilog, sizeof(ilog),
                           "[TIME] NTP ipd poll len=%u\r\n",
                           (unsigned)rx_len);
            TIME_TRACE_MSG(ilog);
        }
#endif
        if(s_ntp_synced != 0u) {
            s_ntp_req_pending = 0u;
            return;
        }
    }
    if(s_ntp_synced != 0u) {
        s_ntp_req_pending = 0u;
    }
}
#endif

static void aliyun_prepare_params(void)
{
    snprintf(s_host, sizeof(s_host), "%s.iot-as-mqtt.%s.aliyuncs.com",
             APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_REGION);
    snprintf(s_client_id, sizeof(s_client_id), "lvglf407|securemode=3,signmethod=hmacsha1|");
    snprintf(s_user_name, sizeof(s_user_name), "%s&%s",
             APP_ALIYUN_DEVICE_NAME, APP_ALIYUN_PRODUCT_KEY);
    snprintf(s_password, sizeof(s_password), "%s", APP_ALIYUN_MQTT_PASSWORD);
    snprintf(s_prop_post_topic, sizeof(s_prop_post_topic), "/sys/%s/%s/thing/event/property/post",
             APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME);
    snprintf(s_prop_reply_topic, sizeof(s_prop_reply_topic),
             "/sys/%s/%s/thing/event/property/post_reply",
             APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME);
    snprintf(s_ntp_req_topic, sizeof(s_ntp_req_topic), "/ext/ntp/%s/%s/request",
             APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME);
    snprintf(s_ntp_resp_topic, sizeof(s_ntp_resp_topic), "/ext/ntp/%s/%s/response",
             APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME);
    snprintf(s_mqtt_will_topic, sizeof(s_mqtt_will_topic), "/%s/%s%s",
             APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
             APP_BRIDGE_TOPIC_TERMINAL_PUSH);
    (void)snprintf(s_mqtt_will_payload, sizeof(s_mqtt_will_payload),
                   "{\"cmd\":\"presence\",\"state\":\"offline\"}");
    s_mqtt_connect_pkt_len = build_mqtt_connect(s_mqtt_connect_pkt, sizeof(s_mqtt_connect_pkt),
                                                s_client_id, s_user_name, s_password,
                                                s_mqtt_will_topic, s_mqtt_will_payload);
}

void cloud_aliyun_at_init(void)
{
    uart2_mutex_init_once();
#if (APP_ALIYUN_AT_ENABLE == 1)
    uint8_t boot_rx_empty = 0u;

    esp_rst_gpio_init_once();
    esp_rst_set(0u);
    HAL_Delay(100u);
    esp_rst_set(1u);
#if (APP_WIFI_MODEM_WF24 != 0)
    HAL_Delay(1800u);
#else
    HAL_Delay(2500u);
#endif
    uart2_init_once();
    s_rx_win_pos = 0u;
    s_rx_win[0] = '\0';
#if (APP_WIFI_MODEM_WF24 != 0)
    uart2_drain_rx_ms(600u);
#else
    uart2_drain_rx_ms(1500u);
#endif
    if(s_rx_win_pos == 0u) {
        boot_rx_empty = 1u;
    }
#if (APP_HOST_SILENCE_ALIYUN_TERMINAL == 0)
    uart2_log_rx_snapshot("after ESP boot");
#endif
    aliyun_prepare_params();
    /* 勿停在 IDLE（switch 无 IDLE 分支会空转）；modem 已探活则直接进 WiFi 状态机 */
    s_step = (s_modem_at_ok != 0u) ? ALIYUN_STEP_WIFI_IDLE : ALIYUN_STEP_AT;
    s_wifi_ever_up = 0u;
    s_wifi_sta_ip_ok = 0u;
    s_wf24_cwstate_query_ms = 0u;
    s_step_ms = HAL_GetTick();
    s_last_step_log = ALIYUN_STEP_IDLE;
    s_at_fail_cnt = 0u;
    s_cipmode_fail_cnt = 0u;
    s_cipmux1_ok = 0u;
    s_mqtt_fast_join = 0u;
    s_cipsend_retry_cnt = 0u;
    s_last_ping_ms = HAL_GetTick();
#if (APP_ALIYUN_SNTP_ENABLE == 1)
    s_sntp_synced = 0u;
    s_sntp_give_up = 0u;
    s_sntp_retry_online_ms = HAL_GetTick();
    s_http_date_tried = 0u;
#endif
    s_ntp_synced = 0u;
    s_ntp_mqtt_fail_cnt = 0u;
    s_ntp_next_request_ms = 0u;
    s_mqtt_pkt_id = 1u;
    s_modem_at_ok = 0u;
    memset(s_rx_win, 0, sizeof(s_rx_win));
    s_rx_win_pos = 0u;
#if (APP_WIFI_MODEM_WF24 != 0)
    if(wf24_modem_at_probe(2u, 300u, 1500u)) {
#else
    if(uart2_send_text("AT\r\n") && uart2_wait_text("OK", 2500u)) {
#endif
        modem_at_echo_off();
        s_modem_at_ok = 1u;
        printf("[ALIYUN] init: modem AT OK modem_ready=1\r\n");
#if (APP_WIFI_MODEM_WF24 != 0)
    } else if(boot_rx_empty != 0u) {
        printf("[ALIYUN] init: zero RX -> modem KEY reset\r\n");
        if(wf24_modem_key_recover("init recover") != 0u) {
            s_modem_at_ok = 1u;
        } else {
            printf("[ALIYUN] init: still no AT (WF24 VCC/GND? PA2/PA3? PE12 KEY?)\r\n");
        }
    } else {
        printf("[ALIYUN] init: modem AT probe fail (background retry)\r\n");
    }
#else
    } else if(boot_rx_empty != 0u) {
        printf("[ALIYUN] init: zero RX -> modem KEY reset (WiFi CWLAP path)\r\n");
        cloud_uart2_esp_hw_reset(2500u);
        cloud_uart2_flush_rx();
        (void)cloud_uart2_wait_esp_ready(6000u);
        if(cloud_uart2_try_modem_ready() != 0u) {
            modem_at_echo_off();
            s_modem_at_ok = 1u;
            printf("[ALIYUN] init: modem AT OK after KEY reset\r\n");
        } else {
            uart2_log_rx_snapshot("init recover fail");
            printf("[ALIYUN] init: still no AT (WF24 VCC/GND? PA2/PA3? PE12 KEY?)\r\n");
        }
    } else {
        printf("[ALIYUN] init: modem AT probe fail (retry on WiFi page)\r\n");
    }
#endif
    printf("[ALIYUN] mqtt host=%s user=%s\r\n", s_host, s_user_name);
    printf("[ALIYUN] wifi SSID=%s (app_wifi_cfg)\r\n", app_wifi_cfg_get_ssid());
    printf("[ALIYUN] uart2 baud=%lu\r\n", (unsigned long)s_uart2_baud);
#if (APP_WIFI_MODEM_WF24 != 0)
    app_wifi_cfg_mark_reconnect_if_saved();
    printf("[ALIYUN] WF24 boot: reconnect if saved SSID\r\n");
#endif
    if(s_modem_at_ok != 0u) {
        printf("[ALIYUN] cloud poll: modem ready\r\n");
    } else {
        printf("[ALIYUN] cloud poll: modem dead, background AT retry\r\n");
    }
#endif
}

uint8_t cloud_aliyun_at_wifi_was_up(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return s_wifi_ever_up;
#else
    return 0u;
#endif
}

uint8_t cloud_aliyun_at_poll_allowed(uint8_t on_wifi_scr)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    if(on_wifi_scr != 0u) {
        return 1u;
    }
    if(s_wifi_ever_up != 0u) {
        return 1u;
    }
    if(s_user_wifi_join_st == 1u) {
        return 1u;
    }
    /* MQTT 在线或建链中：离 WiFi 页也要收 NTP +IPD */
    if(s_step >= ALIYUN_STEP_CIFSR && s_step <= ALIYUN_STEP_ONLINE) {
        return 1u;
    }
    /* 已记住 WiFi：首页也要跑状态机建 MQTT，不能只在 WiFi 页 poll */
    if(app_wifi_cfg_get_ssid()[0] != '\0') {
        return 1u;
    }
#if (APP_WIFI_MODEM_WF24 != 0)
    if(s_modem_at_ok == 0u && app_wifi_cfg_request_reconnect() != 0u) {
        return 1u;
    }
#endif
    return 0u;
#else
    (void)on_wifi_scr;
    return 0u;
#endif
}

uint8_t cloud_aliyun_at_is_online(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return (s_step == ALIYUN_STEP_ONLINE) ? 1u : 0u;
#else
    return 0u;
#endif
}

uint8_t cloud_aliyun_at_mqtt_connecting(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return (s_step >= ALIYUN_STEP_CIPSTART && s_step <= ALIYUN_STEP_WAIT_CONNACK) ? 1u : 0u;
#else
    return 0u;
#endif
}

void cloud_uart2_ensure_init(void)
{
    uart2_init_once();
}

int cloud_uart2_send(const char *text)
{
    uart2_init_once();
    return uart2_send_text(text);
}

int cloud_uart2_wait(const char *needle, uint32_t timeout_ms)
{
    uart2_init_once();
    return uart2_wait_text(needle, timeout_ms);
}

int cloud_uart2_read_byte(uint8_t *out, uint32_t timeout_ms)
{
    uart2_init_once();
    return uart2_read_byte_timeout(out, timeout_ms);
}

void cloud_uart2_rx_clear(void)
{
#if (MODEM_USE_NATIVE_MQTT != 0)
    if(s_step >= ALIYUN_STEP_CIPSTART && s_step <= ALIYUN_STEP_ONLINE) {
        wf24_poll_mqtt_urc();
    }
#endif
    s_rx_win_pos = 0u;
    s_rx_win[0] = '\0';
}

int cloud_uart2_rx_has(const char *needle)
{
    if(needle == NULL || needle[0] == '\0') return 0;
    return (strstr(s_rx_win, needle) != NULL) ? 1 : 0;
}

static void cloud_uart2_rx_push_char(char ch)
{
    if(s_rx_win_pos < (sizeof(s_rx_win) - 1u)) {
        s_rx_win[s_rx_win_pos++] = ch;
    } else {
        memmove(s_rx_win, s_rx_win + 1, sizeof(s_rx_win) - 2u);
        s_rx_win[sizeof(s_rx_win) - 2u] = ch;
    }
    s_rx_win[sizeof(s_rx_win) - 1u] = '\0';
}

static void uart2_pump_rx_core(uint32_t duration_ms, uint8_t ignore_ui_busy)
{
    uint8_t ch = 0u;
    uint32_t end;

#if (APP_ALIYUN_SNTP_ENABLE == 1)
    if(cloud_aliyun_sntp_step_active() != 0u) {
        sntp_uart_pump(duration_ms);
        return;
    }
#endif
    if(ignore_ui_busy == 0u && s_uart_ui_busy != 0u) {
        return;
    }
    uart2_init_once();
    if(duration_ms == 0u) {
        return;
    }
    end = HAL_GetTick() + duration_ms;
    while((HAL_GetTick() < end) != 0u) {
        (void)uart2_read_byte_timeout(&ch, 8u);
        uart2_coop_yield();
    }
}

void cloud_uart2_pump_rx(uint32_t duration_ms)
{
    uart2_pump_rx_core(duration_ms, 0u);
}

uint16_t cloud_uart2_collect_to(char *dst, uint16_t dst_sz, uint16_t dst_pos, uint32_t duration_ms)
{
    uint8_t ch = 0u;
    uint32_t end;

    uart2_init_once();
    if(dst_sz == 0u) {
        return dst_pos;
    }
    end = HAL_GetTick() + duration_ms;
    while((HAL_GetTick() < end) != 0u) {
        if(cwlap_scan_should_stop() != 0u) {
            break;
        }
        if(uart2_ring_pop(&ch) != 0u) {
            cloud_uart2_rx_push_char((char)ch);
            if(dst != NULL && dst_pos < (dst_sz - 1u)) {
                dst[dst_pos++] = (char)ch;
            }
            continue;
        }
        uart2_coop_yield();
    }
    if(dst != NULL && dst_pos < dst_sz) {
        dst[dst_pos] = '\0';
    }
    return dst_pos;
}

uint8_t cloud_uart2_ui_busy(void)
{
    return s_uart_ui_busy;
}

void cloud_uart2_set_ui_busy(uint8_t busy)
{
    s_uart_ui_busy = busy ? 1u : 0u;
}

static void cloud_uart2_hw_drain_ms(uint32_t ms)
{
    uint8_t ch = 0u;
    uint32_t end;

    if(!s_uart2_inited) {
        return;
    }
    end = HAL_GetTick() + ms;
    while((HAL_GetTick() < end) != 0u) {
        if(cwlap_scan_should_stop() != 0u) {
            break;
        }
        (void)uart2_read_byte_timeout(&ch, 5u);
    }
}

/* 读到串口连续 quiet_ms 无新字节或超过 max_ms 即停，并清 RX 窗/环形缓冲 */
static void cloud_uart2_drain_until_idle(uint32_t max_ms, uint32_t quiet_ms)
{
    uint32_t t0;
    uint32_t last_rx;
    uint8_t ch = 0u;

    if(!s_uart2_inited) {
        return;
    }
    if(max_ms == 0u) {
        max_ms = ALIYUN_MQTT_DRAIN_MAX_MS;
    }
    if(quiet_ms == 0u) {
        quiet_ms = ALIYUN_MQTT_DRAIN_QUIET_MS;
    }
    t0 = HAL_GetTick();
    last_rx = t0;
    while((HAL_GetTick() - t0) < max_ms) {
        if(uart2_read_byte_timeout(&ch, 2u) != 0) {
            last_rx = HAL_GetTick();
            continue;
        }
        if((HAL_GetTick() - last_rx) >= quiet_ms) {
            break;
        }
        uart2_coop_yield();
    }
    cloud_uart2_rx_clear();
    uart2_ring_clear();
    if(s_uart2.Instance == USART2) {
        __HAL_UART_CLEAR_OREFLAG(&s_uart2);
        __HAL_UART_CLEAR_FEFLAG(&s_uart2);
        __HAL_UART_CLEAR_NEFLAG(&s_uart2);
    }
}

static void aliyun_mqtt_uart_prep(void)
{
    cloud_uart2_drain_until_idle(ALIYUN_MQTT_DRAIN_MAX_MS, ALIYUN_MQTT_DRAIN_QUIET_MS);
}

static void aliyun_mqtt_close_link0(void)
{
    s_unlock_flush_done = 0u;
    cloud_uart2_drain_until_idle(60u, 10u);
#if (APP_WIFI_MODEM_WF24 != 0)
    (void)uart2_send_text("AT+MQTTCLEAN\r\n");
    (void)uart2_wait_text("OK", ALIYUN_WAIT_MID_MS);
    wf24_mqtt_reset_state();
    s_wf24_mqtt_conn_ok = 0u;
#else
    (void)uart2_send_text("AT+CIPCLOSE=0\r\n");
    (void)uart2_wait_text("OK", 150u);
    (void)uart2_wait_text("CLOSED", 100u);
#endif
    cloud_uart2_drain_until_idle(ALIYUN_MQTT_DRAIN_MAX_MS, ALIYUN_MQTT_DRAIN_QUIET_MS);
}

/** MQTT 发送失败时立即下线并重连，避免长时间假在线 */
static void aliyun_mqtt_schedule_reconnect(const char *reason)
{
    if(s_step != ALIYUN_STEP_ONLINE) {
        return;
    }
#if (APP_CLOUD_TRACE != 0)
    if(reason != NULL) {
        char tr[52];
        (void)snprintf(tr, sizeof(tr), "[CLOUD] MQTT drop %s\r\n", reason);
        cloud_trace_tx(tr);
    }
#endif
    s_cloud_scan_kick_done = 0u;
    aliyun_ntp_reset_bridge_wait();
#if (APP_CLOUD_COMMAND_ENABLE != 0)
    app_cloud_command_publish_offline_best_effort();
#endif
    aliyun_mqtt_close_link0();
#if (APP_CLOUD_COMMAND_ENABLE != 0)
    app_cloud_command_reset();
#endif
    s_ntp_sub_sess = 0u;
    s_ntp_sub_ok = 0u;
    s_ntp_sub_pkt_id = 0u;
    app_pair_mark_ui_dirty();
    if(app_wifi_policy_may_run_mqtt() != 0u) {
#if (APP_CLOUD_FAST_AFTER_WIFI_JOIN != 0)
        if(s_wifi_sta_ip_ok != 0u) {
            s_mqtt_fast_join = 1u;
        }
#endif
        s_cipstart_next_ms = HAL_GetTick() + ALIYUN_MQTT_RECONNECT_MS;
        s_step = ALIYUN_STEP_CIPSTART;
    } else {
        s_step = ALIYUN_STEP_WIFI_IDLE;
    }
}

/* 必须是完整 OK 行，避免启动日志里的 OK 子串误判 */
static uint8_t uart2_rx_has_ok_reply(void)
{
    if(cloud_uart2_rx_has("busy p") != 0) {
        return 0u;
    }
    if(strstr(s_rx_win, "\r\nOK\r\n") != NULL) {
        return 1u;
    }
    if(strstr(s_rx_win, "\nOK\r\n") != NULL) {
        return 1u;
    }
    if(strstr(s_rx_win, "OK\r\n") != NULL && strstr(s_rx_win, "FAIL") == NULL) {
        return 1u;
    }
    return 0u;
}

/* 发 AT 并等 OK；遇 busy p... 则排空后重试 */
static uint8_t uart2_at_cmd_wait_ok(const char *cmd, uint32_t timeout_ms)
{
    uint8_t try;

    if(cmd == NULL || cmd[0] == '\0') {
        return 0u;
    }
    for(try = 0u; try < 4u; try++) {
        cloud_uart2_hw_drain_ms(60u);
        cloud_uart2_rx_clear();
        if(!uart2_send_text(cmd)) {
            continue;
        }
        if(uart2_wait_text("OK", timeout_ms) != 0 &&
           uart2_rx_has_ok_reply() != 0u) {
            return 1u;
        }
        if(strstr(s_rx_win, "busy") != NULL || strstr(s_rx_win, "BUSY") != NULL) {
            cloud_uart2_hw_drain_ms(300u);
            continue;
        }
    }
    return 0u;
}

void cloud_uart2_drain_hw(uint32_t ms)
{
    cloud_uart2_hw_drain_ms(ms);
}

typedef enum {
    ESP_RST_ST_IDLE = 0,
    ESP_RST_ST_LOW,
    ESP_RST_ST_BOOT,
    ESP_RST_ST_DRAIN
} esp_rst_st_t;

static esp_rst_st_t s_esp_rst_st = ESP_RST_ST_IDLE;
static uint32_t s_esp_rst_phase_ms = 0u;
static uint32_t s_esp_rst_boot_ms = 400u;
static uint32_t s_esp_rst_drain_ms = 0u;

uint8_t cloud_uart2_esp_hw_reset_active(void)
{
    return (s_esp_rst_st != ESP_RST_ST_IDLE) ? 1u : 0u;
}

uint8_t cloud_uart2_esp_hw_reset_begin(uint32_t boot_ms)
{
    if(s_esp_rst_st != ESP_RST_ST_IDLE) {
        return 0u;
    }
    if(boot_ms < 400u) {
        boot_ms = 400u;
    }
    s_esp_rst_boot_ms = boot_ms;
    s_esp_rst_drain_ms = 0u;
    esp_rst_gpio_init_once();
    esp_rst_set(0u);
    s_esp_rst_phase_ms = HAL_GetTick();
    s_esp_rst_st = ESP_RST_ST_LOW;
    return 1u;
}

uint8_t cloud_uart2_esp_hw_reset_poll(void)
{
    uint32_t now;

    if(s_esp_rst_st == ESP_RST_ST_IDLE) {
        return 1u;
    }
    now = HAL_GetTick();
    if(s_esp_rst_st == ESP_RST_ST_LOW) {
        if((now - s_esp_rst_phase_ms) < 100u) {
            return 0u;
        }
        esp_rst_set(1u);
        s_esp_rst_phase_ms = now;
        s_esp_rst_st = ESP_RST_ST_BOOT;
        return 0u;
    }
    if(s_esp_rst_st == ESP_RST_ST_BOOT) {
        if((now - s_esp_rst_phase_ms) < s_esp_rst_boot_ms) {
            return 0u;
        }
        uart2_init_once();
        s_esp_rst_drain_ms = 0u;
        s_esp_rst_phase_ms = now;
        s_esp_rst_st = ESP_RST_ST_DRAIN;
        return 0u;
    }
    cloud_uart2_hw_drain_ms(30u);
    s_esp_rst_drain_ms += 30u;
    if(s_esp_rst_drain_ms < 1200u) {
        return 0u;
    }
    cloud_uart2_rx_clear();
    s_modem_at_ok = 0u;
    s_esp_rst_st = ESP_RST_ST_IDLE;
    return 1u;
}

void cloud_uart2_esp_hw_reset(uint32_t boot_ms)
{
    (void)cloud_uart2_esp_hw_reset_begin(boot_ms);
    while(cloud_uart2_esp_hw_reset_poll() == 0u) {
        uart2_coop_yield();
    }
}

void cloud_uart2_prepare_for_cwlap_only(void)
{
    uart2_init_once();
    if(s_uart2_baud != 115200u) {
        uart2_set_baud(115200u);
    }
    cloud_uart2_hw_drain_ms(40u);
    cloud_uart2_rx_clear();
}

static void cloud_uart2_prepare_for_cwjap(void)
{
    uint8_t i;

    uart2_init_once();
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    g_wifi_scan_abort = 1u;
    cloud_aliyun_cwlap_scan_abort();
#endif
    cloud_uart2_hw_drain_ms(300u);
    cloud_uart2_rx_clear();
    for(i = 0u; i < 6u; i++) {
        cloud_uart2_rx_clear();
        if(uart2_send_text("AT\r\n") && uart2_wait_text("OK", ALIYUN_WAIT_MID_MS)) {
            break;
        }
        cloud_uart2_hw_drain_ms(120u);
    }
    cloud_uart2_rx_clear();
}

void cloud_uart2_prepare_for_ui_at(void)
{
    uart2_init_once();
    if(s_uart2_baud != 115200u) {
        uart2_set_baud(115200u);
    }
    cloud_uart2_hw_drain_ms(300u);
    cloud_uart2_rx_clear();
#if (APP_ALIYUN_AT_ENABLE == 1)
    /* 扫描/换热点前断开 MQTT TCP，保持 WiFi 关联 */
    if(s_step >= ALIYUN_STEP_CIPSTART) {
#if (APP_WIFI_MODEM_WF24 != 0)
        (void)uart2_send_text("AT+MQTTCLEAN\r\n");
        (void)uart2_wait_text("OK", 1000u);
#else
        (void)uart2_send_text("AT+CIPCLOSE=0\r\n");
        (void)uart2_wait_text("OK", 1000u);
#endif
        cloud_uart2_hw_drain_ms(200u);
        cloud_uart2_rx_clear();
        s_step = ALIYUN_STEP_CIFSR;
        s_step_ms = HAL_GetTick();
    }
#endif
}

uint8_t cloud_aliyun_at_wifi_joined(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return (s_modem_at_ok != 0u && s_wifi_sta_ip_ok != 0u) ? 1u : 0u;
#else
    return 0u;
#endif
}

uint8_t cloud_aliyun_at_sta_on_target_ssid(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    const char *cfg;
#if (APP_WIFI_MODEM_WF24 != 0)
    uint8_t st = 0u;
    char ssid_live[40];

    if(!s_uart2_inited || s_uart_ui_busy != 0u) {
        return 0u;
    }
    cfg = app_wifi_cfg_get_ssid();
    if(cfg == NULL || cfg[0] == '\0') {
        return 0u;
    }
    if(s_wifi_sta_ip_ok != 0u && s_connected_sta_ssid[0] != '\0' &&
       strcmp(cfg, s_connected_sta_ssid) == 0) {
        return 1u;
    }
    if(cloud_aliyun_at_mqtt_connecting() != 0u) {
        return 0u;
    }
    {
        uint32_t now = HAL_GetTick();
        if(s_sta_probe_ms != 0u && (now - s_sta_probe_ms) < CWJAP_STA_PROBE_MS) {
            return 0u;
        }
        s_sta_probe_ms = now;
    }
    if(wf24_query_cwstate(&st, ssid_live, (uint16_t)sizeof(ssid_live)) == 0u) {
        return 0u;
    }
    if(ssid_live[0] != '\0') {
        strncpy(s_connected_sta_ssid, ssid_live, sizeof(s_connected_sta_ssid) - 1u);
        s_connected_sta_ssid[sizeof(s_connected_sta_ssid) - 1u] = '\0';
    }
    if(st != 2u) {
        printf("[ALIYUN] sta check: CWSTATE=%u ssid='%s' (need 2)\r\n",
               (unsigned)st, ssid_live);
        return 0u;
    }
    if(strcmp(cfg, ssid_live) != 0) {
        printf("[ALIYUN] sta check: on '%s' want '%s'\r\n", ssid_live, cfg);
        return 0u;
    }
    return 1u;
#else
    char quoted[40];

    if(!s_uart2_inited || s_uart_ui_busy != 0u) {
        return 0u;
    }
    cfg = app_wifi_cfg_get_ssid();
    if(cfg == NULL || cfg[0] == '\0') {
        return 0u;
    }
    cloud_uart2_rx_clear();
    if(!uart2_send_text("AT+CWJAP?\r\n") || !uart2_wait_text("+CWJAP:", ALIYUN_WAIT_MID_MS)) {
        return 0u;
    }
    cloud_uart2_copy_cwjap_ssid(s_connected_sta_ssid, (uint16_t)sizeof(s_connected_sta_ssid));
    (void)snprintf(quoted, sizeof(quoted), "'%s'", cfg);
    if(strstr(s_rx_win, quoted) != NULL) {
        return 1u;
    }
    (void)snprintf(quoted, sizeof(quoted), "\"%s\"", cfg);
    return (strstr(s_rx_win, quoted) != NULL) ? 1u : 0u;
#endif
#else
    return 0u;
#endif
}

uint8_t cloud_aliyun_at_wifi_link_ready(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return s_wifi_sta_ip_ok;
#else
    return 0u;
#endif
}

void cloud_aliyun_at_request_target_wifi_join(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    const char *cfg;

    if(s_uart_ui_busy != 0u) {
        return;
    }
    cfg = app_wifi_cfg_get_ssid();
    if(cfg == NULL || cfg[0] == '\0') {
        return;
    }
    s_step = ALIYUN_STEP_CWJAP_SET;
    s_step_ms = HAL_GetTick();
#endif
}

static void cloud_uart2_copy_cwjap_ssid(char *out, uint16_t out_sz)
{
    const char *p;
    const char *q;
    uint16_t n;

    if(out == NULL || out_sz < 2u) {
        return;
    }
    out[0] = '\0';
    p = strstr(s_rx_win, "+CWJAP:");
    if(p == NULL) {
        return;
    }
    p = strchr(p, '"');
    if(p == NULL) {
        p = strchr(s_rx_win, '\'');
    }
    if(p == NULL) {
        return;
    }
    p++;
    q = strchr(p, '"');
    if(q == NULL) {
        q = strchr(p, '\'');
    }
    if(q == NULL || q <= p) {
        return;
    }
    n = (uint16_t)(q - p);
    if(n >= out_sz) {
        n = (uint16_t)(out_sz - 1u);
    }
    memcpy(out, p, n);
    out[n] = '\0';
}

uint8_t cloud_aliyun_at_get_connected_ssid(char *out, uint16_t out_sz)
{
    const char *cfg;

    if(out == NULL || out_sz < 2u) {
        return 0u;
    }
    out[0] = '\0';
    if(cloud_aliyun_at_wifi_link_ready() == 0u) {
        return 0u;
    }
    if(s_connected_sta_ssid[0] != '\0') {
        strncpy(out, s_connected_sta_ssid, (size_t)(out_sz - 1u));
        out[out_sz - 1u] = '\0';
        return 1u;
    }
    cfg = app_wifi_cfg_get_ssid();
    if(cfg != NULL && cfg[0] != '\0') {
        strncpy(out, cfg, (size_t)(out_sz - 1u));
        out[out_sz - 1u] = '\0';
        return 1u;
    }
    return 0u;
}

uint8_t cloud_aliyun_at_refresh_connected_ssid(void)
{
    if(!s_uart2_inited || s_uart_ui_busy != 0u) {
        return 0u;
    }
    if(cloud_aliyun_at_wifi_joined() == 0u) {
        s_connected_sta_ssid[0] = '\0';
        return 0u;
    }
    cloud_uart2_rx_clear();
    if(!uart2_send_text("AT+CWJAP?\r\n")) {
        return 0u;
    }
    if(!uart2_wait_text("+CWJAP:", ALIYUN_WAIT_MID_MS)) {
        return 0u;
    }
    cloud_uart2_copy_cwjap_ssid(s_connected_sta_ssid, (uint16_t)sizeof(s_connected_sta_ssid));
    return (s_connected_sta_ssid[0] != '\0') ? 1u : 0u;
}

void cloud_uart2_copy_rx_win(char *dst, uint16_t dst_sz)
{
    if(dst == NULL || dst_sz == 0u) return;
    strncpy(dst, s_rx_win, (size_t)(dst_sz - 1u));
    dst[dst_sz - 1u] = '\0';
}

uint8_t cloud_uart2_modem_ready(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return (s_uart2_inited != 0u && s_modem_at_ok != 0u) ? 1u : 0u;
#else
    return s_uart2_inited;
#endif
}

uint8_t cloud_uart2_try_modem_ready(void)
{
    return cloud_uart2_try_modem_ready_timeout(2000u);
}

uint8_t cloud_uart2_try_modem_ready_timeout(uint32_t wait_ms)
{
    uart2_init_once();
    if(!s_uart2_inited) {
        return 0u;
    }
    cloud_uart2_rx_clear();
    if(uart2_send_text("AT\r\n") && uart2_wait_text("OK", wait_ms)) {
        s_modem_at_ok = 1u;
        return 1u;
    }
    return 0u;
}

void cloud_aliyun_at_request_wifi_ip_verify(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    s_wifi_ip_verify_pending = 1u;
#else
    (void)0;
#endif
}

void cloud_aliyun_at_scr11_enter(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1) && (APP_WIFI_UI_SCAN_ENABLE == 1)
    uart2_init_once();
    /* 扫描前勿清 ui_busy：用户手动 CWJAP 可能正在进行 */
    if(cloud_aliyun_at_user_wifi_join_active() != 0u) {
        return;
    }
    /* 已连 WiFi 时进 WiFi 页仅配置，勿 CIPCLOSE 打断 MQTT */
    if(cloud_aliyun_at_wifi_link_ready() != 0u) {
        s_wifi_ip_verify_pending = 1u;
        s_scr11_mqtt_paused = 0u;
        return;
    }
    /* STA 已掉线：清 scr11 暂停 MQTT 残留，避免无法扫描 */
    if(s_scr11_mqtt_paused != 0u) {
        s_scr11_mqtt_paused = 0u;
        s_scr11_resume_step = ALIYUN_STEP_IDLE;
        s_step = ALIYUN_STEP_WIFI_IDLE;
        s_step_ms = HAL_GetTick();
        return;
    }
#if (MODEM_USE_NATIVE_MQTT != 0)
    /* WF24 原生 MQTT：扫描前仅暂停 poll，不发 CIPCLOSE */
    if(s_step == ALIYUN_STEP_ONLINE || cloud_aliyun_at_is_online() != 0u) {
        s_scr11_resume_step = s_step;
        s_scr11_mqtt_paused = 1u;
        s_step = ALIYUN_STEP_WIFI_IDLE;
        s_step_ms = HAL_GetTick();
    }
#else
    if(s_step >= ALIYUN_STEP_CIPSTART) {
        s_scr11_resume_step = s_step;
        cloud_uart2_hw_drain_ms(80u);
        cloud_uart2_rx_clear();
        (void)uart2_send_text("AT+CIPCLOSE=0\r\n");
        (void)uart2_wait_text("OK", 400u);
        cloud_uart2_hw_drain_ms(40u);
        cloud_uart2_rx_clear();
        s_scr11_mqtt_paused = 1u;
        s_step = ALIYUN_STEP_WIFI_IDLE;
        s_step_ms = HAL_GetTick();
    }
#endif
#else
    (void)0;
#endif
}

void cloud_aliyun_at_scr11_leave(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1) && (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(s_scr11_mqtt_paused == 0u) {
        if(cloud_aliyun_at_wifi_link_ready() == 0u &&
           cloud_aliyun_at_wifi_bringup_active() == 0u) {
            cloud_aliyun_at_user_wifi_join_abort();
        }
        /* WiFi 已连且 MQTT 未暂停：保持 ONLINE/建链，勿 CIFSR 打断长连 */
        return;
    }
    s_scr11_mqtt_paused = 0u;
    if(cloud_aliyun_at_wifi_link_ready() == 0u &&
       cloud_aliyun_at_wifi_bringup_active() == 0u) {
        cloud_aliyun_at_user_wifi_join_abort();
    }
#if (MODEM_USE_NATIVE_MQTT != 0)
    if(cloud_aliyun_at_wifi_link_ready() != 0u) {
        if(s_scr11_resume_step == ALIYUN_STEP_ONLINE || s_wf24_mqtt_conn_ok != 0u) {
            s_step = ALIYUN_STEP_ONLINE;
        } else if(s_modem_at_ok != 0u) {
            s_step = ALIYUN_STEP_WIFI_IDLE;
        }
    } else if(s_modem_at_ok != 0u) {
        s_step = ALIYUN_STEP_WIFI_IDLE;
    }
#else
    if(s_modem_at_ok != 0u) {
        s_step = ALIYUN_STEP_CIFSR;
    } else if(s_scr11_resume_step >= ALIYUN_STEP_CIPSTART) {
        s_step = ALIYUN_STEP_CIPSTART;
    } else {
        s_step = s_scr11_resume_step;
    }
#endif
    s_step_ms = HAL_GetTick();
    s_scr11_resume_step = ALIYUN_STEP_IDLE;
    cloud_uart2_rx_clear();
#else
    (void)0;
#endif
}

#if (APP_ALIYUN_AT_ENABLE == 1) && (APP_WIFI_UI_SCAN_ENABLE == 1)

#if 0 /* rev=23: async 分片路径已废弃，仅 blocking run_cwlap_scan */

#define CWLAP_SLICE_MS        30u
#define CWLAP_BURST_MS        120u
#define CWLAP_TOTAL_MS        22000u
#define CWLAP_NO_DATA_MS      6000u
#define CWLAP_IDLE_GAP_MS     2500u
#define CWLAP_GARBAGE_MS      10000u
#define CWLAP_MODEM_TRY_MAX   5u
#define CWLAP_RST_BOOT_MS     2500u
#define CWLAP_RST_DRAIN_MS    500u

typedef enum {
    CA_IDLE = 0,
    CA_PREP,
    CA_MODEM_SEND,
    CA_MODEM_WAIT,
    CA_RST_HOLD,
    CA_RST_RELEASE,
    CA_RST_BOOT,
    CA_RST_DRAIN,
    CA_DIRECT_SEND,
    CA_COLLECT,
    CA_COLLECT_TAIL,
    CA_RECOVER_AT_SEND,
    CA_RECOVER_AT_WAIT,
    CA_ATE0_SEND,
    CA_ATE0_WAIT,
    CA_CWMODE_SEND,
    CA_CWMODE_WAIT,
    CA_CWLAP_SEND,
    CA_DONE,
    CA_FAIL
} cwlap_async_st_t;

static struct {
    cwlap_async_st_t st;
    uint32_t deadline;
    uint32_t collect_t0;
    uint32_t last_growth;
    uint16_t pos;
    uint16_t last_pos;
    uint8_t saw_cwlap;
    uint8_t recover_retry;
    uint8_t did_hw_reset;
    uint8_t direct_tried;
    uint8_t modem_try;
    int fail_rc;
    char *dst;
    uint16_t dst_sz;
    uint16_t *out_len;
} s_cwlap_async;

static int uart2_wait_slice(const char *needle, uint32_t deadline_ms)
{
    uint8_t ch = 0u;
    uint32_t slice_end;

    if(needle != NULL && needle[0] != '\0' && cloud_uart2_rx_has(needle) != 0) {
        return 1;
    }
    if((int32_t)(HAL_GetTick() - deadline_ms) >= 0) {
        return -1;
    }
    slice_end = HAL_GetTick() + CWLAP_SLICE_MS;
    while((int32_t)(HAL_GetTick() - slice_end) < 0) {
        if((int32_t)(HAL_GetTick() - deadline_ms) >= 0) {
            return -1;
        }
        if(HAL_UART_Receive(&s_uart2, &ch, 1u, 1u) == HAL_OK) {
            cloud_uart2_rx_push_char((char)ch);
            if(needle != NULL && needle[0] != '\0' && cloud_uart2_rx_has(needle) != 0) {
                return 1;
            }
        }
        uart2_coop_yield();
    }
    return 0;
}

static int uart2_push_byte_to_dst(uint8_t ch)
{
    char *dst = s_cwlap_async.dst;
    uint16_t dst_sz = s_cwlap_async.dst_sz;
    uint16_t pos = s_cwlap_async.pos;

    cloud_uart2_rx_push_char((char)ch);
    if(dst != NULL && pos < (dst_sz - 1u)) {
        dst[pos++] = (char)ch;
        dst[pos] = '\0';
        s_cwlap_async.pos = pos;
    }
    return 1;
}

static void cwlap_async_fail(int rc)
{
    if(s_cwlap_async.out_len != NULL) {
        *s_cwlap_async.out_len = s_cwlap_async.pos;
    }
    s_cwlap_async.fail_rc = rc;
    s_cwlap_async.st = CA_FAIL;
}

static void cwlap_async_success(void)
{
    if(s_cwlap_async.out_len != NULL) {
        *s_cwlap_async.out_len = s_cwlap_async.pos;
    }
    cloud_uart2_set_ui_busy(0u);
    s_cwlap_async.st = CA_DONE;
}

static void uart2_clear_errors(void)
{
    if(!s_uart2_inited) {
        return;
    }
    __HAL_UART_CLEAR_OREFLAG(&s_uart2);
    __HAL_UART_CLEAR_FEFLAG(&s_uart2);
    __HAL_UART_CLEAR_NEFLAG(&s_uart2);
}

static uint8_t cwlap_collect_slice(void)
{
    uint32_t end = HAL_GetTick() + CWLAP_BURST_MS;
    uint16_t pos = s_cwlap_async.pos;

    while((int32_t)(HAL_GetTick() - end) < 0) {
        pos = cloud_uart2_collect_to(s_cwlap_async.dst, s_cwlap_async.dst_sz, pos, CWLAP_SLICE_MS);
        s_cwlap_async.pos = pos;
    }
    return 1u;
}

static uint8_t cwlap_collect_check_done(uint8_t tail_phase)
{
    uint32_t tnow = HAL_GetTick();
    char *dst = s_cwlap_async.dst;

    if(s_cwlap_async.pos > s_cwlap_async.last_pos) {
        s_cwlap_async.last_pos = s_cwlap_async.pos;
        s_cwlap_async.last_growth = tnow;
    }
    if(dst != NULL && strstr(dst, "+CWLAP") != NULL) {
        s_cwlap_async.saw_cwlap = 1u;
    }
    if(cloud_uart2_rx_has("ERROR") != 0 || cloud_uart2_rx_has("FAIL") != 0) {
        return 2u;
    }
    if((tnow - s_cwlap_async.collect_t0) >= CWLAP_TOTAL_MS) {
        return 2u;
    }
    if(s_cwlap_async.saw_cwlap != 0u) {
        if(cloud_uart2_rx_has("OK") != 0) {
            return 1u;
        }
        /* 已有 +CWLAP 且收包停顿：视为扫完（与 rev=13 阻塞版一致，不硬等 OK） */
        if((tnow - s_cwlap_async.last_growth) >= CWLAP_IDLE_GAP_MS) {
            return 1u;
        }
        (void)tail_phase;
        return 0u;
    }
    /* 尚未见到 +CWLAP */
    if(s_cwlap_async.pos == 0u && (tnow - s_cwlap_async.collect_t0) >= CWLAP_NO_DATA_MS) {
        return 2u;
    }
    if(s_cwlap_async.pos > 0u && (tnow - s_cwlap_async.collect_t0) >= CWLAP_GARBAGE_MS) {
        return 2u;
    }
    if(s_cwlap_async.pos > 0u && (tnow - s_cwlap_async.last_growth) >= 5000u) {
        return 2u;
    }
    (void)tail_phase;
    return 0u;
}

void cloud_aliyun_cwlap_scan_begin(void)
{
    memset(&s_cwlap_async, 0, sizeof(s_cwlap_async));
    s_cwlap_async.st = CA_PREP;
    s_cwlap_async.fail_rc = CWLAP_SCAN_RUNNING;
}

void cloud_aliyun_cwlap_scan_abort(void)
{
    /* 仅置 abort 标志：实际 UART2/CWLAP 正在跑时，不能提前清 ui_busy，
     * 否则 CWJAP/MQTT 可能与 CWLAP 并发导致 ESP8266 返回 busy p... */
    g_wifi_scan_abort = 1u;
    memset(&s_cwlap_async, 0, sizeof(s_cwlap_async));
}

uint8_t cloud_aliyun_cwlap_scan_active(void)
{
    cwlap_async_st_t st = s_cwlap_async.st;
    return (st != CA_IDLE && st != CA_DONE && st != CA_FAIL) ? 1u : 0u;
}

int cloud_aliyun_cwlap_scan_step(char *dst, uint16_t dst_sz, uint16_t *out_len)
{
    int wr;
    uint8_t collect_done;

    if(s_cwlap_async.st == CA_IDLE) {
        return 0;
    }
    if(s_cwlap_async.st == CA_DONE) {
        s_cwlap_async.st = CA_IDLE;
        return 0;
    }
    if(s_cwlap_async.st == CA_FAIL) {
        int rc = s_cwlap_async.fail_rc;
        cloud_uart2_set_ui_busy(0u);
        s_cwlap_async.st = CA_IDLE;
        return rc;
    }

    s_cwlap_async.dst = dst;
    s_cwlap_async.dst_sz = dst_sz;
    s_cwlap_async.out_len = out_len;

    switch(s_cwlap_async.st) {
    case CA_PREP:
        WIFI_DBG("cwlap async prep modem=%u", (unsigned)cloud_uart2_modem_ready());
        uart2_init_once();
        cloud_uart2_set_ui_busy(1u);
        if(!s_uart2_inited) {
            cwlap_async_fail(-1);
            break;
        }
        cloud_uart2_prepare_for_cwlap_only();
        if(cloud_uart2_modem_ready() == 0u) {
            s_cwlap_async.modem_try = 0u;
            s_cwlap_async.st = CA_MODEM_SEND;
            break;
        }
        if(dst != NULL && dst_sz > 0u) {
            memset(dst, 0, dst_sz);
        }
        s_cwlap_async.pos = 0u;
        s_cwlap_async.st = CA_DIRECT_SEND;
        break;

    case CA_MODEM_SEND:
        cloud_uart2_rx_clear();
        if(!uart2_send_text("AT\r\n")) {
            s_cwlap_async.modem_try++;
            if(s_cwlap_async.modem_try < CWLAP_MODEM_TRY_MAX) {
                s_cwlap_async.st = CA_MODEM_SEND;
                return CWLAP_SCAN_RUNNING;
            }
            if(s_cwlap_async.did_hw_reset == 0u) {
                s_cwlap_async.st = CA_RST_HOLD;
            } else {
                cwlap_async_fail(-2);
            }
            break;
        }
        s_cwlap_async.deadline = HAL_GetTick() + 2500u;
        s_cwlap_async.st = CA_MODEM_WAIT;
        break;

    case CA_MODEM_WAIT:
        wr = uart2_wait_slice("OK", s_cwlap_async.deadline);
        if(wr == 0) {
            return CWLAP_SCAN_RUNNING;
        }
        if(wr < 0) {
            s_cwlap_async.modem_try++;
            WIFI_DBG("cwlap async modem AT miss try=%u", (unsigned)s_cwlap_async.modem_try);
            if(s_cwlap_async.modem_try < CWLAP_MODEM_TRY_MAX) {
                cloud_uart2_hw_drain_ms(40u);
                s_cwlap_async.st = CA_MODEM_SEND;
                break;
            }
            if(s_cwlap_async.did_hw_reset == 0u) {
                WIFI_DBG("cwlap async modem AT fail -> hw reset");
                s_cwlap_async.st = CA_RST_HOLD;
            } else {
                cwlap_async_fail(-2);
            }
            break;
        }
        s_modem_at_ok = 1u;
        s_cwlap_async.modem_try = 0u;
        WIFI_DBG("cwlap async modem ok");
        cloud_uart2_prepare_for_cwlap_only();
        if(dst != NULL && dst_sz > 0u) {
            memset(dst, 0, dst_sz);
        }
        s_cwlap_async.pos = 0u;
        s_cwlap_async.st = CA_DIRECT_SEND;
        break;

    case CA_RST_HOLD:
        WIFI_DBG("cwlap async ESP hw reset");
        esp_rst_gpio_init_once();
        esp_rst_set(0u);
        s_cwlap_async.deadline = HAL_GetTick() + 100u;
        s_cwlap_async.did_hw_reset = 1u;
        s_cwlap_async.modem_try = 0u;
        s_cwlap_async.st = CA_RST_RELEASE;
        break;

    case CA_RST_RELEASE:
        if((int32_t)(HAL_GetTick() - s_cwlap_async.deadline) < 0) {
            uart2_coop_yield();
            return CWLAP_SCAN_RUNNING;
        }
        esp_rst_set(1u);
        s_cwlap_async.deadline = HAL_GetTick() + CWLAP_RST_BOOT_MS;
        s_cwlap_async.st = CA_RST_BOOT;
        break;

    case CA_RST_BOOT:
        if((int32_t)(HAL_GetTick() - s_cwlap_async.deadline) < 0) {
            uart2_coop_yield();
            return CWLAP_SCAN_RUNNING;
        }
        uart2_init_once();
        cloud_uart2_rx_clear();
        s_modem_at_ok = 0u;
        s_cwlap_async.deadline = HAL_GetTick() + CWLAP_RST_DRAIN_MS;
        s_cwlap_async.st = CA_RST_DRAIN;
        break;

    case CA_RST_DRAIN:
        (void)cloud_uart2_collect_to(NULL, 0u, 0u, CWLAP_SLICE_MS);
        if((int32_t)(HAL_GetTick() - s_cwlap_async.deadline) < 0) {
            return CWLAP_SCAN_RUNNING;
        }
        s_cwlap_async.st = CA_MODEM_SEND;
        break;

    default:
        break;
    }

    switch(s_cwlap_async.st) {
    case CA_DIRECT_SEND:
        if(s_cwlap_async.direct_tried != 0u) {
            s_cwlap_async.st = CA_RECOVER_AT_SEND;
            break;
        }
        s_cwlap_async.direct_tried = 1u;
        cloud_uart2_rx_clear();
        uart2_clear_errors();
        if(!uart2_send_text("AT+CWLAP\r\n")) {
            WIFI_DBG("cwlap async direct send fail");
            s_cwlap_async.st = CA_RECOVER_AT_SEND;
            break;
        }
        WIFI_DBG("cwlap async send direct");
        s_cwlap_async.collect_t0 = HAL_GetTick();
        s_cwlap_async.last_growth = s_cwlap_async.collect_t0;
        s_cwlap_async.last_pos = 0u;
        s_cwlap_async.saw_cwlap = 0u;
        s_cwlap_async.st = CA_COLLECT;
        return CWLAP_SCAN_RUNNING;

    case CA_COLLECT:
        {
            static uint32_t s_collect_log_ms;
            uint32_t tnow = HAL_GetTick();

            if(s_collect_log_ms == 0u || (tnow - s_collect_log_ms) >= 3000u) {
                s_collect_log_ms = tnow;
                if(dst != NULL && s_cwlap_async.pos > 0u) {
                    WIFI_DBG("cwlap collect bytes=%u elapsed=%ums raw=%.*s",
                             (unsigned)s_cwlap_async.pos,
                             (unsigned)(tnow - s_cwlap_async.collect_t0),
                             (int)((s_cwlap_async.pos < 64u) ? s_cwlap_async.pos : 64u),
                             dst);
                } else {
                    WIFI_DBG("cwlap collect bytes=%u elapsed=%ums",
                             (unsigned)s_cwlap_async.pos,
                             (unsigned)(tnow - s_cwlap_async.collect_t0));
                }
            }
        }
        (void)cwlap_collect_slice();
        if(dst != NULL && s_cwlap_async.pos > 0u) {
            app_wifi_scan_notify_rx(dst, s_cwlap_async.pos);
        }
        collect_done = cwlap_collect_check_done(0u);
        if(collect_done == 0u) {
            return CWLAP_SCAN_RUNNING;
        }
        if(dst != NULL && strstr(dst, "+CWLAP") != NULL &&
           (collect_done == 1u || collect_done == 2u)) {
            if(collect_done == 1u) {
                WIFI_DBG("cwlap async direct ok bytes=%u", (unsigned)s_cwlap_async.pos);
            } else {
                WIFI_DBG("cwlap async direct ok idle bytes=%u", (unsigned)s_cwlap_async.pos);
            }
            s_cwlap_async.st = CA_COLLECT_TAIL;
            return CWLAP_SCAN_RUNNING;
        }
        if(collect_done == 1u) {
            s_cwlap_async.st = CA_COLLECT_TAIL;
            return CWLAP_SCAN_RUNNING;
        }
        WIFI_DBG("cwlap async direct fail bytes=%u rx=%.*s",
                 (unsigned)s_cwlap_async.pos,
                 (int)((s_cwlap_async.pos < 48u) ? s_cwlap_async.pos : 48u),
                 (dst != NULL) ? dst : "");
        if(cloud_uart2_modem_ready() == 0u && s_cwlap_async.did_hw_reset == 0u) {
            s_cwlap_async.st = CA_RST_HOLD;
            return CWLAP_SCAN_RUNNING;
        }
        s_cwlap_async.recover_retry = 0u;
        s_cwlap_async.st = CA_RECOVER_AT_SEND;
        break;

    case CA_COLLECT_TAIL:
        (void)cwlap_collect_slice();
        if(strstr(dst != NULL ? dst : "", "+CWLAP") != NULL) {
            cwlap_async_success();
            WIFI_DBG("cwlap async done bytes=%u", (unsigned)s_cwlap_async.pos);
            return 0;
        }
        cwlap_async_fail(-5);
        break;

    case CA_RECOVER_AT_SEND:
        if(s_cwlap_async.recover_retry >= 2u) {
            if(dst != NULL && dst_sz > 0u) {
                cloud_uart2_copy_rx_win(dst, dst_sz < 96u ? dst_sz : 96u);
            }
            cwlap_async_fail(-2);
            break;
        }
        cloud_uart2_rx_clear();
        if(!uart2_send_text("AT\r\n")) {
            s_cwlap_async.recover_retry++;
            s_cwlap_async.st = CA_RECOVER_AT_SEND;
            return CWLAP_SCAN_RUNNING;
        }
        s_cwlap_async.deadline = HAL_GetTick() + 1500u;
        s_cwlap_async.st = CA_RECOVER_AT_WAIT;
        return CWLAP_SCAN_RUNNING;

    case CA_RECOVER_AT_WAIT:
        wr = uart2_wait_slice("OK", s_cwlap_async.deadline);
        if(wr == 0) {
            return CWLAP_SCAN_RUNNING;
        }
        if(wr < 0) {
            s_cwlap_async.recover_retry++;
            cloud_uart2_hw_drain_ms(40u);
            s_cwlap_async.st = CA_RECOVER_AT_SEND;
            return CWLAP_SCAN_RUNNING;
        }
        WIFI_DBG("cwlap async AT recover ok retry=%u", (unsigned)s_cwlap_async.recover_retry);
        s_cwlap_async.st = CA_ATE0_SEND;
        break;

    case CA_ATE0_SEND:
        cloud_uart2_rx_clear();
        modem_at_echo_off();
#if (MODEM_SKIP_ATE0 != 0)
        s_cwlap_async.st = CA_CWMODE_SEND;
#else
        s_cwlap_async.deadline = HAL_GetTick() + ALIYUN_WAIT_SHORT_MS;
        s_cwlap_async.st = CA_ATE0_WAIT;
#endif
        return CWLAP_SCAN_RUNNING;

    case CA_ATE0_WAIT:
        (void)uart2_wait_slice("OK", s_cwlap_async.deadline);
        cloud_uart2_rx_clear();
        s_cwlap_async.st = CA_CWMODE_SEND;
        break;

    case CA_CWMODE_SEND:
        WIFI_DBG("cwlap async CWMODE=1");
        cloud_uart2_rx_clear();
        uart2_clear_errors();
        if(!uart2_send_text(MODEM_AT_CWMODE_STA)) {
            cwlap_async_fail(-3);
            break;
        }
        s_cwlap_async.deadline = HAL_GetTick() + ALIYUN_WAIT_LONG_MS;
        s_cwlap_async.st = CA_CWMODE_WAIT;
        return CWLAP_SCAN_RUNNING;

    case CA_CWMODE_WAIT:
        wr = uart2_wait_slice("OK", s_cwlap_async.deadline);
        if(wr == 0) {
            return CWLAP_SCAN_RUNNING;
        }
        if(wr < 0) {
            cwlap_async_fail(-3);
            break;
        }
        cloud_uart2_rx_clear();
        s_cwlap_async.st = CA_CWLAP_SEND;
        break;

    case CA_CWLAP_SEND:
        cloud_uart2_rx_clear();
        uart2_clear_errors();
        if(!uart2_send_text("AT+CWLAP\r\n")) {
            cwlap_async_fail(-4);
            break;
        }
        WIFI_DBG("cwlap async send CWLAP");
        if(dst != NULL && dst_sz > 0u) {
            memset(dst, 0, dst_sz);
        }
        s_cwlap_async.pos = 0u;
        s_cwlap_async.collect_t0 = HAL_GetTick();
        s_cwlap_async.last_growth = s_cwlap_async.collect_t0;
        s_cwlap_async.last_pos = 0u;
        s_cwlap_async.saw_cwlap = 0u;
        s_cwlap_async.st = CA_COLLECT;
        return CWLAP_SCAN_RUNNING;

    default:
        break;
    }

    if(s_cwlap_async.st == CA_FAIL) {
        int rc = s_cwlap_async.fail_rc;
        cloud_uart2_set_ui_busy(0u);
        s_cwlap_async.st = CA_IDLE;
        return rc;
    }
    if(s_cwlap_async.st == CA_DONE) {
        s_cwlap_async.st = CA_IDLE;
        return 0;
    }
    return CWLAP_SCAN_RUNNING;
}

#endif /* rev=23 async disabled */

void cloud_aliyun_cwlap_scan_begin(void)
{
}

void cloud_aliyun_cwlap_scan_abort(void)
{
    /* 仅置 abort：由 CloudTask poll 走 cwk_finish_fail(-9) 并等 ESP idle */
    g_wifi_scan_abort = 1u;
}

uint8_t cloud_aliyun_cwlap_scan_active(void)
{
    return cloud_aliyun_at_cwlap_scan_async_active();
}

int cloud_aliyun_cwlap_scan_step(char *dst, uint16_t dst_sz, uint16_t *out_len)
{
    (void)dst;
    (void)dst_sz;
    if(out_len != NULL && cloud_aliyun_at_cwlap_scan_async_pos(out_len) != 0u) {
        /* pos updated by poll */
    }
    return cloud_aliyun_at_cwlap_scan_async_poll();
}

static void cloud_uart2_flush_rx(void)
{
    uart2_init_once();
    cloud_uart2_hw_drain_ms(400u);
    cloud_uart2_rx_clear();
    uart2_ring_clear();
    if(s_uart2.Instance == USART2) {
        __HAL_UART_CLEAR_OREFLAG(&s_uart2);
        __HAL_UART_CLEAR_FEFLAG(&s_uart2);
        __HAL_UART_CLEAR_NEFLAG(&s_uart2);
    }
}

/* 复位后等 ESP-AT 打印 ready（部分模组只打版本信息，无 ready 也可靠 AT 探活） */
static uint8_t cloud_uart2_wait_esp_ready(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    uint8_t ch = 0u;

    cloud_uart2_hw_drain_ms(2000u);
    while((HAL_GetTick() - t0) < timeout_ms) {
        if(uart2_read_byte_timeout(&ch, 50u) != 0) {
            if(strstr(s_rx_win, "ready") != NULL ||
               strstr(s_rx_win, "Ready") != NULL ||
               strstr(s_rx_win, "power on") != NULL ||
               strstr(s_rx_win, "Power On") != NULL) {
                return 1u;
            }
        } else {
            uart2_coop_yield();
        }
    }
    return 0u;
}

/* 快路径：仅清旧连接 + STA 模式，避免 skip prep 后 CWJAP 立刻 DISCONNECT */
static uint8_t cwjap_scr11_min_prep(void)
{
    cloud_uart2_rx_clear();
    /* 未连热点时 CWQAP 可能 ERROR，不因此失败 */
    (void)uart2_at_cmd_wait_ok("AT+CWQAP\r\n", 3000u);
    cloud_uart2_hw_drain_ms(400u);
    cloud_uart2_rx_clear();
    if(uart2_at_cmd_wait_ok("AT\r\n", 1500u) == 0u) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP min prep AT fail\r\n");
        return 0u;
    }
    cloud_uart2_rx_clear();
    if(uart2_at_cmd_wait_ok(MODEM_AT_CWMODE_STA, 5000u) == 0u) {
        cloud_uart2_hw_drain_ms(500u);
        cloud_uart2_rx_clear();
        if(uart2_at_cmd_wait_ok(MODEM_AT_CWMODE_STA, 5000u) == 0u) {
            CWJAP_TRACE_MSG("[WiFi] CWJAP min prep CWMODE fail\r\n");
            return 0u;
        }
    }
    cloud_uart2_hw_drain_ms(400u);
    cloud_uart2_rx_clear();
    s_cwjap_esp_idle_ok = 0u;
    return 1u;
}

static uint8_t cwjap_scr11_radio_prep(void)
{
    cloud_uart2_rx_clear();
    if(uart2_at_cmd_wait_ok("AT\r\n", 2000u) == 0u) {
        return 0u;
    }
    cloud_uart2_rx_clear();
    if(uart2_at_cmd_wait_ok(MODEM_AT_CWMODE_STA, ALIYUN_WAIT_MID_MS) == 0u) {
        return 0u;
    }
#if (MODEM_SKIP_CIP_STACK == 0)
    cloud_uart2_rx_clear();
    if(uart2_at_cmd_wait_ok("AT+CIPMUX=0\r\n", ALIYUN_WAIT_SHORT_MS) == 0u) {
        return 0u;
    }
    cloud_uart2_rx_clear();
    if(uart2_at_cmd_wait_ok("AT+CIPSERVER=0\r\n", ALIYUN_WAIT_SHORT_MS) == 0u) {
        return 0u;
    }
#endif
    cloud_uart2_rx_clear();
    return 1u;
}

/* 连续 AT 探活；need_ok=2 用于 CWJAP 前，3 用于扫描 abort 收尾 */
static uint8_t cloud_uart2_wait_modem_idle_ex(uint32_t timeout_ms, uint8_t need_ok)
{
    uint32_t t0 = HAL_GetTick();
    uint8_t ok_streak = 0u;

    if(need_ok < 1u) {
        need_ok = 1u;
    }
    while((HAL_GetTick() - t0) < timeout_ms) {
        cloud_uart2_rx_clear();
        if(uart2_at_cmd_wait_ok("AT\r\n", 3000u) != 0u) {
            ok_streak++;
            if(ok_streak >= need_ok) {
                return 1u;
            }
            cloud_uart2_hw_drain_ms(250u);
        } else {
            ok_streak = 0u;
            if(strstr(s_rx_win, "busy") != NULL || strstr(s_rx_win, "BUSY") != NULL) {
                cloud_uart2_hw_drain_ms(800u);
            } else {
                cloud_uart2_hw_drain_ms(300u);
            }
        }
    }
    return 0u;
}

static uint8_t cloud_uart2_wait_modem_idle(uint32_t timeout_ms)
{
    return cloud_uart2_wait_modem_idle_ex(timeout_ms, 3u);
}

#if (APP_WIFI_UI_SCAN_ENABLE == 1)
static void cwjap_modem_purge(uint8_t hard_reset)
{
    g_wifi_scan_abort = 1u;
    g_wifi_scan_pending = 0u;
    cloud_uart2_flush_rx();
    if(hard_reset != 0u) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP esp reset\r\n");
        cloud_uart2_esp_hw_reset(1200u);
        cloud_uart2_flush_rx();
        cloud_uart2_hw_drain_ms(500u);
        if(cloud_uart2_wait_esp_ready(5000u) == 0u) {
            CWJAP_TRACE_MSG("[WiFi] CWJAP wait ready timeout\r\n");
        }
    } else {
        CWJAP_TRACE_MSG("[WiFi] CWJAP soft settle\r\n");
        cloud_uart2_wait_quiet_after_scan_abort(3500u);
        cloud_uart2_hw_drain_ms(500u);
    }
    cloud_uart2_flush_rx();
    modem_at_echo_off();
    if(cloud_uart2_wait_modem_idle_ex(12000u, 1u) == 0u) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP modem idle fail\r\n");
        /* Fallback: one-shot AT probe is enough to continue CWJAP path. */
        if(uart2_at_cmd_wait_ok("AT\r\n", 1200u) != 0u) {
            CWJAP_TRACE_MSG("[WiFi] CWJAP modem idle degraded-ok\r\n");
            s_modem_at_ok = 1u;
        } else {
            s_modem_at_ok = 0u;
        }
    } else {
        s_modem_at_ok = 1u;
    }
    cloud_uart2_rx_clear();
}
#else
static void cwjap_modem_purge(uint8_t hard_reset)
{
    (void)hard_reset;
    cloud_uart2_flush_rx();
    cloud_uart2_esp_hw_reset(1200u);
    (void)cloud_uart2_wait_esp_ready(5000u);
    if(cloud_uart2_wait_modem_idle_ex(12000u, 1u) == 0u) {
        if(uart2_at_cmd_wait_ok("AT\r\n", 1200u) != 0u) {
            s_modem_at_ok = 1u;
        } else {
            s_modem_at_ok = 0u;
        }
    } else {
        s_modem_at_ok = 1u;
    }
}
#endif

static void cloud_uart2_quiet_for_cwlap(void)
{
    cloud_uart2_flush_rx();
#if (MODEM_SKIP_CIP_STACK == 0)
    (void)uart2_send_text("AT+CIPCLOSE=0\r\n");
    (void)uart2_wait_text("OK", 600u);
    (void)uart2_wait_text("ERROR", 300u);
    cloud_uart2_flush_rx();
    modem_at_echo_off();
#endif
    cloud_uart2_flush_rx();
}

static uint8_t cwlap_frame_valid(const char *dst, uint16_t pos)
{
#if (APP_WIFI_MODEM_WF24 != 0)
    return modem_cwlap_frame_valid(dst, pos);
#else
    if(dst == NULL || pos < 12u) {
        return 0u;
    }
    if(strstr(dst, "+CWLAP:(") == NULL) {
        return 0u;
    }
    if(strstr(dst, "\r\nOK\r\n") != NULL || strstr(dst, "\nOK\r\n") != NULL) {
        return 1u;
    }
    return 0u;
#endif
}

#define CWK_SLICE_MS        5u
#define CWK_TOTAL_MS        22000u
#define CWK_NO_DATA_MS      6000u
#define CWK_IDLE_GAP_MS     2500u
#define CWK_TAIL_MS         400u

typedef enum {
    CWK_IDLE = 0,
    CWK_COLLECT,
    CWK_TAIL,
    CWK_RECOVER_COLLECT,
    CWK_RECOVER_TAIL
} cwk_phase_t;

static struct {
    cwk_phase_t phase;
    char *dst;
    uint16_t dst_sz;
    uint16_t *out_len;
    uint16_t pos;
    uint16_t last_pos;
    uint32_t t0;
    uint32_t last_growth;
    uint32_t tail_t0;
    uint8_t saw_cwlap;
    uint8_t direct_try;
    int fail_rc;
} s_cwk;

static void cwk_reset(void)
{
    memset(&s_cwk, 0, sizeof(s_cwk));
    s_cwk.phase = CWK_IDLE;
}

void cloud_aliyun_at_cwlap_scan_async_abort(void)
{
    g_wifi_scan_abort = 1u;
}

uint8_t cloud_aliyun_at_cwlap_teardown_idle(uint32_t timeout_ms)
{
    uint32_t quiet_ms;
    uint32_t idle_ms;

    if(timeout_ms < 2000u) {
        timeout_ms = 2000u;
    }
    uart2_mutex_init_once();
    if(uart2_mutex_take(800u) == 0u) {
        return 0u;
    }
    quiet_ms = (timeout_ms > CWJAP_TEARDOWN_QUIET_MS) ? CWJAP_TEARDOWN_QUIET_MS : timeout_ms;
    idle_ms = (timeout_ms > quiet_ms) ? (timeout_ms - quiet_ms) : CWJAP_TEARDOWN_IDLE_MS;
    if(idle_ms > CWJAP_TEARDOWN_IDLE_MS) {
        idle_ms = CWJAP_TEARDOWN_IDLE_MS;
    }
    cloud_uart2_wait_quiet_after_scan_abort(quiet_ms);
    if(cloud_uart2_wait_modem_idle(idle_ms) == 0u) {
        uart2_mutex_give();
        return 0u;
    }
    uart2_mutex_give();
    return 1u;
}

uint8_t cloud_aliyun_at_cwlap_scan_async_active(void)
{
    return (s_cwk.phase != CWK_IDLE) ? 1u : 0u;
}

uint8_t cloud_aliyun_at_cwlap_scan_async_pos(uint16_t *out_len)
{
    if(out_len != NULL) {
        *out_len = s_cwk.pos;
    }
    return 1u;
}

static void cwk_feed_byte(uint8_t ch)
{
    if(s_cwk.dst != NULL && s_cwk.pos < (s_cwk.dst_sz - 1u)) {
        s_cwk.dst[s_cwk.pos++] = (char)ch;
        s_cwk.dst[s_cwk.pos] = '\0';
    }
}

static void cwk_drain_slice(uint32_t slice_ms)
{
    uint32_t end = HAL_GetTick() + slice_ms;
    uint8_t ch;

    while((int32_t)(HAL_GetTick() - (int32_t)end) < 0) {
        if(cwlap_scan_should_stop() != 0u) {
            break;
        }
        if(uart2_ring_pop(&ch) == 0u) {
            uart2_coop_yield();
            continue;
        }
        cloud_uart2_rx_push_char((char)ch);
        cwk_feed_byte(ch);
    }
    if(s_cwk.pos > s_cwk.last_pos) {
        s_cwk.last_pos = s_cwk.pos;
        s_cwk.last_growth = HAL_GetTick();
    }
    if(s_cwk.dst != NULL && strstr(s_cwk.dst, "+CWLAP") != NULL) {
        s_cwk.saw_cwlap = 1u;
    }
}

static int cwk_finish_ok(void)
{
    if(s_cwk.out_len != NULL) {
        *s_cwk.out_len = s_cwk.pos;
    }
    /* 扫描结束后再吸收尾包，避免 CWJAP 命中 busy p */
    (void)cloud_uart2_collect_to(NULL, 0u, 0u, 400u);
    cloud_uart2_flush_rx();
    cloud_uart2_wait_quiet_after_scan_abort(1500u);
    if(cloud_uart2_wait_modem_idle(8000u) == 0u) {
        CWJAP_TRACE_MSG("[WiFi] CWLAP finish idle fail\r\n");
        printf("[WiFi] CWLAP finish idle fail\r\n");
        cloud_uart2_set_ui_busy(0u);
        s_cwk.fail_rc = -5;
        cwk_reset();
        uart2_mutex_give();
        return -5;
    }
    cloud_uart2_set_ui_busy(0u);
    cwk_reset();
    uart2_mutex_give();
    return 0;
}

static int cwk_finish_fail(int rc)
{
    if(s_cwk.out_len != NULL) {
        *s_cwk.out_len = s_cwk.pos;
    }
    /* abort 时 ESP 侧 CWLAP 可能仍在跑，须等真 idle 再释放 UART */
    if(rc == -9) {
        CWJAP_TRACE_MSG("[WiFi] scan abort wait idle\r\n");
        cloud_uart2_wait_quiet_after_scan_abort(5000u);
        if(cloud_uart2_wait_modem_idle(18000u) == 0u) {
            CWJAP_TRACE_MSG("[WiFi] scan abort idle fail\r\n");
        }
    }
    cloud_uart2_set_ui_busy(0u);
    s_cwk.fail_rc = rc;
    cwk_reset();
    uart2_mutex_give();
    return rc;
}

static int cwk_check_fail(void)
{
    uint32_t tnow = HAL_GetTick();

    if(cloud_uart2_rx_has("ERROR") != 0 || cloud_uart2_rx_has("FAIL") != 0) {
#if (APP_WIFI_CWJAP_TRACE != 0)
        CWJAP_TRACE_MSG("[WiFi] CWLAP fail: ERROR/FAIL in rx\r\n");
#endif
        return -6;
    }
    if((tnow - s_cwk.t0) >= CWK_TOTAL_MS) {
#if (APP_WIFI_CWJAP_TRACE != 0)
        CWJAP_TRACE_MSG("[WiFi] CWLAP fail: total timeout\r\n");
#endif
        return -5;
    }
    if(s_cwk.saw_cwlap == 0u) {
        if(s_cwk.pos == 0u && (tnow - s_cwk.t0) >= CWK_NO_DATA_MS) {
#if (APP_WIFI_CWJAP_TRACE != 0)
            CWJAP_TRACE_MSG("[WiFi] CWLAP fail: no data timeout\r\n");
#endif
            return -5;
        }
    } else if((s_cwk.dst != NULL &&
               (strstr(s_cwk.dst, "\r\nOK\r\n") != NULL || strstr(s_cwk.dst, "\nOK\r\n") != NULL))) {
#if (APP_WIFI_CWJAP_TRACE != 0)
        CWJAP_TRACE_MSG("[WiFi] CWLAP finish: got OK tail\r\n");
#endif
        return 0;
    } else if((tnow - s_cwk.last_growth) >= CWK_IDLE_GAP_MS) {
        /* 仅空闲但未见完整 OK，不提前判收尾，继续等到总超时。 */
#if (APP_WIFI_CWJAP_TRACE != 0)
        CWJAP_TRACE_MSG("[WiFi] CWLAP idle gap, waiting OK\r\n");
#endif
        return 1;
    }
    return 1;
}

static void cwk_begin_collect(char *dst, uint16_t dst_sz, uint16_t *out_len, cwk_phase_t phase)
{
    cwk_reset();
    s_cwk.phase = phase;
    s_cwk.dst = dst;
    s_cwk.dst_sz = dst_sz;
    s_cwk.out_len = out_len;
    s_cwk.t0 = HAL_GetTick();
    s_cwk.last_growth = s_cwk.t0;
    if(dst != NULL && dst_sz > 0u) {
        memset(dst, 0, dst_sz);
    }
}

int cloud_aliyun_at_cwlap_scan_async_start(char *dst, uint16_t dst_sz, uint16_t *out_len)
{
    if(dst == NULL || dst_sz < 64u || out_len == NULL) {
        return -1;
    }
    WIFI_DBG("cwlap async start");
    uart2_init_once();
    if(!s_uart2_inited) {
        return -1;
    }
    /* 尝试取 mutex；CWJAP/MQTT 正在运行时 50ms 后再试 */
    if(uart2_mutex_take(50u) == 0u) {
        WIFI_DBG("cwlap async start: mutex busy, defer");
        return CWLAP_SCAN_DEFERRED;
    }
    cwk_reset();
    cloud_uart2_set_ui_busy(1u);
    if(cloud_uart2_modem_ready() == 0u) {
        (void)cloud_uart2_try_modem_ready();
    }
    cloud_uart2_prepare_for_cwlap_only();
    cloud_uart2_quiet_for_cwlap();
    if(cwlap_scan_should_stop() != 0u) {
        cloud_uart2_set_ui_busy(0u);
        uart2_mutex_give();
        return -9;
    }
    cloud_uart2_flush_rx();
    if(!uart2_send_text("AT+CWLAP\r\n")) {
        cloud_uart2_set_ui_busy(0u);
        uart2_mutex_give();
        return -4;
    }
    WIFI_DBG("cwlap async send try=0");
    cwk_begin_collect(dst, dst_sz, out_len, CWK_COLLECT);
    /* mutex 保持到 async_poll 结束（cwk_finish_ok/fail 里 give） */
    return CWLAP_SCAN_RUNNING;
}

int cloud_aliyun_at_cwlap_scan_async_poll(void)
{
    int st;

    if(s_cwk.phase == CWK_IDLE) {
        /* 勿返回 0：GuiTask 会误判扫描结束；真正结束由 cwk_finish_ok 返回 0 */
        return CWLAP_SCAN_RUNNING;
    }
    if(cwlap_scan_should_stop() != 0u) {
        return cwk_finish_fail(-9);
    }

    if(s_cwk.phase == CWK_TAIL || s_cwk.phase == CWK_RECOVER_TAIL) {
        cwk_drain_slice(CWK_SLICE_MS);
        if((HAL_GetTick() - s_cwk.tail_t0) >= CWK_TAIL_MS) {
            if(cwlap_frame_valid(s_cwk.dst, s_cwk.pos) != 0u) {
                WIFI_DBG("cwlap async ok bytes=%u", (unsigned)s_cwk.pos);
                return cwk_finish_ok();
            }
            return cwk_finish_fail(-6);
        }
        return CWLAP_SCAN_RUNNING;
    }

    cwk_drain_slice(CWK_SLICE_MS);
    st = cwk_check_fail();
    if(st == 0) {
        if(cwlap_frame_valid(s_cwk.dst, s_cwk.pos) != 0u) {
            WIFI_DBG("cwlap async ok bytes=%u", (unsigned)s_cwk.pos);
            return cwk_finish_ok();
        }
        return cwk_finish_fail(-6);
    }
    if(st == 2) {
        s_cwk.phase = (s_cwk.phase == CWK_RECOVER_COLLECT) ? CWK_RECOVER_TAIL : CWK_TAIL;
        s_cwk.tail_t0 = HAL_GetTick();
        return CWLAP_SCAN_RUNNING;
    }
    if(st < 0) {
        if(s_cwk.phase == CWK_COLLECT && s_cwk.direct_try < 2u) {
            s_cwk.direct_try++;
            cloud_uart2_flush_rx();
            if(!uart2_send_text("AT+CWLAP\r\n")) {
                return cwk_finish_fail(-4);
            }
            WIFI_DBG("cwlap async send try=%u", (unsigned)s_cwk.direct_try);
            s_cwk.pos = 0u;
            s_cwk.last_pos = 0u;
            s_cwk.saw_cwlap = 0u;
            s_cwk.t0 = HAL_GetTick();
            s_cwk.last_growth = s_cwk.t0;
            if(s_cwk.dst != NULL && s_cwk.dst_sz > 0u) {
                memset(s_cwk.dst, 0, s_cwk.dst_sz);
            }
            return CWLAP_SCAN_RUNNING;
        }
        if(s_cwk.phase == CWK_COLLECT) {
            WIFI_DBG("cwlap async direct fail -> recover");
            cloud_uart2_flush_rx();
            if(!(uart2_send_text(MODEM_AT_CWMODE_STA) && uart2_wait_text("OK", ALIYUN_WAIT_LONG_MS))) {
                return cwk_finish_fail(-3);
            }
            cloud_uart2_flush_rx();
            if(!uart2_send_text("AT+CWLAP\r\n")) {
                return cwk_finish_fail(-4);
            }
            s_cwk.phase = CWK_RECOVER_COLLECT;
            s_cwk.pos = 0u;
            s_cwk.last_pos = 0u;
            s_cwk.saw_cwlap = 0u;
            s_cwk.t0 = HAL_GetTick();
            s_cwk.last_growth = s_cwk.t0;
            if(s_cwk.dst != NULL && s_cwk.dst_sz > 0u) {
                memset(s_cwk.dst, 0, s_cwk.dst_sz);
            }
            return CWLAP_SCAN_RUNNING;
        }
        return cwk_finish_fail(st);
    }
    return CWLAP_SCAN_RUNNING;
}

static int cwlap_collect_framed(char *dst, uint16_t dst_sz, uint16_t *out_len)
{
    uint16_t pos = 0u;
    uint16_t last_pos = 0u;
    uint32_t t0;
    uint32_t last_growth;
    uint8_t saw_cwlap = 0u;

    if(dst == NULL || dst_sz < 64u || out_len == NULL) {
        return -5;
    }
    memset(dst, 0, dst_sz);
    t0 = HAL_GetTick();
    last_growth = t0;
    while((HAL_GetTick() - t0) < 22000u) {
        if(cwlap_scan_should_stop() != 0u) {
            return -9;
        }
        pos = cloud_uart2_collect_to(dst, dst_sz, pos, 120u);
        if(pos > last_pos) {
            last_pos = pos;
            last_growth = HAL_GetTick();
        } else if(pos == 0u && (HAL_GetTick() - t0) >= 6000u) {
            break;
        } else if(pos > 0u && (HAL_GetTick() - last_growth) >= 3000u) {
            break;
        }
        if(cloud_uart2_rx_has("ERROR") != 0 || cloud_uart2_rx_has("FAIL") != 0) {
            break;
        }
        if(strstr(dst, "+CWLAP") != NULL) {
            saw_cwlap = 1u;
        }
        if(saw_cwlap != 0u && cloud_uart2_rx_has("OK") != 0) {
            break;
        }
        uart2_coop_yield();
    }
    if(saw_cwlap != 0u) {
        pos = cloud_uart2_collect_to(dst, dst_sz, pos, 400u);
    }
    if(pos < dst_sz) {
        dst[pos] = '\0';
    }
    *out_len = pos;
    if(cwlap_frame_valid(dst, pos) != 0u) {
        return 0;
    }
    if(pos > 0u) {
        WIFI_DBG("cwlap bad frame bytes=%u head=%.*s",
                 (unsigned)pos,
                 (int)((pos < 48u) ? pos : 48u),
                 dst);
        return -6;
    }
    return -5;
}

static int cwlap_try_direct(char *dst, uint16_t dst_sz, uint16_t *out_len)
{
    uint8_t attempt;
    int rc;

    for(attempt = 0u; attempt < 3u; attempt++) {
        if(cwlap_scan_should_stop() != 0u) {
            return -9;
        }
        cloud_uart2_flush_rx();
        if(!uart2_send_text("AT+CWLAP\r\n")) {
            WIFI_DBG("cwlap send fail try=%u", (unsigned)attempt);
            continue;
        }
        WIFI_DBG("cwlap send try=%u", (unsigned)attempt);
        rc = cwlap_collect_framed(dst, dst_sz, out_len);
        if(rc == 0) {
            WIFI_DBG("cwlap ok bytes=%u try=%u", (unsigned)(*out_len), (unsigned)attempt);
            return 0;
        }
        WIFI_DBG("cwlap collect rc=%d try=%u", rc, (unsigned)attempt);
    }
    return -5;
}

static int cwlap_collect_list(char *dst, uint16_t dst_sz, uint16_t *out_len)
{
    return cwlap_collect_framed(dst, dst_sz, out_len);
}

int cloud_aliyun_at_run_cwlap_scan(char *dst, uint16_t dst_sz, uint16_t *out_len)
{
    uint16_t pos = 0u;
    int rc;

    if(dst == NULL || dst_sz < 64u || out_len == NULL) {
        return -1;
    }
    WIFI_DBG("cwlap enter");
    WIFI_TRACE("cwlap blocking enter");
    uart2_init_once();
    if(!s_uart2_inited) {
        WIFI_TRACE("cwlap no uart2");
        return -1;
    }

    *out_len = 0u;
    uart2_mutex_init_once();
    if(uart2_mutex_take(1200u) == 0u) {
        WIFI_TRACE("cwlap mutex timeout");
        return -2;
    }

    g_wifi_scan_abort = 0u;
    cloud_uart2_set_ui_busy(1u);
    if(cloud_uart2_modem_ready() == 0u) {
        if(cloud_uart2_try_modem_ready() == 0u) {
            WIFI_TRACE("cwlap esp reset begin");
#if (APP_WIFI_CWJAP_TRACE != 0)
            CWJAP_TRACE_MSG("[WiFi] cwlap: esp hw reset\r\n");
#endif
            WIFI_DBG("cwlap: esp hw reset (modem not ready)");
            cloud_uart2_esp_hw_reset(2500u);
            cloud_uart2_flush_rx();
            WIFI_TRACE("cwlap esp wait ready");
            (void)cloud_uart2_wait_esp_ready(6000u);
            WIFI_TRACE("cwlap esp try AT");
            (void)cloud_uart2_try_modem_ready();
            modem_at_echo_off();
        }
    }
    if(cloud_uart2_modem_ready() == 0u) {
        WIFI_TRACE("cwlap modem still dead");
        printf("[WiFi] cwlap abort: modem dead after reset\r\n");
        cloud_uart2_set_ui_busy(0u);
        uart2_mutex_give();
        return -2;
    }
    WIFI_TRACE("cwlap modem ok");
    cloud_uart2_prepare_for_cwlap_only();
    cloud_uart2_quiet_for_cwlap();

    if(cwlap_scan_should_stop() != 0u) {
        rc = -9;
        goto cwlap_out;
    }

    WIFI_TRACE("cwlap try direct");
    rc = cwlap_try_direct(dst, dst_sz, out_len);
    if(rc == -9) {
        goto cwlap_out;
    }
    if(rc == 0) {
        goto cwlap_out;
    }

    /* 直扫 3 次仍无合法 +CWLAP:( 帧：走 CWMODE 恢复后再扫一次 */
    WIFI_DBG("cwlap direct fail -> recover path");
    WIFI_TRACE("cwlap recover CWMODE");
    cloud_uart2_flush_rx();
    if(!(uart2_send_text(MODEM_AT_CWMODE_STA) && uart2_wait_text("OK", ALIYUN_WAIT_LONG_MS))) {
        rc = -3;
        goto cwlap_out;
    }
    cloud_uart2_flush_rx();
    if(!uart2_send_text("AT+CWLAP\r\n")) {
        rc = -4;
        goto cwlap_out;
    }
    rc = cwlap_collect_framed(dst, dst_sz, &pos);
    *out_len = pos;
    WIFI_DBG("cwlap exit rc=%d bytes=%u", rc, (unsigned)pos);
    WIFI_TRACE("cwlap blocking exit");

cwlap_out:
    cloud_uart2_set_ui_busy(0u);
    uart2_mutex_give();
    return rc;
}

#else /* !(APP_ALIYUN_AT && APP_WIFI_UI_SCAN) */

void cloud_aliyun_cwlap_scan_begin(void)
{
}

void cloud_aliyun_cwlap_scan_abort(void)
{
    g_wifi_scan_abort = 1u;
    cloud_uart2_set_ui_busy(0u);
}

uint8_t cloud_aliyun_cwlap_scan_active(void)
{
    return 0u;
}

int cloud_aliyun_cwlap_scan_step(char *dst, uint16_t dst_sz, uint16_t *out_len)
{
    (void)dst;
    (void)dst_sz;
    (void)out_len;
    return -1;
}

int cloud_aliyun_at_run_cwlap_scan(char *dst, uint16_t dst_sz, uint16_t *out_len)
{
    (void)dst;
    (void)dst_sz;
    if(out_len != NULL) {
        *out_len = 0u;
    }
    return -1;
}

void cloud_aliyun_at_cwlap_scan_async_abort(void)
{
}

uint8_t cloud_aliyun_at_cwlap_scan_async_active(void)
{
    return 0u;
}

uint8_t cloud_aliyun_at_cwlap_teardown_idle(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return 1u;
}

int cloud_aliyun_at_cwlap_scan_async_start(char *dst, uint16_t dst_sz, uint16_t *out_len)
{
    (void)dst;
    (void)dst_sz;
    (void)out_len;
    return -1;
}

int cloud_aliyun_at_cwlap_scan_async_poll(void)
{
    return -1;
}

#endif /* APP_ALIYUN_AT && APP_WIFI_UI_SCAN */

#if (APP_WIFI_UART_DEBUG != 0)
void cloud_aliyun_at_wifi_join_diag_printf(void)
{
    printf("[ALIYUN] join_diag step=%s join_st=%u ui_busy=%u async=%u scan_busy=%u\r\n",
           aliyun_step_name(s_step),
           (unsigned)s_user_wifi_join_st,
           (unsigned)cloud_uart2_ui_busy(),
           (unsigned)cloud_aliyun_at_cwlap_scan_async_active(),
           (unsigned)app_wifi_scan_busy());
}
#endif

#if (APP_ALIYUN_AT_ENABLE == 1)

static void cloud_aliyun_at_wifi_join_fail_finalize(void);

static void cloud_aliyun_at_wifi_link_lost(const char *reason)
{
    uint8_t was_up;
    uint8_t joining;
    uint8_t need_teardown;

    was_up = s_wifi_sta_ip_ok;
    joining = (uint8_t)(s_user_wifi_join_st == 1u);
    need_teardown = (uint8_t)(
        was_up != 0u || joining != 0u || s_step == ALIYUN_STEP_ONLINE ||
        s_step >= ALIYUN_STEP_CIPSTART || s_scr11_mqtt_paused != 0u ||
        s_connected_sta_ssid[0] != '\0' || cloud_aliyun_at_is_online() != 0u);

    /* 无论是否重复 URC，先清 STA/云端同步标志，避免 scr11 误显「已连接」 */
    s_wifi_sta_ip_ok = 0u;
    s_wifi_ip_verify_pending = 0u;
    s_wifi_got_ip_ms = 0u;
    s_wf24_cwstate_query_ms = 0u;
    s_connected_sta_ssid[0] = '\0';
    s_scr11_mqtt_paused = 0u;
    s_scr11_resume_step = ALIYUN_STEP_IDLE;
    s_cloud_scan_kick_done = 0u;
    s_ntp_await_until_ms = 0u;
#if (APP_ALIYUN_SNTP_ENABLE == 1)
    s_sntp_synced = 0u;
    s_sntp_give_up = 0u;
    s_sntp_cfg_sent = 0u;
    s_http_date_tried = 0u;
#endif
#if (APP_WIFI_MODEM_WF24 != 0)
    s_wf24_sntp_active = 0u;
    s_wf24_sntp_retry_ms = 0u;
#endif
    s_ntp_synced = 0u;
    s_ntp_mqtt_fail_cnt = 0u;
    s_ntp_next_request_ms = 0u;
    s_ntp_req_pending = 0u;
    s_ntp_req_sent_ms = 0u;
    s_ntp_sub_pkt_id = 0u;
#if (APP_TIME_TRACE != 0)
    s_ntp_suback_logged = 0u;
    s_ntp_pend_diag_ms = 0u;
#endif
    s_mqtt_online_ms = 0u;
#if (APP_CLOUD_COMMAND_ENABLE != 0)
    app_cloud_command_reset();
#endif
    s_last_ping_ms = HAL_GetTick();
    s_mqtt_fast_join = 0u;
    s_cipstart_next_ms = 0u;
    s_cipsend_retry_cnt = 0u;
    s_ping_fail_streak = 0u;
    s_cifsr_retry_cnt = 0u;
    /* WF24: 掉线后需要重新触发 CWJAP；否则会停在 WIFI_IDLE 一直 CWSTATE=4 */
    app_wifi_cfg_mark_reconnect_if_saved();
    cloud_aliyun_at_invalidate_unlock_flush();
    app_wifi_remember_on_wifi_down();
    app_wifi_scan_on_sta_link_down();

    if(need_teardown == 0u) {
        screen_wifi_notify_sta_down();
        app_link_guard_wifi_end(0u);
        app_link_guard_mqtt_end(0u);
        app_cloud_session_wifi_down();
        return;
    }

    CWJAP_TRACE_MSG("[WiFi] STA link lost\r\n");
    if(reason != NULL && reason[0] != '\0') {
        CLOUD_TRACE_MSG("[CLOUD] wifi down\r\n");
        printf("[ALIYUN] WiFi link lost: %s step=%s\r\n", reason, aliyun_step_name(s_step));
    }

    if(s_step >= ALIYUN_STEP_CIPSTART) {
        aliyun_mqtt_close_link0();
#if (MODEM_SKIP_CIP_STACK == 0)
        (void)uart2_send_text("AT+CIPCLOSE=1\r\n");
        (void)uart2_wait_text("OK", 200u);
#endif
        cloud_uart2_hw_drain_ms(40u);
    }
    s_step = ALIYUN_STEP_WIFI_IDLE;
    s_step_ms = HAL_GetTick();

    if(joining != 0u) {
        cloud_aliyun_at_wifi_join_fail_finalize();
        screen_wifi_notify_connect_fail();
    } else {
        if(s_user_wifi_join_st != 0u) {
            s_user_wifi_join_st = 0u;
            s_wifi_join_fail_latch = 0u;
        }
        screen_wifi_notify_sta_down();
    }

    app_link_guard_wifi_end(0u);
    app_link_guard_mqtt_end(0u);
    app_cloud_session_wifi_down();
    app_wifi_connect_reset();
    cloud_uart2_rx_clear();
}

static uint8_t cloud_aliyun_at_wifi_disconnect_urc_valid(void)
{
    const char *disc;
    const char *got_ip;
    const char *cwlap;

    if(cloud_uart2_rx_has("WIFI DISCONNECT") == 0) {
        return 0u;
    }
    if(s_user_wifi_join_st == 1u || s_step == ALIYUN_STEP_CWJAP_SET) {
        return 0u;
    }
#if (APP_ALIYUN_SNTP_ENABLE == 1)
    if(cloud_aliyun_sntp_step_active() != 0u) {
        return 0u;
    }
#endif
    got_ip = strstr(s_rx_win, "WIFI GOT IP");
    disc = strstr(s_rx_win, "WIFI DISCONNECT");
    if(got_ip != NULL && disc != NULL && got_ip < disc) {
        return 0u;
    }
    if(s_wifi_sta_ip_ok != 0u && s_wifi_got_ip_ms != 0u &&
       (HAL_GetTick() - s_wifi_got_ip_ms) < WIFI_DISCONNECT_GRACE_MS) {
        return 0u;
    }
    /* CWLAP 残留里的 DISCONNECT 字样不是真实掉线 */
    cwlap = strstr(s_rx_win, "+CWLAP");
    if(cwlap != NULL && disc != NULL && cwlap < disc) {
        return 0u;
    }
    return 1u;
}

static void cloud_aliyun_at_poll_wifi_urc(void)
{
    if(s_user_wifi_join_st == 1u || s_step == ALIYUN_STEP_CWJAP_SET) {
        return;
    }
    uart2_pump_rx_core(20u, 1u);
    if(cloud_uart2_rx_has("WIFI GOT IP") != 0) {
        return;
    }
    if(cloud_aliyun_at_wifi_disconnect_urc_valid() == 0u) {
        return;
    }
    cloud_aliyun_at_wifi_link_lost("WIFI DISCONNECT URC");
}

static void cloud_aliyun_at_poll_wifi_ip_verify(void)
{
    extern volatile app_scr_t g_app_scr;

    if(s_wifi_ip_verify_pending == 0u) {
        return;
    }
    if(cloud_aliyun_at_mqtt_connecting() != 0u) {
        return;
    }
    if(g_app_scr != APP_SCR_11) {
        s_wifi_ip_verify_pending = 0u;
        return;
    }
    if(s_user_wifi_join_st == 1u || s_step == ALIYUN_STEP_CWJAP_SET) {
        return;
    }
    if(cloud_aliyun_at_cwlap_scan_async_active() != 0u || app_wifi_scan_busy() != 0u) {
        return;
    }
    s_wifi_ip_verify_pending = 0u;
    if(s_wifi_sta_ip_ok == 0u) {
        return;
    }
    if(aliyun_ensure_sta_ip() != 0u) {
        screen_wifi_gui_wake();
        return;
    }
    cloud_aliyun_at_wifi_link_lost("CIFSR no STA IP");
}

static void cloud_aliyun_at_wifi_join_fail_finalize(void)
{
    if(s_user_wifi_join_st == 1u) {
        s_user_wifi_join_st = 3u;
    }
    s_wifi_join_fail_latch = 1u;
    cloud_uart2_set_ui_busy(0u);
    s_cwjap_busy_retry = 0u;
    s_cwjap_esp_idle_ok = 0u;
    s_cwjap_light_join = 0u;
    s_cwjap_light_need_prep = 0u;
    s_cwjap_resend_only = 0u;
    s_cwjap_uart_settle_done = 0u;
    s_sta_radio_prep_done = 0u;
#if (APP_WIFI_MODEM_WF24 != 0)
    s_cwjap_wf24_need_cwqap = 0u;
    cloud_uart2_rx_clear();
    (void)uart2_at_cmd_wait_ok("AT+CWQAP\r\n", 3000u);
    cloud_uart2_hw_drain_ms(200u);
#endif
    s_step = ALIYUN_STEP_WIFI_IDLE;
    if(s_user_wifi_join_st == 3u) {
        app_wifi_cfg_clear_reconnect_request();
    } else {
        /* 开机自动连：与 GitHub 版一致失败后仍重试，勿一次超时就永久放弃 */
        s_wifi_boot_fail_cnt++;
        s_wifi_boot_retry_ms = HAL_GetTick() + CWJAP_BOOT_RETRY_MS;
        printf("[ALIYUN] WiFi boot retry in %lus (fail=%u)\r\n",
               (unsigned long)(CWJAP_BOOT_RETRY_MS / 1000u),
               (unsigned)s_wifi_boot_fail_cnt);
    }
}
#endif

void cloud_aliyun_at_user_wifi_join_set_esp_idle_ok(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    s_cwjap_esp_idle_ok = 1u;
    /* Keep the idle hint, but avoid light-join fast path for stability. */
    s_cwjap_light_join = 0u;
    s_cwjap_light_need_prep = 0u;
#endif
}

void cloud_aliyun_at_user_wifi_join_start(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    /* 此函数在 GuiTask 调用，不得做任何 UART2 AT 命令；只设标志位，
     * 让 CloudTask 的 FSM(CWJAP_SET) 在下一个 poll 里执行实际命令。 */
    if(!s_uart2_inited && !s_modem_at_ok) {
        s_user_wifi_join_st = 3u;
        s_step = ALIYUN_STEP_WIFI_IDLE;
        s_cwjap_esp_idle_ok = 0u;
        CWJAP_TRACE_MSG("[WiFi] CWJAP modem not ready\r\n");
        return;
    }
    s_wifi_join_fail_latch = 0u;
    s_user_wifi_join_st = 1u;
    s_cifsr_retry_cnt = 0u;
    s_cifsr_last_try_ms = 0u;
    s_cwjap_busy_retry = 0u;
    /* First attempt should not force channel; some APs roam channels and stale scan channel fails CWJAP. */
    s_cwjap_try_channel = 0u;
    s_cwjap_light_need_prep = 0u;
    /* s_cwjap_light_join 由 set_esp_idle_ok 置位；此处不清零 */
    s_cwjap_uart_settle_done = 0u;
    s_cwjap_resend_only = 0u;
    s_cwjap_quiet_until_ms = 0u;
    s_user_join_fail_log_ms = 0u;
    s_sta_radio_prep_done = 0u;
    s_wifi_sta_ip_ok = 0u;
#if (APP_WIFI_MODEM_WF24 != 0)
    s_cwjap_wf24_need_cwqap = 1u;
#endif
    /* 清 mqtt_paused 以确保 poll_5ms 会执行 CWJAP_SET */
    s_scr11_mqtt_paused = 0u;
    s_step = ALIYUN_STEP_CWJAP_SET;
    s_step_ms = 0u; /* join 时跳过节流，下一轮 poll 立即执行 */
    __DMB();
    CWJAP_TRACE_MSG("[WiFi] CWJAP join start\r\n");
    if(app_wifi_cfg_get_ssid()[0] != '\0') {
        if(app_wifi_scan_has_ssid(app_wifi_cfg_get_ssid()) == 0u) {
            CWJAP_TRACE_MSG("[WiFi] CWJAP warn: ssid not in scan list\r\n");
        } else if(app_wifi_scan_get_ssid_channel(app_wifi_cfg_get_ssid()) == 0u) {
            CWJAP_TRACE_MSG("[WiFi] CWJAP warn: channel unknown\r\n");
        }
    }
#endif
}

uint8_t cloud_aliyun_at_user_wifi_join_fail_pending(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return s_wifi_join_fail_latch;
#else
    return 0u;
#endif
}

void cloud_aliyun_at_user_wifi_join_fail_clear(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    s_wifi_join_fail_latch = 0u;
#endif
}

uint8_t cloud_aliyun_at_user_wifi_join_poll(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    uint32_t now_ms = HAL_GetTick();

    if(s_wifi_join_fail_latch != 0u) {
        cloud_uart2_set_ui_busy(0u);
        if(s_user_wifi_join_st == 3u) {
            s_user_wifi_join_st = 0u;
        }
        CWJAP_TRACE_MSG("[WiFi] CWJAP join fail\r\n");
        return 2u;
    }

    /* join 标志已清但 FSM 仍停在 CWJAP/CIFSR：避免 GUI 层傻等 CONNECT_TIMEOUT */
    if(s_user_wifi_join_st == 0u && app_wifi_connect_busy() != 0u &&
       (s_step == ALIYUN_STEP_CWJAP_SET || s_step == ALIYUN_STEP_CIFSR)) {
        s_step = ALIYUN_STEP_WIFI_IDLE;
        return 2u;
    }
    if(s_user_wifi_join_st == 1u) {
        return 0u;
    }
    if(s_user_wifi_join_st == 2u) {
        return 1u;
    }
    if(s_user_wifi_join_st == 3u) {
        cloud_uart2_set_ui_busy(0u);
        if(cloud_aliyun_at_wifi_link_ready() != 0u) {
            s_user_wifi_join_st = 0u;
            return 1u;
        }
        CWJAP_TRACE_MSG("[WiFi] CWJAP join fail\r\n");
        s_user_wifi_join_st = 0u;
        return 2u;
    }
    if(s_uart_ui_busy != 0u || s_step == ALIYUN_STEP_CWJAP_SET || s_step == ALIYUN_STEP_CIFSR) {
        return 0u;
    }
    if(cloud_aliyun_at_wifi_link_ready() != 0u) {
        return 1u;
    }
#endif
    return 2u;
}

uint8_t cloud_aliyun_at_user_wifi_join_state(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return s_user_wifi_join_st;
#else
    return 0u;
#endif
}

uint8_t cloud_aliyun_at_user_wifi_join_active(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return (s_user_wifi_join_st == 1u) ? 1u : 0u;
#else
    return 0u;
#endif
}

uint8_t cloud_aliyun_at_wifi_bringup_active(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    /* 勿用 step 区间：ALIYUN_STEP_WIFI_IDLE 夹在 CWJAP_SET 与 CIFSR 之间 */
    switch(s_step) {
    case ALIYUN_STEP_CWJAP_SET:
    case ALIYUN_STEP_CIFSR:
#if (APP_ALIYUN_SNTP_ENABLE == 1)
    case ALIYUN_STEP_SNTP_CFG:
    case ALIYUN_STEP_SNTP_WAIT:
    case ALIYUN_STEP_SNTP_TIME:
#endif
    case ALIYUN_STEP_CIPSTART:
    case ALIYUN_STEP_CIPSEND:
    case ALIYUN_STEP_SEND_CONNECT:
    case ALIYUN_STEP_WAIT_CONNACK:
        return 1u;
    default:
        break;
    }
#endif
    return 0u;
}

uint8_t cloud_aliyun_at_scr11_cloud_hold(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    extern volatile app_scr_t g_app_scr;
    /* WiFi 页未联网且未在连 MQTT 时才暂停 session */
    if((g_app_scr == APP_SCR_11 || app_wifi_scan_ui_active() != 0u) &&
       cloud_aliyun_at_wifi_link_ready() == 0u &&
       app_link_guard_mqtt() == 0u &&
       cloud_aliyun_at_wifi_bringup_active() == 0u) {
        return 1u;
    }
#endif
    return 0u;
}

#if (APP_ALIYUN_SNTP_ENABLE == 1)
/* APP_ALIYUN_SNTP_BEFORE_MQTT=0：直接 MQTT；失败并 give_up 后也不再挡 MQTT */
static uint8_t aliyun_want_esp_sntp_before_mqtt(void)
{
    if(s_sntp_give_up != 0u) {
        return 0u;
    }
#if (APP_ALIYUN_SNTP_BEFORE_MQTT != 0)
    if(app_wall_clock_valid() == 0u && s_sntp_synced == 0u) {
        return 1u;
    }
#endif
    return 0u;
}
#endif

/* WiFi 已拿到 IP 后：按配置决定是否先 ESP SNTP，再建 MQTT */
#if (APP_CLOUD_CONNECT_DIAG != 0)
static void cloud_connect_diag_maybe(void)
{
    static uint32_t s_diag_ms;
    uint32_t now = HAL_GetTick();
    char line[120];

    if(s_wifi_sta_ip_ok == 0u || s_step == ALIYUN_STEP_ONLINE) {
        return;
    }
    if(s_mqtt_fast_join != 0u) {
        return;
    }
    if(s_diag_ms != 0u && (now - s_diag_ms) < 8000u) {
        return;
    }
    s_diag_ms = now;
    (void)snprintf(line, sizeof(line),
                   "[CLOUD][DIAG] step=%s join=%u ui=%u ip=%u mux=%u\r\n",
                   aliyun_step_name(s_step),
                   (unsigned)s_user_wifi_join_st,
                   (unsigned)s_uart_ui_busy,
                   (unsigned)s_wifi_sta_ip_ok,
                   (unsigned)s_cipmux1_ok);
    cloud_trace_tx(line);
}
#endif

static void cloud_aliyun_at_mqtt_kick_after_wifi(void)
{
    if(app_link_guard_wifi() != 0u) {
        return;
    }
#if (APP_WIFI_MODEM_WF24 != 0)
    /* WF24: CWJAP 已返回非 0 IP 时，CWSTATE 可能稍后才变 2；此处优先信任已拿到 IP 的标志位。 */
    if(s_wifi_sta_ip_ok == 0u && aliyun_ensure_sta_ip() == 0u) {
        printf("[ALIYUN] MQTT kick blocked: WiFi not up\r\n");
        return;
    }
#endif
    if(s_step == ALIYUN_STEP_ONLINE) {
        return;
    }
    if(cloud_aliyun_at_is_online() != 0u) {
        return;
    }
    if(cloud_aliyun_at_mqtt_connecting() != 0u) {
        return;
    }
    if(s_step >= ALIYUN_STEP_CIPSTART && s_step <= ALIYUN_STEP_WAIT_CONNACK) {
        return;
    }
#if (APP_ALIYUN_SNTP_ENABLE == 1)
    if(aliyun_want_esp_sntp_before_mqtt() != 0u &&
       s_step != ALIYUN_STEP_SNTP_CFG && s_step != ALIYUN_STEP_SNTP_WAIT &&
       s_step != ALIYUN_STEP_SNTP_TIME &&
       s_step < ALIYUN_STEP_CIPSTART) {
        TIME_TRACE_MSG("[TIME] ESP SNTP before MQTT\r\n");
        s_step = ALIYUN_STEP_SNTP_CFG;
        s_step_ms = HAL_GetTick();
        return;
    }
#endif
    if(app_link_guard_mqtt() == 0u) {
        app_link_guard_mqtt_begin();
    }
    s_scr11_mqtt_paused = 0u;
#if (APP_CLOUD_FAST_AFTER_WIFI_JOIN != 0)
    s_mqtt_fast_join = 1u;
    if(s_wifi_got_ip_ms != 0u) {
        uint32_t ready_ms = s_wifi_got_ip_ms + ALIYUN_MQTT_POST_JOIN_CIPSTART_MS;
        if(HAL_GetTick() < ready_ms) {
            s_cipstart_next_ms = ready_ms;
        } else {
            s_cipstart_next_ms = 0u;
        }
    } else {
        s_cipstart_next_ms = 0u;
    }
#else
    s_mqtt_fast_join = 0u;
    s_cipstart_next_ms = HAL_GetTick() + 150u;
#endif
    s_step = ALIYUN_STEP_CIPSTART;
    s_step_ms = 0u;
    LOG_ESSENTIAL("[CLOUD] MQTT kick\r\n");
}

#if (APP_ALIYUN_SNTP_ENABLE == 1)
static void aliyun_sntp_give_up_and_mqtt_kick(void)
{
    if(s_sntp_give_up == 0u) {
        s_sntp_give_up = 1u;
        TIME_TRACE_MSG("[TIME] ESP SNTP give up -> MQTT\r\n");
    }
    cloud_aliyun_at_mqtt_kick_after_wifi();
}
#endif

void cloud_aliyun_at_user_wifi_join_abort(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    if(s_user_wifi_join_st == 1u ||
       s_step == ALIYUN_STEP_CWJAP_SET ||
       s_step == ALIYUN_STEP_CIFSR) {
        cloud_aliyun_at_wifi_join_fail_finalize();
    }
    s_wifi_join_fail_latch = 0u;
    s_user_wifi_join_st = 0u;
    s_cwjap_busy_retry = 0u;
    s_cwjap_resend_only = 0u;
    s_cwjap_esp_idle_ok = 0u;
    s_cwjap_light_join = 0u;
    s_cwjap_light_need_prep = 0u;
    s_cwjap_try_channel = 0u;
    if(s_step == ALIYUN_STEP_CWJAP_SET || s_step == ALIYUN_STEP_CIFSR) {
        s_step = ALIYUN_STEP_WIFI_IDLE;
        s_step_ms = HAL_GetTick();
    }
    cloud_uart2_set_ui_busy(0u);
    cloud_uart2_drain_hw(200u);
    cloud_uart2_rx_clear();
#else
    (void)0;
#endif
}

void cloud_aliyun_at_request_mqtt_connect(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    if(s_uart_ui_busy != 0u) {
        return;
    }
    if(cloud_aliyun_at_wifi_link_ready() == 0u) {
        return;
    }
    if(app_link_guard_wifi() != 0u) {
        return;
    }
    cloud_aliyun_at_mqtt_kick_after_wifi();
#endif
}

void cloud_aliyun_at_mqtt_session_disconnect(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    if(s_wifi_sta_ip_ok == 0u) {
        if(s_step >= ALIYUN_STEP_CIPSTART) {
            aliyun_mqtt_close_link0();
        }
        s_step = ALIYUN_STEP_WIFI_IDLE;
        s_step_ms = HAL_GetTick();
        s_cloud_scan_kick_done = 0u;
        return;
    }
    if(s_step >= ALIYUN_STEP_CIPSTART) {
        aliyun_mqtt_close_link0();
        cloud_uart2_rx_clear();
    }
    s_step = ALIYUN_STEP_CIFSR;
    s_step_ms = HAL_GetTick();
    s_cloud_scan_kick_done = 0u;
#endif
}

uint8_t cloud_aliyun_at_time_is_synced(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return (s_sntp_synced != 0u || s_ntp_synced != 0u) ? 1u : 0u;
#else
    return 0u;
#endif
}

/* 从 UART 窗口或 +IPD MQTT 载荷中解析物模型 post_reply 的 code */
static int aliyun_thing_model_code_from_buf(const char *buf, uint16_t len)
{
    const char *p;
    const char *end;
    int code = -1;

    if(buf == NULL || len < 8u) {
        return -1;
    }
    end = buf + len;
    for(p = buf; p + 7u < end; p++) {
        if(p[0] != '"' || p[1] != 'c') {
            continue;
        }
        if(strncmp(p, "\"code\":", 7) == 0) {
            if(sscanf(p + 7, "%d", &code) == 1) {
                return code;
            }
        } else if(strncmp(p, "\"code\": ", 8) == 0) {
            if(sscanf(p + 8, "%d", &code) == 1) {
                return code;
            }
        }
    }
    return -1;
}

static uint8_t aliyun_property_post_reply_code_seen(int code)
{
    if(code == 200) {
        return 1u;
    }
    /* code==0 多为二进制帧误匹配，继续等待；仅 >0 且非 200 视为明确拒绝 */
    if(code > 0 && code != 200) {
        return 2u;
    }
    return 0u;
}

/* 0=失败(无 reply 或 code!=200)，1=成功(code==200) */
static uint8_t aliyun_prop_reply_scan_rx_win(void)
{
    const char *p;

#if (MODEM_USE_NATIVE_MQTT != 0)
    wf24_poll_mqtt_urc();
#endif
    if(s_prop_post_reply_ok != 0u) {
        return 1u;
    }
    if(strstr(s_rx_win, "post_reply") != NULL) {
        if(aliyun_thing_model_code_from_buf(s_rx_win, s_rx_win_pos) == 200) {
            s_prop_post_reply_ok = 1u;
            return 1u;
        }
    }
    p = strstr(s_rx_win, "\"code\":200");
    if(p == NULL) {
        p = strstr(s_rx_win, "\"code\": 200");
    }
    if(p != NULL &&
       (strstr(s_rx_win, "post_reply") != NULL ||
        strstr(s_rx_win, "thing.event.property.post") != NULL)) {
        s_prop_post_reply_ok = 1u;
        return 1u;
    }
    return 0u;
}

static void aliyun_prop_reply_ensure_subscribed(void)
{
    if(s_prop_reply_sub_sent != 0u || s_prop_reply_topic[0] == '\0') {
        return;
    }
    if(cloud_aliyun_at_mqtt_subscribe_qos0(s_prop_reply_topic, NULL) != 0u) {
        s_prop_reply_sub_sent = 1u;
#if (MODEM_USE_NATIVE_MQTT != 0)
        {
            uint8_t round;
            for(round = 0u; round < 8u; round++) {
                cloud_aliyun_at_pump_mqtt_ctrl();
            }
        }
#endif
    }
}

static uint8_t aliyun_property_post_result(uint32_t wait_ms)
{
    uint32_t t0 = HAL_GetTick();
    uint8_t resp[384];
    uint16_t rlen = 0u;
    int code;
    uint8_t seen;

    s_prop_post_awaiting = 1u;
    while((HAL_GetTick() - t0) < wait_ms) {
        if(aliyun_prop_reply_scan_rx_win() != 0u) {
            s_prop_post_reply_ok = 0u;
            s_prop_post_awaiting = 0u;
            return 1u;
        }
        if(s_prop_post_reply_ok != 0u) {
            s_prop_post_reply_ok = 0u;
            s_prop_post_awaiting = 0u;
            return 1u;
        }
        cloud_aliyun_at_pump_mqtt_ctrl();
        uart2_pump_rx_core(16u, 1u);
        code = aliyun_thing_model_code_from_buf(s_rx_win, s_rx_win_pos);
        seen = aliyun_property_post_reply_code_seen(code);
        if(seen == 1u) {
            s_prop_post_awaiting = 0u;
            return 1u;
        }
        if(seen == 2u) {
#if (APP_CLOUD_TRACE != 0)
            {
                char tr[40];
                (void)snprintf(tr, sizeof(tr), "[UNLOCK] pub code=%d\r\n", code);
                cloud_trace_tx(tr);
            }
#endif
            s_prop_post_awaiting = 0u;
            return 0u;
        }

        if(uart2_recv_ipd_payload(resp, sizeof(resp), &rlen, 50u) != 0) {
            (void)aliyun_ntp_handle_ipd(resp, rlen);
            if(s_prop_post_reply_ok != 0u) {
                s_prop_post_reply_ok = 0u;
                s_prop_post_awaiting = 0u;
                return 1u;
            }
            code = aliyun_thing_model_code_from_buf((const char *)resp, rlen);
            seen = aliyun_property_post_reply_code_seen(code);
            if(seen == 1u) {
                s_prop_post_awaiting = 0u;
                return 1u;
            }
            if(seen == 2u) {
#if (APP_CLOUD_TRACE != 0)
                {
                    char tr[40];
                    (void)snprintf(tr, sizeof(tr), "[UNLOCK] pub code=%d\r\n", code);
                    cloud_trace_tx(tr);
                }
#endif
                s_prop_post_awaiting = 0u;
                return 0u;
            }
        }
    }
    CLOUD_TRACE_MSG("[UNLOCK] pub no reply\r\n");
    s_prop_post_awaiting = 0u;
    return 2u;
}

static uint8_t aliyun_unlock_publish_uart_idle(void)
{
    if(app_wifi_connect_busy() != 0u) {
        return 0u;
    }
    if(app_wifi_scan_busy() != 0u || app_wifi_scan_has_pending() != 0u) {
        return 0u;
    }
    if(cloud_aliyun_at_cwlap_scan_async_active() != 0u) {
        return 0u;
    }
    if(cloud_aliyun_at_user_wifi_join_active() != 0u ||
       cloud_aliyun_at_wifi_bringup_active() != 0u) {
        return 0u;
    }
#if (APP_ALIYUN_SNTP_ENABLE == 1)
    if(s_ntp_req_pending != 0u) {
        return 0u;
    }
#if (APP_WIFI_MODEM_WF24 != 0)
    if(s_wf24_sntp_active != 0u) {
        return 0u;
    }
#endif
#endif
    if(s_prop_post_awaiting != 0u) {
        return 0u;
    }
    return 1u;
}

void cloud_aliyun_at_time_sync_done(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
#if (APP_WIFI_MODEM_WF24 != 0)
    s_wf24_sntp_active = 0u;
    s_wf24_sntp_retry_ms = 0u;
#endif
    s_sntp_synced = 1u;
    s_ntp_synced = 1u;
    s_ntp_sub_sess = 0u;
    s_ntp_sub_ok = 0u;
    s_ntp_req_pending = 0u;
    s_ntp_give_up = 1u;
    cloud_aliyun_at_invalidate_unlock_flush();
#else
    (void)0;
#endif
}

void cloud_aliyun_at_invalidate_unlock_flush(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    s_unlock_flush_done = 0u;
    s_unlock_flush_last_ms = 0u;
#else
    (void)0;
#endif
}

static void aliyun_unlock_flush_if_due(uint8_t force_now)
{
    uint32_t now = HAL_GetTick();

    if(app_unlock_flash_count() == 0u) {
        if(s_unlock_flush_done == 0u) {
            s_unlock_flush_done = 1u;
        }
        return;
    }
    if(force_now == 0u && (now - s_unlock_flush_last_ms) < ALIYUN_UNLOCK_FLUSH_RETRY_MS) {
        return;
    }
    s_unlock_flush_last_ms = now;
    cloud_ota_service_flush_unlock_pending();
    if(app_unlock_flash_count() == 0u) {
        s_unlock_flush_done = 1u;
    }
}

uint8_t cloud_aliyun_at_property_post_await(uint32_t wait_ms)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return aliyun_property_post_result(wait_ms);
#else
    (void)wait_ms;
    return 0u;
#endif
}

uint8_t cloud_aliyun_at_publish_property_send(const char *json_payload)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    uint8_t pkt[512];
    uint16_t pkt_len;

    if(json_payload == NULL || json_payload[0] == '\0') {
        return 0u;
    }
    if(s_step != ALIYUN_STEP_ONLINE) {
        CLOUD_TRACE_MSG("[UNLOCK] pub skip not online\r\n");
        return 0u;
    }
    s_prop_post_reply_ok = 0u;
    aliyun_prop_reply_ensure_subscribed();
    if(aliyun_unlock_publish_uart_idle() == 0u) {
        LOG_ESSENTIAL("[UNLOCK] pub skip uart busy\r\n");
        return 0u;
    }
    aliyun_mqtt_uart_prep();

#if (MODEM_USE_NATIVE_MQTT != 0)
    if(uart2_mutex_take(800u) == 0u) {
        LOG_ESSENTIAL("[UNLOCK] pub skip mutex busy\r\n");
        return 0u;
    }
    if(wf24_mqtt_publish_raw(s_prop_post_topic,
                              (const uint8_t *)json_payload,
                              (uint16_t)strlen(json_payload)) == 0u) {
        uart2_mutex_give();
        CLOUD_TRACE_MSG("[UNLOCK] pub send fail\r\n");
        return 0u;
    }
    uart2_mutex_give();
#else
    pkt_len = build_mqtt_publish_qos0(pkt, sizeof(pkt), s_prop_post_topic, json_payload);
    if(pkt_len == 0u) {
        CLOUD_TRACE_MSG("[UNLOCK] pub build fail\r\n");
        return 0u;
    }
    if(mqtt_send_packet(pkt, pkt_len) == 0u) {
        CLOUD_TRACE_MSG("[UNLOCK] pub send fail\r\n");
        return 0u;
    }
#endif
    uart2_pump_rx_core(60u, 1u);
#if (MODEM_USE_NATIVE_MQTT != 0)
    {
        uint32_t t_pump = HAL_GetTick();
        while((HAL_GetTick() - t_pump) < 600u) {
            if(aliyun_prop_reply_scan_rx_win() != 0u) {
                break;
            }
            uart2_pump_rx_core(20u, 1u);
        }
    }
#endif
    if(s_ntp_synced == 0u) {
        (void)aliyun_ntp_try_parse_any();
    }
    CLOUD_TRACE_MSG("[UNLOCK] pub sent\r\n");
    return 1u;
#else
    (void)json_payload;
    return 0u;
#endif
}

uint8_t cloud_aliyun_at_publish_property(const char *json_payload)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    if(cloud_aliyun_at_publish_property_send(json_payload) == 0u) {
        return 0u;
    }
    if(aliyun_property_post_result(2500u) == 0u) {
        CLOUD_TRACE_MSG("[UNLOCK] pub fail\r\n");
        return 0u;
    }
    {
        uint8_t dump[32];
        uint16_t dump_len = 0u;
        (void)uart2_recv_ipd_payload(dump, sizeof(dump), &dump_len, 80u);
        (void)dump_len;
    }
    CLOUD_TRACE_MSG("[UNLOCK] pub ok\r\n");
    return 1u;
#else
    (void)json_payload;
    return 0u;
#endif
}

uint8_t cloud_aliyun_at_ntp_give_up(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return s_ntp_give_up;
#else
    return 0u;
#endif
}

void cloud_aliyun_at_pump_mqtt_ctrl(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    uint8_t rx[384];
    uint16_t rx_len = 0u;
    uint8_t round;

    if(s_step != ALIYUN_STEP_ONLINE) {
        return;
    }
#if (MODEM_USE_NATIVE_MQTT != 0)
    wf24_poll_mqtt_urc();
#endif
    for(round = 0u; round < 8u; round++) {
        uart2_pump_rx_core(40u, 1u);
        rx_len = 0u;
        if(uart2_recv_ipd_payload(rx, sizeof(rx), &rx_len, 40u) != 0 && rx_len > 0u) {
            (void)aliyun_ntp_handle_ipd(rx, rx_len);
        }
        {
            uint16_t suback_pkt = 0u;
            if(aliyun_mqtt_suback_pkt_id_in_buf((const uint8_t *)s_rx_win, s_rx_win_pos, &suback_pkt) != 0u) {
#if (APP_CLOUD_COMMAND_ENABLE != 0)
                aliyun_dispatch_mqtt_suback(suback_pkt);
#else
                if(s_ntp_sub_sess != 0u && s_ntp_sub_ok == 0u && suback_pkt == s_ntp_sub_pkt_id) {
                    s_ntp_sub_ok = 1u;
                }
#endif
            }
        }
    }
#endif
}

uint8_t cloud_aliyun_at_mqtt_suback_matches(uint16_t pkt_id)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    uint16_t got = 0u;
    if(pkt_id == 0u) {
        return 0u;
    }
    if(aliyun_mqtt_suback_pkt_id_in_buf((const uint8_t *)s_rx_win, s_rx_win_pos, &got) != 0u &&
       got == pkt_id) {
        return 1u;
    }
    return 0u;
#else
    (void)pkt_id;
    return 0u;
#endif
}

void cloud_aliyun_at_cmd_diag_log(const char *tag)
{
    (void)tag;
}

void cloud_aliyun_at_cmd_diag_rx_hex(void)
{
}

uint8_t cloud_aliyun_at_mqtt_publish_qos0(const char *topic, const char *json_payload)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    uint8_t pkt[512];
    uint16_t pkt_len;

    if(topic == NULL || json_payload == NULL || json_payload[0] == '\0') {
        return 0u;
    }
    if(s_step != ALIYUN_STEP_ONLINE) {
        return 0u;
    }
    aliyun_mqtt_uart_prep();
#if (MODEM_USE_NATIVE_MQTT != 0)
    {
        uint8_t ok;

        if(uart2_mutex_take(800u) == 0u) {
            return 0u;
        }
        ok = wf24_mqtt_publish_raw(topic,
                                   (const uint8_t *)json_payload,
                                   (uint16_t)strlen(json_payload));
        uart2_mutex_give();
        return ok;
    }
#else
    pkt_len = build_mqtt_publish_qos0(pkt, sizeof(pkt), topic, json_payload);
    if(pkt_len == 0u) {
        return 0u;
    }
    return mqtt_send_packet(pkt, pkt_len);
#endif
#else
    (void)topic;
    (void)json_payload;
    return 0u;
#endif
}

uint16_t cloud_aliyun_at_mqtt_peek_pkt_id(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    return s_mqtt_pkt_id;
#else
    return 1u;
#endif
}

uint8_t cloud_aliyun_at_mqtt_subscribe_qos0(const char *topic, uint16_t *out_pkt_id)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
#if (MODEM_USE_NATIVE_MQTT != 0)
    if(topic == NULL || topic[0] == '\0') {
        return 0u;
    }
    if(cloud_aliyun_at_is_online() == 0u) {
        return 0u;
    }
    aliyun_mqtt_uart_prep();
    return wf24_mqtt_subscribe_topic(topic, out_pkt_id);
#else
    uint8_t pkt[256];
    uint16_t pkt_len;
    uint16_t pkt_id;
    uint8_t round;

    if(topic == NULL || topic[0] == '\0') {
        return 0u;
    }
    if(cloud_aliyun_at_is_online() == 0u) {
        return 0u;
    }
    aliyun_mqtt_uart_prep();
    pkt_id = s_mqtt_pkt_id++;
    if(out_pkt_id != NULL) {
        *out_pkt_id = pkt_id;
    }
    pkt_len = build_mqtt_subscribe_qos0(pkt, sizeof(pkt), pkt_id, topic);
    if(pkt_len == 0u) {
        return 0u;
    }
    if(mqtt_send_packet(pkt, pkt_len) == 0u) {
        return 0u;
    }
    for(round = 0u; round < 12u; round++) {
        cloud_aliyun_at_pump_mqtt_ctrl();
    }
    return 1u;
#endif /* MODEM_USE_NATIVE_MQTT */
#else
    (void)topic;
    (void)out_pkt_id;
    return 0u;
#endif
}

void cloud_aliyun_at_poll_5ms(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    char cmd[180];
    extern volatile app_scr_t g_app_scr;
    uint8_t on_wifi_scr = (g_app_scr == APP_SCR_11 || app_wifi_scan_ui_active() != 0u) ? 1u : 0u;
    uint8_t scr11_pre_wifi = 0u;

    if(!cloud_aliyun_at_poll_allowed(on_wifi_scr)) {
        return;
    }
    if(!s_uart2_inited) {
        if(s_user_wifi_join_st == 1u) {
            static uint32_t s_no_uart_log_ms;
            uint32_t t = HAL_GetTick();
            if(s_no_uart_log_ms == 0u || (t - s_no_uart_log_ms) >= 2000u) {
                s_no_uart_log_ms = t;
                printf("[ALIYUN] CWJAP poll skip: uart2 not inited\r\n");
            }
        }
        return;
    }

    uart2_mutex_init_once();

    /* 用户手动/自动连 WiFi：必须先停 CWLAP，不能因扫描占用直接 return */
    if(s_user_wifi_join_st == 1u) {
        if(cloud_aliyun_at_cwlap_scan_async_active() != 0u) {
            cloud_aliyun_cwlap_scan_abort();
        }
        cloud_uart2_set_ui_busy(0u);
    } else if(cloud_aliyun_at_cwlap_scan_async_active() != 0u) {
        if(app_link_guard_active() != 0u) {
            cloud_aliyun_cwlap_scan_abort();
            cloud_aliyun_at_cwlap_scan_async_abort();
        } else {
            return;
        }
    }

    /* MQTT/WiFi 建链须持锁等待；timeout=0 会导致 scr11 上永远进不了 CIPSTART */
    {
        uint32_t mux_ms = 0u;
        if(s_user_wifi_join_st == 1u || app_link_guard_mqtt() != 0u ||
           app_link_guard_wifi() != 0u ||
           cloud_aliyun_at_wifi_bringup_active() != 0u) {
            mux_ms = 500u;
        }
        if(uart2_mutex_take(mux_ms) == 0u) {
            if(s_user_wifi_join_st == 1u || app_link_guard_mqtt() != 0u) {
                static uint32_t s_mux_log_ms;
                uint32_t now = HAL_GetTick();
                if(s_mux_log_ms == 0u || (now - s_mux_log_ms) >= 2000u) {
                    s_mux_log_ms = now;
                    CLOUD_TRACE_MSG("[CLOUD] poll mutex busy\r\n");
                }
            }
            return;
        }
    }

    /* 仅「未拿到 IP + 暂停」时让出 UART；已连 WiFi 后在 WiFi 页也继续 MQTT */
    if(on_wifi_scr != 0u && s_scr11_mqtt_paused != 0u &&
       s_wifi_sta_ip_ok == 0u && s_user_wifi_join_st != 1u) {
        uart2_mutex_give(); return;
    }

    scr11_pre_wifi = (uint8_t)(on_wifi_scr != 0u &&
                               cloud_aliyun_at_wifi_link_ready() == 0u &&
                               s_wifi_sta_ip_ok == 0u);

    cloud_aliyun_at_poll_wifi_urc();
    cloud_aliyun_at_poll_wifi_ip_verify();
#if (APP_WIFI_MODEM_WF24 != 0) && (APP_ALIYUN_SNTP_ENABLE == 1)
    wf24_modem_sntp_poll();
#endif
#if (MODEM_USE_NATIVE_MQTT != 0)
    if(s_step == ALIYUN_STEP_ONLINE) {
        wf24_poll_mqtt_urc();
    }
#endif

    /* scr11 且尚未连 WiFi：让出 UART 给 CWLAP/CWJAP；模组未就绪时仍跑 AT 探活 */
    if(scr11_pre_wifi != 0u) {
        if(s_modem_at_ok == 0u && (s_step == ALIYUN_STEP_AT || s_step == ALIYUN_STEP_WIFI_IDLE)) {
            /* fall through: 启动探活失败时须在 WiFi 页恢复 ESP */
        } else if(s_user_wifi_join_st != 1u &&
           app_wifi_connect_busy() == 0u &&
           s_step != ALIYUN_STEP_CWJAP_SET &&
           s_step != ALIYUN_STEP_CIFSR) {
            uart2_mutex_give(); return;
        }
    }

    if(s_uart_ui_busy != 0u && s_user_wifi_join_st != 1u &&
       app_link_guard_mqtt() == 0u
#if (APP_ALIYUN_SNTP_ENABLE == 1)
       && cloud_aliyun_sntp_step_active() == 0u
#endif
       ) {
        uart2_mutex_give(); return;
    }
    if(app_wifi_cfg_request_reconnect() != 0u && scr11_pre_wifi == 0u &&
       s_modem_at_ok != 0u) {
        app_wifi_cfg_clear_reconnect_request();
        s_step = ALIYUN_STEP_CWJAP_SET;
        s_step_ms = HAL_GetTick();
    }
#if (APP_WIFI_MODEM_WF24 != 0)
    if(s_wifi_boot_retry_ms != 0u && HAL_GetTick() >= s_wifi_boot_retry_ms &&
       s_user_wifi_join_st != 1u && cloud_uart2_ui_busy() == 0u) {
        s_wifi_boot_retry_ms = 0u;
        app_wifi_cfg_mark_reconnect_if_saved();
        if(app_wifi_cfg_request_reconnect() != 0u) {
            s_step = ALIYUN_STEP_CWJAP_SET;
            s_step_ms = HAL_GetTick();
            s_cwjap_uart_settle_done = 0u;
            s_sta_radio_prep_done = 0u;
        }
    }
#endif
    if(s_user_wifi_join_st != 1u &&
       app_link_guard_mqtt() == 0u &&
       s_mqtt_fast_join == 0u &&
       cloud_aliyun_at_wifi_bringup_active() == 0u &&
       (HAL_GetTick() - s_step_ms) < ALIYUN_POLL_INTERVAL_MS) {
        uart2_mutex_give(); return;
    }
    s_step_ms = HAL_GetTick();
    if(s_step != s_last_step_log) {
        s_last_step_log = s_step;
        CLOUD_DBG("step=%s", aliyun_step_name(s_step));
        printf("[ALIYUN] step=%s\r\n", aliyun_step_name(s_step));
    }
#if (APP_CLOUD_CONNECT_DIAG != 0)
    cloud_connect_diag_maybe();
#endif

    switch(s_step) {
    case ALIYUN_STEP_IDLE:
        s_step = (s_modem_at_ok != 0u) ? ALIYUN_STEP_WIFI_IDLE : ALIYUN_STEP_AT;
        break;
    case ALIYUN_STEP_AT:
        s_rx_win_pos = 0u;
        s_rx_win[0] = '\0';
        if(uart2_send_text("AT\r\n") && uart2_wait_text("OK", 2000u)) {
            s_modem_at_ok = 1u;
#if (MODEM_SKIP_ATE0 != 0)
            s_step = ALIYUN_STEP_CWMODE;
#else
            s_step = ALIYUN_STEP_ATE0;
#endif
            s_at_fail_cnt = 0u;
            printf("[ALIYUN] AT OK\r\n");
        } else {
            uart2_log_rx_snapshot("AT fail");
            s_at_fail_cnt++;
#if (APP_WIFI_MODEM_WF24 != 0)
            if(s_at_fail_cnt >= 8u &&
               cloud_uart2_esp_hw_reset_active() == 0u &&
               s_uart_ui_busy == 0u) {
                uint32_t now = HAL_GetTick();
                if(s_modem_key_recover_ms == 0u ||
                   (now - s_modem_key_recover_ms) >= 20000u) {
                    s_modem_key_recover_ms = now;
                    s_at_fail_cnt = 0u;
                    if(wf24_modem_key_recover("AT dead") != 0u) {
                        s_modem_at_ok = 1u;
#if (MODEM_SKIP_ATE0 != 0)
                        s_step = ALIYUN_STEP_CWMODE;
#else
                        s_step = ALIYUN_STEP_ATE0;
#endif
                        break;
                    }
                }
            }
#endif
#if (APP_ALIYUN_BAUD_PROBE != 0)
            if(s_at_fail_cnt >= 3u) {
                if(s_uart2_baud == 115200u) uart2_set_baud(9600u);
                else uart2_set_baud(115200u);
                printf("[ALIYUN] try baud=%lu\r\n", (unsigned long)s_uart2_baud);
                s_at_fail_cnt = 0u;
            }
#endif
        }
        break;
    case ALIYUN_STEP_ATE0:
        modem_at_echo_off();
        s_step = ALIYUN_STEP_CWMODE;
        break;
    case ALIYUN_STEP_CWMODE:
        if(uart2_send_text(MODEM_AT_CWMODE_STA) && uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS)) {
#if (MODEM_SKIP_CIP_STACK != 0)
            s_step = ALIYUN_STEP_WIFI_IDLE;
#else
            s_step = ALIYUN_STEP_CIPMODE;
#endif
        } else {
            printf("[ALIYUN] CWMODE fail\r\n");
        }
        break;
    case ALIYUN_STEP_CIPMODE:
        if(uart2_send_text("AT+CIPMODE=0\r\n") && uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS)) {
            s_step = ALIYUN_STEP_CIPMUX;
            s_cipmode_fail_cnt = 0u;
        } else {
            printf("[ALIYUN] CIPMODE fail\r\n");
            s_cipmode_fail_cnt++;
            if(s_cipmode_fail_cnt >= 3u) {
                /* Some AT firmware variants do not support/need CIPMODE for normal TCP. */
                printf("[ALIYUN] skip CIPMODE and continue\r\n");
                s_step = ALIYUN_STEP_CIPMUX;
                s_cipmode_fail_cnt = 0u;
            }
        }
        break;
    case ALIYUN_STEP_CIPMUX:
        if(uart2_send_text("AT+CIPMUX=1\r\n") && uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS)) {
            s_cipmux1_ok = 1u;
            s_step = ALIYUN_STEP_WIFI_IDLE;
        } else {
            printf("[ALIYUN] CIPMUX fail\r\n");
        }
        break;
    case ALIYUN_STEP_WIFI_IDLE:
        if(cloud_aliyun_at_sta_on_target_ssid() != 0u) {
            if(s_wifi_sta_ip_ok == 0u) {
                s_step = ALIYUN_STEP_CIFSR;
            } else if(cloud_aliyun_at_is_online() == 0u &&
                      app_link_guard_active() == 0u) {
                cloud_aliyun_at_mqtt_kick_after_wifi();
            }
            break;
        }
        s_wifi_sta_ip_ok = 0u;
        {
            const char *cfg_ssid = app_wifi_cfg_get_ssid();
            /* 仅在“明确请求连接”时才尝试 CWJAP。
             * - 手动/自动连接：cloud_aliyun_at_user_wifi_join_start() 会置 s_user_wifi_join_st=1
             * - 用户输入新 SSID/PWD：app_wifi_cfg_set() 会置 reconnect_req=1
             * 连接失败后必须停下，避免无限重试卡死扫描/UI。 */
            if(cfg_ssid != NULL && cfg_ssid[0] != '\0' &&
               (s_user_wifi_join_st == 1u || app_wifi_cfg_request_reconnect() != 0u)) {
                s_step = ALIYUN_STEP_CWJAP_SET;
            }
        }
        break;
    case ALIYUN_STEP_CWJAPQ:
        if(cloud_aliyun_at_sta_on_target_ssid() != 0u) {
            s_step = ALIYUN_STEP_CIFSR;
        } else {
            const char *cfg_ssid = app_wifi_cfg_get_ssid();
            if(cfg_ssid != NULL && cfg_ssid[0] != '\0') {
                s_step = ALIYUN_STEP_CWJAP_SET;
            } else {
                s_step = ALIYUN_STEP_WIFI_IDLE;
            }
        }
        break;
    case ALIYUN_STEP_CWJAP_SET:
#if (APP_WIFI_MODEM_WF24 != 0)
        if(wf24_boot_try_reuse_saved_wifi() != 0u) {
            break;
        }
#endif
        {
            const char *cfg_ssid = app_wifi_cfg_get_ssid();
            /* 非连接请求：不要在此处自发重试 */
            if((cfg_ssid == NULL || cfg_ssid[0] == '\0') ||
               (s_user_wifi_join_st != 1u && app_wifi_cfg_request_reconnect() == 0u)) {
                s_step = ALIYUN_STEP_WIFI_IDLE;
                break;
            }
        }
        {
            static uint32_t s_cwjap_defer_since = 0u;
            static uint32_t s_cwjap_defer_log_ms = 0u;
            uint8_t uart_ready;

            cloud_uart2_set_ui_busy(0u);
            if(s_user_wifi_join_st == 1u) {
                uart_ready = (cloud_uart2_ui_busy() == 0u) ? 1u : 0u;
            } else {
                uart_ready = (uint8_t)((app_wifi_scan_busy() == 0u &&
                                        cloud_aliyun_at_cwlap_scan_async_active() == 0u &&
                                        cloud_uart2_ui_busy() == 0u) ? 1u : 0u);
            }
            if(uart_ready == 0u) {
                uint32_t now = HAL_GetTick();
                if(s_cwjap_defer_since == 0u) {
                    s_cwjap_defer_since = now;
                }
                if(s_cwjap_defer_log_ms == 0u || (now - s_cwjap_defer_log_ms) >= 2000u) {
                    s_cwjap_defer_log_ms = now;
                    printf("[ALIYUN] CWJAP defer scan=%u async=%u ui=%u join=%u\r\n",
                           (unsigned)app_wifi_scan_busy(),
                           (unsigned)cloud_aliyun_at_cwlap_scan_async_active(),
                           (unsigned)cloud_uart2_ui_busy(),
                           (unsigned)s_user_wifi_join_st);
                }
                if((now - s_cwjap_defer_since) >= CWJAP_JOIN_DEFER_FAIL_MS) {
                    printf("[ALIYUN] CWJAP abort: uart busy timeout scan=%u async=%u ui=%u step=%s\r\n",
                           (unsigned)app_wifi_scan_busy(),
                           (unsigned)cloud_aliyun_at_cwlap_scan_async_active(),
                           (unsigned)cloud_uart2_ui_busy(),
                           aliyun_step_name(s_step));
                    s_cwjap_defer_since = 0u;
                    s_cwjap_defer_log_ms = 0u;
                    if(s_user_wifi_join_st == 1u) {
                        s_user_wifi_join_st = 3u;
                    }
                    s_step = ALIYUN_STEP_WIFI_IDLE;
                    break;
                }
                break;
            }
            s_cwjap_defer_since = 0u;
            s_cwjap_defer_log_ms = 0u;
        }
        if(s_cwjap_uart_settle_done == 0u) {
            if(s_cwjap_light_join != 0u && s_cwjap_light_need_prep == 0u &&
               s_cwjap_resend_only == 0u) {
                CWJAP_TRACE_MSG("[WiFi] CWJAP light quiet\r\n");
                cloud_uart2_flush_rx();
                cloud_uart2_wait_quiet_after_scan_abort(CWJAP_LIGHT_QUIET_MS);
                s_modem_at_ok = 1u;
                s_cwjap_uart_settle_done = 1u;
                s_sta_radio_prep_done = 1u;
                s_cwjap_quiet_until_ms = HAL_GetTick() + CWJAP_LIGHT_HOLD_MS;
                break;
            }
            if(s_cwjap_esp_idle_ok != 0u) {
                cwjap_uart_settle_fast();
            } else {
                CWJAP_TRACE_MSG("[WiFi] CWJAP uart settle\r\n");
                cwjap_uart_settle();
            }
            s_cwjap_uart_settle_done = 1u;
            if(s_modem_at_ok == 0u) {
                if(s_cwjap_esp_idle_ok != 0u) {
                    CWJAP_TRACE_MSG("[WiFi] CWJAP purge fail ignored (scan idle ok)\r\n");
                    s_modem_at_ok = 1u;
                } else {
                    CWJAP_TRACE_MSG("[WiFi] CWJAP purge fail\r\n");
                    if(s_user_wifi_join_st == 1u) {
                        s_user_wifi_join_st = 3u;
                    }
                    s_cwjap_esp_idle_ok = 0u;
                    s_cwjap_light_join = 0u;
                    s_step = ALIYUN_STEP_WIFI_IDLE;
                    break;
                }
            }
            s_cwjap_quiet_until_ms = HAL_GetTick() +
                (s_cwjap_esp_idle_ok != 0u ? CWJAP_ESP_IDLE_HOLD_MS : 1500u);
            break;
        }
        if(s_sta_radio_prep_done == 0u && s_cwjap_resend_only == 0u) {
            uint8_t prep_ok = 0u;

            if(s_cwjap_light_join != 0u && s_cwjap_light_need_prep == 0u) {
                prep_ok = 1u;
            } else if(s_cwjap_light_need_prep != 0u) {
                prep_ok = cwjap_scr11_min_prep();
                if(prep_ok != 0u) {
                    CWJAP_TRACE_MSG("[WiFi] CWJAP min prep ok\r\n");
                }
            } else if(s_cwjap_esp_idle_ok != 0u) {
                /* CWLAP/teardown 后已在 STA，勿再 CWQAP+CWMODE 搅乱模组 */
                CWJAP_TRACE_MSG("[WiFi] CWJAP min prep skip (scan idle ok)\r\n");
                prep_ok = 1u;
            } else {
            cloud_uart2_hw_drain_ms(150u);
            cloud_uart2_rx_clear();
            if(s_user_wifi_join_st == 1u && g_app_scr == APP_SCR_11) {
                prep_ok = cwjap_scr11_radio_prep();
            } else if(uart2_at_cmd_wait_ok("AT\r\n", 800u) &&
                      uart2_at_cmd_wait_ok(MODEM_AT_CWMODE_STA, ALIYUN_WAIT_MID_MS)
#if (MODEM_SKIP_CIP_STACK == 0)
                      && uart2_at_cmd_wait_ok("AT+CIPMUX=0\r\n", ALIYUN_WAIT_SHORT_MS) &&
                      uart2_at_cmd_wait_ok("AT+CIPSERVER=0\r\n", ALIYUN_WAIT_SHORT_MS) &&
                      uart2_at_cmd_wait_ok("AT+CIPMODE=0\r\n", ALIYUN_WAIT_SHORT_MS)
#endif
                      ) {
                prep_ok = 1u;
            }
            }
            if(prep_ok != 0u) {
                s_sta_radio_prep_done = 1u;
                s_modem_at_ok = 1u;
                s_cwjap_quiet_until_ms = HAL_GetTick() + CWJAP_PREP_HOLD_MS;
                break;
            } else {
                printf("[ALIYUN] STA radio prep fail\r\n");
                uart2_log_rx_snapshot("STA prep fail");
                if(s_user_wifi_join_st == 1u && g_app_scr == APP_SCR_11) {
                    CWJAP_TRACE_MSG("[WiFi] CWJAP prep fail, try CWJAP anyway\r\n");
                    s_sta_radio_prep_done = 1u;
                    s_modem_at_ok = 1u;
                    s_cwjap_quiet_until_ms = HAL_GetTick() + 500u;
                    break;
                }
                if(s_user_wifi_join_st == 1u) {
                    s_user_wifi_join_st = 3u;
                }
                s_cwjap_esp_idle_ok = 0u;
                s_step = ALIYUN_STEP_WIFI_IDLE;
                break;
            }
        }
        if(s_cwjap_quiet_until_ms != 0u && HAL_GetTick() < s_cwjap_quiet_until_ms) {
            break;
        }
        if(s_cwjap_resend_only == 0u &&
           (cloud_aliyun_at_wifi_joined() != 0u
#if (APP_WIFI_MODEM_WF24 != 0)
            || s_cwjap_wf24_need_cwqap != 0u
#endif
            )) {
            cloud_uart2_rx_clear();
            (void)uart2_send_text("AT+CWQAP\r\n");
            (void)uart2_wait_text("OK", 2000u);
            cloud_uart2_hw_drain_ms(200u);
#if (APP_WIFI_MODEM_WF24 != 0)
            s_cwjap_wf24_need_cwqap = 0u;
#endif
            s_cwjap_quiet_until_ms = HAL_GetTick() + CWJAP_CWQAP_HOLD_MS;
            break;
        }
        {
            uint8_t idle_before_send_ok = 0u;

            if(s_cwjap_esp_idle_ok != 0u && s_cwjap_resend_only == 0u) {
                /* 扫描 teardown 已 idle，勿再连发 AT 探活（易 busy / 超时） */
                CWJAP_TRACE_MSG("[WiFi] CWJAP idle skip probe (scan idle ok)\r\n");
                cloud_uart2_flush_rx();
                cloud_uart2_wait_quiet_after_scan_abort(CWJAP_LIGHT_BEFORE_SEND_MS);
                idle_before_send_ok = 1u;
            } else if(s_user_wifi_join_st == 0u && s_cwjap_resend_only == 0u) {
                /* 开机自动连：GitHub 版轻路径，跳过 10s 双 AT idle 探活 */
                CWJAP_TRACE_MSG("[WiFi] CWJAP boot fast prep\r\n");
                cloud_uart2_rx_clear();
                (void)uart2_at_cmd_wait_ok("AT+CWQAP\r\n", 3000u);
                cloud_uart2_hw_drain_ms(200u);
                cloud_uart2_rx_clear();
                if(uart2_at_cmd_wait_ok("AT\r\n", 1500u) != 0u &&
                   uart2_at_cmd_wait_ok(MODEM_AT_CWMODE_STA, ALIYUN_WAIT_MID_MS) != 0u) {
                    idle_before_send_ok = 1u;
                }
            } else if(s_cwjap_light_join != 0u && s_cwjap_light_need_prep == 0u &&
                      s_cwjap_resend_only == 0u) {
                CWJAP_TRACE_MSG("[WiFi] CWJAP light before send\r\n");
                cloud_uart2_wait_quiet_after_scan_abort(CWJAP_LIGHT_BEFORE_SEND_MS);
                idle_before_send_ok = 1u;
            } else {
                CWJAP_TRACE_MSG("[WiFi] CWJAP modem idle probe\r\n");
                if(cloud_uart2_wait_modem_idle_ex(10000u, 2u) != 0u) {
                    idle_before_send_ok = 1u;
                    cloud_uart2_hw_drain_ms(600u);
                } else if(uart2_at_cmd_wait_ok("AT\r\n", 2000u) != 0u) {
                    CWJAP_TRACE_MSG("[WiFi] CWJAP idle degraded-ok\r\n");
                    idle_before_send_ok = 1u;
                    cloud_uart2_hw_drain_ms(300u);
                }
            }
            if(idle_before_send_ok == 0u) {
                CWJAP_TRACE_MSG("[WiFi] CWJAP not idle before send\r\n");
                if(s_user_wifi_join_st == 1u) {
                    s_user_wifi_join_st = 3u;
                }
                s_cwjap_esp_idle_ok = 0u;
                s_cwjap_light_join = 0u;
                s_step = ALIYUN_STEP_WIFI_IDLE;
                break;
            }
        }
        s_cwjap_esp_idle_ok = 0u;

        cwjap_build_cmd(cmd, sizeof(cmd));
        CWJAP_TRACE_MSG((s_cwjap_busy_retry != 0u) ?
                        "[WiFi] CWJAP send retry\r\n" :
                        "[WiFi] CWJAP send\r\n");
        if(cwjap_send_cmd_wait(cmd, CWJAP_CONNECT_WAIT_MS) > 0) {
            const char *joined_ssid = app_wifi_cfg_get_ssid();
            if(modem_wifi_got_ip_in_rx() == 0u) {
                (void)uart2_wait_text("+CWSTATE:2", CWJAP_GOT_IP_WAIT_MS);
            }
            if(modem_wifi_got_ip_in_rx() == 0u) {
                (void)uart2_wait_text("WIFI GOT IP", 400u);
            }
            if(modem_wifi_got_ip_in_rx() == 0u) {
                printf("[ALIYUN] CWJAP no IP after connect rx=%s\r\n", s_rx_win);
                CWJAP_TRACE_MSG("[WiFi] CWJAP no IP\r\n");
                if(s_user_wifi_join_st == 1u) {
                    s_user_wifi_join_st = 3u;
                }
                s_step = ALIYUN_STEP_WIFI_IDLE;
                app_wifi_cfg_clear_reconnect_request();
                break;
            }
            CWJAP_TRACE_MSG("[WiFi] CWJAP IP ok\r\n");
            LOG_ESSENTIAL("[WiFi] connected\r\n");
            cloud_uart2_copy_cwjap_ssid(s_connected_sta_ssid, (uint16_t)sizeof(s_connected_sta_ssid));
            if(s_connected_sta_ssid[0] == '\0' && joined_ssid != NULL && joined_ssid[0] != '\0') {
                strncpy(s_connected_sta_ssid, joined_ssid, sizeof(s_connected_sta_ssid) - 1u);
                s_connected_sta_ssid[sizeof(s_connected_sta_ssid) - 1u] = '\0';
            }
            printf("[ALIYUN] WIFI connected SSID=%s sta_ssid=%s\r\n",
                   (joined_ssid != NULL) ? joined_ssid : "",
                   s_connected_sta_ssid);
            CWJAP_TRACE_MSG("[WiFi] CWJAP join ok\r\n");
            CLOUD_DBG("WIFI connected SSID=%s", joined_ssid);
            s_wifi_boot_fail_cnt = 0u;
            s_wifi_boot_retry_ms = 0u;
            app_link_guard_wifi_end(1u);
            /* WF24: CWJAP 已返回非 0 IP 时，CWSTATE 可能稍后才变 2；不要因此回退重连。 */
#if (APP_WIFI_MODEM_WF24 != 0)
            s_wifi_sta_ip_ok = 1u;
            s_wifi_ever_up = 1u;
#else
            if(aliyun_ensure_sta_ip() == 0u) {
                printf("[ALIYUN] CWJAP ok but CWSTATE!=2\r\n");
                CWJAP_TRACE_MSG("[WiFi] CWJAP no CWSTATE2\r\n");
                if(s_user_wifi_join_st == 1u) {
                    s_user_wifi_join_st = 3u;
                }
                s_step = ALIYUN_STEP_WIFI_IDLE;
                break;
            }
#endif
            s_wifi_got_ip_ms = HAL_GetTick();
            cloud_uart2_hw_drain_ms(60u);
            cloud_uart2_rx_clear();
            if(s_user_wifi_join_st == 1u) {
                s_wifi_join_fail_latch = 0u;
                s_user_wifi_join_st = 2u;
                cloud_uart2_set_ui_busy(0u);
            }
            s_cifsr_retry_cnt = 0u;
            s_cifsr_last_try_ms = 0u;
            s_cwjap_busy_retry = 0u;
            s_cwjap_esp_idle_ok = 0u;
            s_cwjap_light_join = 0u;
            s_cwjap_light_need_prep = 0u;
            if(g_app_scr == APP_SCR_11) {
                screen_wifi_notify_sta_up();
            }
#if (APP_WIFI_MODEM_WF24 != 0) && (APP_ALIYUN_SNTP_ENABLE == 1)
            /* SNTP 配置挪到 CIPSTART 等待窗口，勿阻塞 MQTT kick */
#endif
            cloud_aliyun_at_mqtt_kick_after_wifi();
            /* 本次连接请求已消费 */
            app_wifi_cfg_clear_reconnect_request();
        } else {
            if((strstr(s_rx_win, "busy p") != NULL ||
                (strstr(s_rx_win, "WIFI DISCONNECT") != NULL &&
                 strstr(s_rx_win, "WIFI GOT IP") == NULL &&
                 strstr(s_rx_win, "+CWJAP:2") == NULL)) &&
               s_cwjap_busy_retry < 1u) {
                uint8_t idle_ok;

                s_cwjap_busy_retry++;
                if(strstr(s_rx_win, "busy p") != NULL) {
                    CWJAP_TRACE_MSG("[WiFi] CWJAP retry (busy p)\r\n");
                } else {
                    CWJAP_TRACE_MSG("[WiFi] CWJAP retry (disconnect)\r\n");
                }
                cloud_uart2_flush_rx();
                cloud_uart2_wait_quiet_after_scan_abort(10000u);
                if(s_cwjap_light_join != 0u && s_cwjap_light_need_prep == 0u &&
                   strstr(s_rx_win, "busy p") != NULL) {
                    s_cwjap_light_need_prep = 1u;
                    s_sta_radio_prep_done = 0u;
                    s_cwjap_resend_only = 0u;
                    s_cwjap_try_channel = 1u;
                    s_cwjap_quiet_until_ms = HAL_GetTick() + 800u;
                    s_step = ALIYUN_STEP_CWJAP_SET;
                    s_step_ms = HAL_GetTick();
                    break;
                }
                idle_ok = cloud_uart2_wait_modem_idle_ex(12000u, 2u);
                if(idle_ok == 0u && s_cwjap_busy_retry >= 2u) {
                    CWJAP_TRACE_MSG("[WiFi] CWJAP soft RST\r\n");
                    cloud_uart2_rx_clear();
                    (void)uart2_send_text("AT+RST\r\n");
                    (void)cloud_uart2_wait_esp_ready(6000u);
                    cloud_uart2_flush_rx();
                    CWJAP_TRACE_MSG("[WiFi] CWJAP RST wait done\r\n");
                    idle_ok = cloud_uart2_wait_modem_idle(20000u);
                }
                if(idle_ok == 0u) {
                    CWJAP_TRACE_MSG("[WiFi] CWJAP post-RST idle fail\r\n");
                    if(s_user_wifi_join_st == 1u) {
                        s_user_wifi_join_st = 3u;
                    }
                    s_cwjap_busy_retry = 0u;
                    s_cwjap_esp_idle_ok = 0u;
                    s_step = ALIYUN_STEP_WIFI_IDLE;
                    app_wifi_cfg_clear_reconnect_request();
                    break;
                }
                if(s_cwjap_busy_retry >= 1u) {
                    s_cwjap_try_channel = 1u;
                }
                s_cwjap_esp_idle_ok = 0u;
                s_cwjap_resend_only = 1u;
                s_cwjap_uart_settle_done = 1u;
                s_sta_radio_prep_done = 1u;
                s_cwjap_quiet_until_ms = HAL_GetTick() + 2000u;
                s_step = ALIYUN_STEP_CWJAP_SET;
                s_step_ms = HAL_GetTick();
                break;
            }
            printf("[ALIYUN] CWJAP connect fail\r\n");
            cwjap_trace_fail_reason();
            CWJAP_TRACE_MSG("[WiFi] CWJAP fail\r\n");
            uart2_log_rx_snapshot("CWJAP fail");
            printf("[ALIYUN] CWJAP fail summary step=%s join=%u ui_busy=%u\r\n",
                   aliyun_step_name(s_step),
                   (unsigned)s_user_wifi_join_st,
                   (unsigned)s_uart_ui_busy);
            cloud_aliyun_at_wifi_join_fail_finalize();
        }
        break;
    case ALIYUN_STEP_CIFSR:
        if(s_cifsr_retry_cnt > 0u &&
           (HAL_GetTick() - s_cifsr_last_try_ms) < CIFSR_RETRY_DELAY_MS) {
            break;
        }
        s_cifsr_last_try_ms = HAL_GetTick();
#if (APP_WIFI_MODEM_WF24 != 0)
        if(aliyun_ensure_sta_ip() != 0u) {
#else
        if(uart2_send_text("AT+CIFSR\r\n") &&
           (uart2_wait_text("STAIP", ALIYUN_WAIT_MID_MS) || uart2_wait_text("+CIFSR:STAIP", ALIYUN_WAIT_MID_MS))) {
            if(strstr(s_rx_win, "0.0.0.0") != NULL) {
                s_wifi_sta_ip_ok = 0u;
                s_cifsr_retry_cnt++;
                break;
            }
#endif
            s_cifsr_retry_cnt = 0u;
            s_wifi_sta_ip_ok = 1u;
            s_wifi_ever_up = 1u;
            s_wifi_got_ip_ms = HAL_GetTick();
            LOG_ESSENTIAL("[WiFi] connected\r\n");
            if(s_connected_sta_ssid[0] == '\0') {
                const char *cfg_ssid = app_wifi_cfg_get_ssid();
                if(cfg_ssid != NULL && cfg_ssid[0] != '\0') {
                    strncpy(s_connected_sta_ssid, cfg_ssid, sizeof(s_connected_sta_ssid) - 1u);
                    s_connected_sta_ssid[sizeof(s_connected_sta_ssid) - 1u] = '\0';
                }
            }
            if(s_user_wifi_join_st == 1u) {
                s_wifi_join_fail_latch = 0u;
                s_user_wifi_join_st = 2u;
                cloud_uart2_set_ui_busy(0u);
            }
#if (APP_WIFI_MODEM_WF24 != 0) && (APP_ALIYUN_SNTP_ENABLE == 1)
            /* SNTP 配置挪到 CIPSTART 等待窗口，勿阻塞 MQTT kick */
#endif
            cloud_aliyun_at_mqtt_kick_after_wifi();
#if (APP_WIFI_MODEM_WF24 != 0)
        } else {
#else
        } else {
#endif
            s_wifi_sta_ip_ok = 0u;
            s_cifsr_retry_cnt++;
            if(s_cifsr_retry_cnt < CIFSR_RETRY_MAX) {
#if (APP_WIFI_MODEM_WF24 != 0)
                {
                    uint8_t st = 0u;
                    char ssid_live[40];
                    if(wf24_query_cwstate(&st, ssid_live, (uint16_t)sizeof(ssid_live)) != 0u) {
                        printf("[ALIYUN] no STA IP CWSTATE=%u ssid='%s' retry %u/%u\r\n",
                               (unsigned)st, ssid_live,
                               (unsigned)s_cifsr_retry_cnt, (unsigned)CIFSR_RETRY_MAX);
                    } else {
                        printf("[ALIYUN] no STA IP (CWSTATE?) retry %u/%u\r\n",
                               (unsigned)s_cifsr_retry_cnt, (unsigned)CIFSR_RETRY_MAX);
                    }
                }
#else
                printf("[ALIYUN] no STA IP (CIFSR) retry %u/%u\r\n",
                       (unsigned)s_cifsr_retry_cnt, (unsigned)CIFSR_RETRY_MAX);
#endif
                s_step = ALIYUN_STEP_CIFSR;
            } else {
                printf("[ALIYUN] no STA IP (CIFSR) give up\r\n");
                s_cifsr_retry_cnt = 0u;
                if(s_user_wifi_join_st == 1u) {
                    s_user_wifi_join_st = 3u;
                    cloud_uart2_set_ui_busy(0u);
                }
                s_step = ALIYUN_STEP_WIFI_IDLE;
            }
        }
        break;
#if (APP_ALIYUN_SNTP_ENABLE == 1)
    case ALIYUN_STEP_SNTP_CFG:
        if(aliyun_ensure_sta_ip() == 0u) {
            break;
        }
        snprintf(cmd, sizeof(cmd), "AT+CIPSNTPCFG=1,%d,\"ntp1.aliyun.com\",\"cn.ntp.org.cn\"\r\n",
                 APP_ALIYUN_SNTP_TZ_HOURS);
        if(uart2_send_text(cmd) && uart2_wait_text("OK", ALIYUN_SNTP_CMD_OK_MS)) {
            s_sntp_time_updated_seen = 0u;
            s_sntp_time_updated_ms = 0u;
            s_sntp_query_start_ms = 0u;
            s_sntp_wait_until_ms = HAL_GetTick() + ALIYUN_SNTP_POSTCFG_DELAY_MS;
            s_sntp_try_cnt = 0u;
            s_sntp_empty_cnt = 0u;
            s_sntp_next_query_ms = s_sntp_wait_until_ms;
            s_sntp_deadline_ms = s_sntp_wait_until_ms + 15000u;
            s_step = ALIYUN_STEP_SNTP_WAIT;
            TIME_TRACE_MSG("[TIME] ESP SNTP cfg ok\r\n");
        } else {
            TIME_TRACE_MSG("[TIME]EcX\r\n");
            aliyun_sntp_give_up_and_mqtt_kick();
        }
        break;
    case ALIYUN_STEP_SNTP_WAIT:
        (void)cloud_uart2_pump_rx(20u);
        if(strstr(s_rx_win, "TIME_UPDATED") != NULL) {
            s_sntp_time_updated_seen = 1u;
            if(s_sntp_time_updated_ms == 0u) {
                s_sntp_time_updated_ms = HAL_GetTick();
            }
        }
        if(s_sntp_time_updated_seen != 0u &&
           (int32_t)(HAL_GetTick() - (int32_t)s_sntp_wait_until_ms) >= 0) {
            s_step = ALIYUN_STEP_SNTP_TIME;
        } else if((int32_t)(HAL_GetTick() - (int32_t)s_sntp_deadline_ms) >= 0) {
            TIME_TRACE_MSG("[TIME] SNTP wait timeout\r\n");
            aliyun_sntp_give_up_and_mqtt_kick();
        }
        break;
    case ALIYUN_STEP_SNTP_TIME:
        if((int32_t)(HAL_GetTick() - (int32_t)s_sntp_next_query_ms) >= 0) {
            s_sntp_next_query_ms = HAL_GetTick() + 1000u;
            if(aliyun_try_fetch_sntp_once()) {
                cloud_aliyun_at_mqtt_kick_after_wifi();
            } else {
                s_sntp_try_cnt++;
                if(s_sntp_try_cnt >= ALIYUN_SNTP_MAX_TRY) {
                    TIME_TRACE_MSG("[TIME]Skm\r\n");
                    aliyun_sntp_give_up_and_mqtt_kick();
                }
            }
        }
        if((int32_t)(HAL_GetTick() - (int32_t)s_sntp_deadline_ms) >= 0) {
            TIME_TRACE_MSG("[TIME]Eto\r\n");
            aliyun_sntp_give_up_and_mqtt_kick();
        }
        break;
#endif
    case ALIYUN_STEP_CIPSTART:
#if (APP_ALIYUN_SNTP_ENABLE == 1)
        if(aliyun_want_esp_sntp_before_mqtt() != 0u) {
            TIME_TRACE_MSG("[TIME] ESP SNTP before MQTT\r\n");
            s_step = ALIYUN_STEP_SNTP_CFG;
            break;
        }
#endif
        if(app_wifi_policy_may_run_mqtt() == 0u) {
            s_step = ALIYUN_STEP_WIFI_IDLE;
            CLOUD_TRACE_MSG("[CLOUD] MQTT blocked no ip\r\n");
            break;
        }
        if(s_cipstart_next_ms != 0u && HAL_GetTick() < s_cipstart_next_ms) {
#if (APP_WIFI_MODEM_WF24 != 0) && (APP_ALIYUN_SNTP_ENABLE == 1)
            /* WiFi 已连、MQTT 尚未建链：预配 SNTP，online 后首查更快 */
            if(s_sntp_cfg_sent == 0u && s_sntp_synced == 0u && s_ntp_synced == 0u &&
               app_wall_clock_valid() == 0u) {
                wf24_sntp_cfg_once_before_mqtt();
            }
#endif
            break;
        }
#if (APP_CLOUD_FAST_AFTER_WIFI_JOIN != 0)
        if(s_mqtt_fast_join != 0u) {
#if (MODEM_USE_NATIVE_MQTT == 0)
            CLOUD_TRACE_MSG("[CLOUD] CIPSTART fast\r\n");
#endif
        } else
#endif
        {
            CLOUD_TRACE_MSG("[CLOUD] CIPSTART run\r\n");
        }
#if (APP_WIFI_MODEM_WF24 != 0)
        /* WF24: 若 CWJAP 已拿到非 0 IP（s_wifi_sta_ip_ok=1），不要再硬卡 CWSTATE==2。
         * 否则会出现“IP ok 但 CWSTATE 短暂为 0/1”导致自我阻塞、反复掉线重试变慢。
         * 真正没网时 MQTTCONN 会返回 ERROR=102 / +MQTTDISCONNECTED，由 MQTT 重试兜底。 */
        if(s_wifi_sta_ip_ok == 0u) {
            uint8_t st = 0u;
            if(wf24_query_cwstate(&st, NULL, 0u) == 0u || st != 2u) {
                printf("[ALIYUN] CIPSTART wait WiFi: CWSTATE=%u\r\n", (unsigned)st);
                CLOUD_TRACE_MSG("[CLOUD] MQTT blocked no ip\r\n");
                s_step = ALIYUN_STEP_WIFI_IDLE;
                break;
            }
            s_wifi_sta_ip_ok = 1u;
        }
#else
        if(s_wifi_sta_ip_ok == 0u && aliyun_ensure_sta_ip() == 0u) {
            printf("[ALIYUN] CIPSTART blocked: no STA IP\r\n");
            CLOUD_TRACE_MSG("[CLOUD] MQTT TCP fail (no STA IP)\r\n");
            s_step = ALIYUN_STEP_CWJAP_SET;
            s_sta_radio_prep_done = 0u;
            break;
        }
#endif
#if (MODEM_USE_NATIVE_MQTT != 0)
        if(s_wf24_mqtt_st == WF24_MQTT_ST_IDLE) {
            cloud_uart2_rx_clear();
        }
#else
        cloud_uart2_rx_clear();
#endif
#if (MODEM_USE_NATIVE_MQTT != 0)
        {
            uint8_t wf_r = 0u;
            uint8_t wf_spin;

            for(wf_spin = 0u; wf_spin < 10u; wf_spin++) {
                wf_r = wf24_mqtt_connect_step(cmd, sizeof(cmd));
                if(wf_r != 0u) {
                    break;
                }
                if(s_wf24_mqtt_st == WF24_MQTT_ST_WAIT) {
                    break;
                }
            }

            if(wf_r == 0u) {
                break;
            }
            if(wf_r == 2u) {
                printf("[ALIYUN] WF24 MQTT connect fail\r\n");
                CLOUD_TRACE_MSG("[CLOUD] MQTT WF24 fail\r\n");
                if(wf24_rx_mqtt_fatal_error() != 0u) {
                    LOG_ESSENTIAL("[CLOUD] MQTT WF24 fail (modem ERROR)\r\n");
                } else {
                    LOG_ESSENTIAL("[CLOUD] MQTT WF24 fail (retry)\r\n");
                }
                uart2_log_rx_snapshot("WF24 MQTT fail");
                s_cipstart_next_ms = HAL_GetTick() + ALIYUN_CIPSTART_RETRY_MS;
                s_step = ALIYUN_STEP_CIPSTART;
                break;
            }
            s_cipsend_retry_cnt = 0u;
            aliyun_mqtt_uart_prep();
            s_cipstart_next_ms = HAL_GetTick() + ALIYUN_POST_TCP_MQTT_MS;
            s_wf24_mqtt_conn_ok = 1u;
            s_step = ALIYUN_STEP_WAIT_CONNACK;
            printf("[ALIYUN] WF24 MQTT connected URC ok\r\n");
            CLOUD_TRACE_MSG("[CLOUD] MQTT WF24 ok\r\n");
            break;
        }
#else
#if (APP_CLOUD_FAST_AFTER_WIFI_JOIN != 0)
        if(s_mqtt_fast_join != 0u && s_wifi_sta_ip_ok != 0u) {
            aliyun_mqtt_close_link0();
            if(s_cipmux1_ok == 0u) {
                if(!(uart2_send_text("AT+CIPMUX=1\r\n") && uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS))) {
                    printf("[ALIYUN] CIPMUX=1 fast fail\r\n");
                    CLOUD_TRACE_MSG("[CLOUD] MQTT TCP fail (CIPMUX)\r\n");
                    s_cipstart_next_ms = HAL_GetTick() + ALIYUN_CIPSTART_RETRY_MS;
                    break;
                }
                s_cipmux1_ok = 1u;
            }
        } else
#endif
        {
            (void)uart2_send_text("AT+CIPCLOSE=0\r\n");
            (void)uart2_wait_text("OK", 200u);
            (void)uart2_send_text("AT+CIPCLOSE=1\r\n");
            (void)uart2_wait_text("OK", 200u);
            cloud_uart2_drain_until_idle(ALIYUN_MQTT_DRAIN_MAX_MS, ALIYUN_MQTT_DRAIN_QUIET_MS);
            if(!(uart2_send_text("AT+CIPMUX=1\r\n") && uart2_wait_text("OK", ALIYUN_WAIT_MID_MS))) {
                printf("[ALIYUN] CIPMUX=1 for MQTT fail\r\n");
                CLOUD_TRACE_MSG("[CLOUD] MQTT TCP fail (CIPMUX)\r\n");
                s_cipstart_next_ms = HAL_GetTick() + ALIYUN_CIPSTART_RETRY_MS;
                break;
            }
            s_cipmux1_ok = 1u;
            cloud_uart2_drain_until_idle(60u, ALIYUN_MQTT_DRAIN_QUIET_MS);
        }
        snprintf(cmd, sizeof(cmd), "AT+CIPSTART=0,\"TCP\",\"%s\",1883\r\n", s_host);
        if(uart2_send_text(cmd) &&
           (uart2_wait_text("CONNECT", ALIYUN_CIPSTART_WAIT_MS) ||
            uart2_wait_text("ALREADY CONNECTED", ALIYUN_WAIT_MID_MS))) {
            s_cipsend_retry_cnt = 0u;
            aliyun_mqtt_uart_prep();
#if (APP_CLOUD_FAST_AFTER_WIFI_JOIN != 0)
            s_cipstart_next_ms = HAL_GetTick() +
                ((s_mqtt_fast_join != 0u) ? ALIYUN_POST_TCP_MQTT_FAST_MS : ALIYUN_POST_TCP_MQTT_MS);
#else
            s_cipstart_next_ms = HAL_GetTick() + ALIYUN_POST_TCP_MQTT_MS;
#endif
            s_step = ALIYUN_STEP_CIPSEND;
            printf("[ALIYUN] TCP connected\r\n");
            CLOUD_TRACE_MSG("[CLOUD] MQTT TCP ok\r\n");
            CLOUD_DBG("TCP connected");
        } else {
            printf("[ALIYUN] CIPSTART fail\r\n");
            if(aliyun_rx_has_no_ip() != 0u) {
                CLOUD_TRACE_MSG("[CLOUD] MQTT TCP fail (no ip)\r\n");
            } else if(strstr(s_rx_win, "DNS") != NULL) {
                CLOUD_TRACE_MSG("[CLOUD] MQTT TCP fail (DNS)\r\n");
            } else {
                CLOUD_TRACE_MSG("[CLOUD] MQTT TCP fail\r\n");
            }
            uart2_log_rx_snapshot("CIPSTART fail");
            s_cipstart_next_ms = HAL_GetTick() + ALIYUN_CIPSTART_RETRY_MS;
            if(aliyun_rx_has_no_ip() != 0u) {
                printf("[ALIYUN] CIPSTART no ip -> recover WiFi\r\n");
                s_wifi_sta_ip_ok = 0u;
                s_mqtt_fast_join = 0u;
                s_step = ALIYUN_STEP_CWJAP_SET;
                s_sta_radio_prep_done = 0u;
            } else {
#if (APP_CLOUD_FAST_AFTER_WIFI_JOIN != 0)
                if(s_wifi_sta_ip_ok != 0u) {
                    s_mqtt_fast_join = 1u;
                }
#endif
                s_step = ALIYUN_STEP_CIPSTART;
            }
        }
#endif /* MODEM_USE_NATIVE_MQTT */
        break;
    case ALIYUN_STEP_CIPSEND:
#if (MODEM_USE_NATIVE_MQTT != 0)
        s_step = ALIYUN_STEP_WAIT_CONNACK;
        break;
#else
        if(s_password[0] == '\0' || s_mqtt_connect_pkt_len == 0u) {
            printf("[ALIYUN] MQTT password empty or pkt invalid\r\n");
            CLOUD_TRACE_MSG("[CLOUD] MQTT cfg invalid\r\n");
            s_step = ALIYUN_STEP_WIFI_IDLE;
            break;
        }
        if(s_cipstart_next_ms != 0u && HAL_GetTick() < s_cipstart_next_ms) {
            break;
        }
        s_cipstart_next_ms = 0u;
        aliyun_mqtt_uart_prep();
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%u\r\n", (unsigned)s_mqtt_connect_pkt_len);
        if(uart2_send_text(cmd) && uart2_wait_text(">", ALIYUN_CIPSEND_PROMPT_MS)) {
            s_cipsend_retry_cnt = 0u;
            CLOUD_TRACE_MSG("[CLOUD] MQTT CIPSEND ok\r\n");
            if(uart2_send_bytes(s_mqtt_connect_pkt, s_mqtt_connect_pkt_len) &&
               uart2_wait_text("SEND OK", ALIYUN_WAIT_MID_MS)) {
                CLOUD_TRACE_MSG("[CLOUD] MQTT CONNECT sent\r\n");
                s_step = ALIYUN_STEP_WAIT_CONNACK;
            } else {
                printf("[ALIYUN] send mqtt connect fail\r\n");
                CLOUD_TRACE_MSG("[CLOUD] MQTT send fail\r\n");
                aliyun_mqtt_close_link0();
#if (APP_CLOUD_FAST_AFTER_WIFI_JOIN != 0)
                if(s_wifi_sta_ip_ok != 0u) {
                    s_mqtt_fast_join = 1u;
                }
#endif
                s_cipstart_next_ms = HAL_GetTick() + ALIYUN_MQTT_RECONNECT_MS;
                s_step = ALIYUN_STEP_CIPSTART;
            }
        } else {
            printf("[ALIYUN] CIPSEND prompt fail\r\n");
            s_cipsend_retry_cnt++;
            if(s_cipsend_retry_cnt < ALIYUN_CIPSEND_RETRY_MAX) {
                CLOUD_TRACE_MSG("[CLOUD] MQTT CIPSEND retry\r\n");
                aliyun_mqtt_uart_prep();
                s_cipstart_next_ms = HAL_GetTick() + ALIYUN_CIPSEND_RETRY_MS;
                break;
            }
            s_cipsend_retry_cnt = 0u;
            CLOUD_TRACE_MSG("[CLOUD] MQTT CIPSEND fail\r\n");
            aliyun_mqtt_close_link0();
#if (APP_CLOUD_FAST_AFTER_WIFI_JOIN != 0)
            if(s_wifi_sta_ip_ok != 0u) {
                s_mqtt_fast_join = 1u;
            }
#endif
            s_cipstart_next_ms = HAL_GetTick() + ALIYUN_MQTT_RECONNECT_MS;
            s_step = ALIYUN_STEP_CIPSTART;
        }
#endif /* MODEM_USE_NATIVE_MQTT */
        break;
    case ALIYUN_STEP_SEND_CONNECT:
        /* 兼容旧状态：与 CIPSEND 合并为一步，此处仅转发 */
        s_step = ALIYUN_STEP_CIPSEND;
        break;
    case ALIYUN_STEP_WAIT_CONNACK:
#if (MODEM_USE_NATIVE_MQTT != 0)
        wf24_poll_mqtt_urc();
        if(s_wf24_mqtt_conn_ok != 0u ||
           wf24_mqtt_connected_in_rx() != 0u) {
            s_wf24_mqtt_conn_ok = 0u;
            /* fall through to online */
        } else if(strstr(s_rx_win, "ERROR") != NULL && wf24_rx_mqtt_fatal_error() != 0u) {
            printf("[ALIYUN] WF24 MQTT connect fail\r\n");
            CLOUD_TRACE_MSG("[CLOUD] MQTT WF24 CONNACK fail\r\n");
            s_cloud_scan_kick_done = 0u;
            aliyun_mqtt_close_link0();
            s_cipstart_next_ms = HAL_GetTick() + ALIYUN_MQTT_RECONNECT_MS;
            s_step = ALIYUN_STEP_CIPSTART;
            break;
        } else if(s_wf24_mqtt_wait_ms != 0u &&
                  (HAL_GetTick() - s_wf24_mqtt_wait_ms) > 15000u) {
            if(uart2_send_text("AT+MQTTCONN?\r\n") &&
               uart2_wait_text("+MQTTCONN:", ALIYUN_WAIT_MID_MS) &&
               strstr(s_rx_win, "+MQTTCONN:1,") != NULL) {
                s_wf24_mqtt_wait_ms = 0u;
                /* fall through to online */
            } else {
                printf("[ALIYUN] WF24 MQTT WAIT_CONNACK timeout\r\n");
                uart2_log_rx_snapshot("WF24 WAIT_CONNACK");
                s_wf24_mqtt_wait_ms = 0u;
                wf24_mqtt_reset_state();
                s_cipstart_next_ms = HAL_GetTick() + ALIYUN_MQTT_RECONNECT_MS;
                s_step = ALIYUN_STEP_CIPSTART;
                break;
            }
        } else {
            if(s_wf24_mqtt_wait_ms == 0u) {
                s_wf24_mqtt_wait_ms = HAL_GetTick();
            }
            break;
        }
        if(1)
#else
        if(aliyun_wait_mqtt_connack(ALIYUN_CONNACK_WAIT_MS) != 0u)
#endif
        {
            s_step = ALIYUN_STEP_ONLINE;
            s_mqtt_fast_join = 0u;
            s_ping_fail_streak = 0u;
            s_last_ping_ms = HAL_GetTick();
            s_mqtt_online_ms = HAL_GetTick();
            s_last_mqtt_activity_ms = HAL_GetTick();
#if (APP_ALIYUN_SNTP_ENABLE == 1)
            s_sntp_online_query_ms = 0u;
            s_ntp_connect_tried = 1u;
#endif
            printf("[ALIYUN] MQTT connected (CONNACK OK) -> cloud online\r\n");
            CLOUD_TRACE_MSG("[CLOUD] MQTT online ok\r\n");
            LOG_ESSENTIAL("[CLOUD] online\r\n");
#if (APP_WIFI_MODEM_WF24 != 0) && (APP_ALIYUN_SNTP_ENABLE == 1)
            if(wf24_modem_sntp_online_fast_sync() == 0u) {
                (void)aliyun_ntp_try_sync_online();
            }
#endif
            aliyun_prop_reply_ensure_subscribed();
#if (APP_WIFI_MODEM_WF24 != 0) && (APP_ALIYUN_SNTP_ENABLE == 1)
            if(s_sntp_synced == 0u && s_ntp_synced == 0u && app_wall_clock_valid() == 0u &&
               s_wf24_sntp_active == 0u) {
                wf24_modem_sntp_kick();
            }
#endif
            aliyun_ntp_reset_bridge_wait();
#if (APP_CLOUD_COMMAND_ENABLE != 0)
            app_cloud_command_reset();
#endif
            /* 勿清空物模型重试队列：桥接已发出时会导致主机本地开锁阿里云缺记录 */
            cloud_ota_service_on_mqtt_online();
#if (APP_ALIYUN_SNTP_ENABLE == 1)
            if(s_ntp_connect_tried == 0u) {
                s_ntp_connect_tried = 1u;
            }
#endif
            s_ntp_sub_sess = 0u;
            s_ntp_sub_ok = 0u;
            s_ntp_sub_pkt_id = 0u;
            s_ntp_sync_running = 0u;
            s_ntp_next_request_ms = 0u;
            s_ntp_await_until_ms = 0u;
            s_ntp_req_pending = 0u;
            s_ntp_req_sent_ms = 0u;
            s_ntp_mqtt_fail_cnt = 0u;
#if (APP_TIME_TRACE != 0)
            s_ntp_suback_logged = 0u;
            s_ntp_pend_diag_ms = 0u;
#endif
            if(s_sntp_synced != 0u || app_wall_clock_valid() != 0u) {
                s_ntp_synced = 1u;
            }
            s_unlock_flush_done = 0u;
            s_unlock_flush_last_ms = 0u;
            s_prop_reply_sub_sent = 0u;
            s_prop_post_reply_ok = 0u;
            aliyun_unlock_flush_if_due(1u);
            app_link_guard_mqtt_end(1u);
            CLOUD_DBG("MQTT connected -> cloud online");
        } else {
            printf("[ALIYUN] CONNACK invalid\r\n");
            CLOUD_TRACE_MSG("[CLOUD] MQTT CONNACK fail\r\n");
            CLOUD_DBG("CONNACK invalid");
            s_cloud_scan_kick_done = 0u;
            aliyun_mqtt_close_link0();
#if (APP_CLOUD_FAST_AFTER_WIFI_JOIN != 0)
            if(s_wifi_sta_ip_ok != 0u) {
                s_mqtt_fast_join = 1u;
            }
#endif
            s_cipstart_next_ms = HAL_GetTick() + ALIYUN_MQTT_RECONNECT_MS;
            s_step = ALIYUN_STEP_CIPSTART;
        }
        break;
    case ALIYUN_STEP_ONLINE:
        /* MQTT 在线：wf24_modem_sntp_poll 在 poll 入口；此处再跑 MQTT-NTP 备用 */
        if(s_ntp_synced == 0u && s_ntp_give_up == 0u &&
           s_mqtt_online_ms != 0u &&
           (HAL_GetTick() - s_mqtt_online_ms) >= 45000u) {
            s_ntp_give_up = 1u;
            CLOUD_TRACE_MSG("[CLOUD] NTP give up -> terminal/get\r\n");
        }
        if(s_ntp_synced == 0u) {
            aliyun_ntp_try_sync_online();
        }
        if(s_unlock_flush_done == 0u || app_unlock_flash_count() > 0u) {
            aliyun_unlock_flush_if_due(0u);
        }
        if(s_ntp_synced == 0u && s_ntp_await_until_ms != 0u &&
           (int32_t)(HAL_GetTick() - (int32_t)s_ntp_await_until_ms) < 0) {
            break;
        }
        /* 校时完成前不发 MQTT Ping，避免与 NTP 抢 UART2 */
        if(s_ntp_synced != 0u) {
            if((HAL_GetTick() - s_mqtt_online_ms) < 2000u) {
                break;
            }
            if((HAL_GetTick() - s_last_ping_ms) >= ALIYUN_KEEPALIVE_PING_MS) {
                uint32_t idle_ms = HAL_GetTick() - s_last_mqtt_activity_ms;
                s_last_ping_ms = HAL_GetTick();
                if(s_last_mqtt_activity_ms != 0u && idle_ms < 28000u) {
                    s_ping_fail_streak = 0u;
                    break;
                }
                if(!aliyun_send_ping_and_check()) {
                    s_ping_fail_streak++;
                    if(s_ping_fail_streak < 3u) {
                        break;
                    }
                    s_ping_fail_streak = 0u;
                    printf("[ALIYUN] ping timeout/reconnect MQTT\r\n");
                    CLOUD_DBG("ping timeout/reconnect MQTT");
                    s_cloud_scan_kick_done = 0u;
#if (APP_CLOUD_COMMAND_ENABLE != 0)
                    app_cloud_command_publish_offline_best_effort();
#endif
                    aliyun_mqtt_close_link0();
#if (APP_CLOUD_COMMAND_ENABLE != 0)
                    app_cloud_command_reset();
#endif
                    s_ntp_sub_sess = 0u;
                    s_ntp_sub_ok = 0u;
                    s_ntp_sub_pkt_id = 0u;
                    if(app_wifi_policy_may_run_mqtt() != 0u) {
#if (APP_CLOUD_FAST_AFTER_WIFI_JOIN != 0)
                        if(s_wifi_sta_ip_ok != 0u) {
                            s_mqtt_fast_join = 1u;
                        }
#endif
                        s_cipstart_next_ms = HAL_GetTick() + ALIYUN_MQTT_RECONNECT_MS;
                        s_step = ALIYUN_STEP_CIPSTART;
                    } else {
                        s_step = ALIYUN_STEP_WIFI_IDLE;
                    }
                } else {
                    s_ping_fail_streak = 0u;
                }
            }
        }
        break;
    default:
        break;
    }
    /* 仅在 MQTT 在线时冲刷 deferred 开锁，避免 CWJAP/CWLAP 期间抢 UART2 */
    if(s_step == ALIYUN_STEP_ONLINE) {
        cloud_ota_service_flush_deferred_unlock();
    }
    uart2_mutex_give();
#endif
}
