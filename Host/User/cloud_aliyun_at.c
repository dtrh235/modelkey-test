#include "cloud_aliyun_at.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_wifi_cfg.h"
#include "app_wifi_policy.h"
#include "app_wifi_scan.h"
#include "app_link_guard.h"
#include "cloud_ota_service.h"

uint8_t app_cloud_session_busy(void);
#include "app_wifi_diag.h"
#include "app_screen.h"
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
static void aliyun_mqtt_uart_prep(void);
static void cloud_uart2_wait_quiet_after_scan_abort(uint32_t quiet_ms);
static void cloud_uart2_flush_rx(void);
static uint8_t cloud_uart2_wait_modem_idle(uint32_t timeout_ms);
static uint8_t uart2_rx_has_ok_reply(void);
static void cwjap_modem_purge(uint8_t hard_reset);
static uint8_t cloud_uart2_wait_esp_ready(uint32_t timeout_ms);
static uint8_t cwjap_scr11_radio_prep(void);
static uint8_t uart2_at_cmd_wait_ok(const char *cmd, uint32_t timeout_ms);

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
static uint8_t s_cipmode_fail_cnt = 0u;
static aliyun_step_t s_step = ALIYUN_STEP_IDLE;
static uint32_t s_step_ms = 0u;

static char s_host[96];
static char s_client_id[96];
static char s_user_name[96];
static char s_password[96];
static char s_prop_post_topic[128];
static char s_ntp_req_topic[96];
static char s_ntp_resp_topic[96];
static uint8_t s_mqtt_connect_pkt[256];
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
    s_uart2_mutex = xSemaphoreCreateMutex();
    configASSERT(s_uart2_mutex != NULL);
}

/* 持有互斥量期间调用；timeout=0 表示不等待直接返回失败 */
static uint8_t uart2_mutex_take(uint32_t timeout_ms)
{
    if(s_uart2_mutex == NULL) return 1u;
    return (xSemaphoreTake(s_uart2_mutex,
                           (timeout_ms == 0u) ? 0u : pdMS_TO_TICKS(timeout_ms)) == pdTRUE) ? 1u : 0u;
}

static void uart2_mutex_give(void)
{
    if(s_uart2_mutex != NULL) {
        xSemaphoreGive(s_uart2_mutex);
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
#if (APP_ALIYUN_SNTP_ENABLE == 1)
static uint32_t s_sntp_wait_until_ms = 0u;
static uint8_t s_sntp_try_cnt = 0u;
static uint8_t s_sntp_empty_cnt = 0u;
static uint32_t s_sntp_next_query_ms = 0u;
static uint32_t s_sntp_deadline_ms = 0u;
static uint8_t s_sntp_synced = 0u;
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
static uint8_t s_ntp_sub_sess = 0u;
static uint8_t s_ntp_give_up = 0u;
static uint32_t s_ntp_next_try_ms = 0u;
static uint8_t s_ntp_sync_running = 0u;
static uint8_t s_ntp_connect_tried = 0u;
static uint8_t s_ping_fail_streak = 0u;
static uint16_t s_mqtt_pkt_id = 1u;
static uint8_t s_uart_ui_busy = 0u;
static uint8_t s_modem_at_ok = 0u;
static uint8_t s_cipmux1_ok = 0u;
static uint8_t s_mqtt_fast_join = 0u;
static uint8_t s_cipsend_retry_cnt = 0u;
static uint8_t s_scr11_mqtt_paused = 0u;
static aliyun_step_t s_scr11_resume_step = ALIYUN_STEP_IDLE;
static uint8_t s_wifi_sta_ip_ok = 0u;
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
static uint8_t s_cloud_scan_kick_done = 0u;
static uint8_t s_cifsr_retry_cnt = 0u;
static uint32_t s_cifsr_last_try_ms = 0u;
#define CIFSR_RETRY_MAX       20u
#define CIFSR_RETRY_DELAY_MS  400u
#define CWJAP_GOT_IP_WAIT_MS    5000u
#define CWJAP_CONNECT_WAIT_MS   18000u
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
static uint8_t aliyun_ntp_try_parse_any(void);
static uint8_t aliyun_ntp_apply_from_mqtt_chunk(const uint8_t *chunk, uint16_t len);

static uint8_t cwlap_scan_should_stop(void)
{
    return (g_wifi_scan_abort != 0u || g_wifi_exclusive == 0u) ? 1u : 0u;
}

#define ALIYUN_ESP_RST_PORT GPIOA
#define ALIYUN_ESP_RST_PIN  GPIO_PIN_8
#define ALIYUN_KEEPALIVE_PING_MS 20000u
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
#define ALIYUN_MQTT_NTP_RSP_WAIT_MS   15000u
#define ALIYUN_MQTT_NTP_SUB_SETTLE_MS 2000u
#define ALIYUN_MQTT_NTP_RETRY_ONLINE_MS 60000u
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
            s_sntp_synced = 1u;
            s_ntp_synced = 1u;
            TIME_TRACE_MSG("[TIME] sync ok (HTTP date)\r\n");
            cloud_ota_service_flush_unlock_pending();
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
    if(year >= 2020 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
        app_home_wall_clock_set(year, month, day, hh, mm, ss);
        s_sntp_synced = 1u;
        s_ntp_synced = 1u;
        TIME_TRACE_MSG("[TIME] sync ok (ESP SNTP)\r\n");
        cloud_ota_service_flush_unlock_pending();
        return 1u;
    }
    return 0u;
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

        if(sntp_parse_epoch_digits(tail) != 0u) {
            return 1u;
        }
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
        if(sscanf(p, "+CIPSNTPTIME:%11s %3s %d %d:%d:%d %d", mon, &day, &hh, &mm, &ss, &year) == 6 ||
           sscanf(p, "+CIPSNTPTIME:%d-%d-%d,%d:%d:%d", &year, &month, &day, &hh, &mm, &ss) == 6) {
            if(month < 1 || month > 12) {
                month = month_abbr_to_i(mon);
            }
            return sntp_apply_parsed_time(year, month, day, hh, mm, ss);
        }
        if(sscanf(tail, "%*3s %3s %d %d:%d:%d %d", mon, &day, &hh, &mm, &ss, &year) == 6) {
            month = month_abbr_to_i(mon);
            return sntp_apply_parsed_time(year, month, day, hh, mm, ss);
        }
        if(sscanf(tail, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hh, &mm, &ss) == 6) {
            return sntp_apply_parsed_time(year, month, day, hh, mm, ss);
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
    TIME_TRACE_MSG("[TIME]Ebd\r\n");
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

    cloud_uart2_rx_clear();
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
    TIME_TRACE_MSG("[TIME]Ebd\r\n");
    return 0u;
}
#endif

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
            printf("[ALIYUN] CONNACK denied code=%u\r\n", (unsigned)buf[i + 3u]);
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

static uint8_t mqtt_send_packet(const uint8_t *pkt, uint16_t pkt_len)
{
    char cmd[40];
    uint8_t try;

    if(pkt == NULL || pkt_len == 0u) {
        return 0u;
    }
    for(try = 0u; try < 2u; try++) {
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%u\r\n", (unsigned)pkt_len);
        if(uart2_send_text(cmd) && uart2_wait_text(">", ALIYUN_WAIT_MID_MS) &&
           uart2_send_bytes(pkt, pkt_len) &&
           uart2_wait_text("SEND OK", ALIYUN_WAIT_MID_MS)) {
            return 1u;
        }
        (void)cloud_uart2_pump_rx(40u);
    }
    return 0u;
}

static void esp_rst_gpio_init_once(void)
{
    GPIO_InitTypeDef gpio = {0};
    if(s_rst_gpio_inited) return;
    __HAL_RCC_GPIOA_CLK_ENABLE();
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
    if(strstr(s_rx_win, "+CWJAP:1") != NULL) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: timeout\r\n");
    } else if(strstr(s_rx_win, "+CWJAP:2") != NULL) {
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
    } else if(cloud_uart2_rx_has("+CWJAP:") != 0) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: auth/assoc fail\r\n");
    } else if(cloud_uart2_rx_has("FAIL") != 0) {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: FAIL\r\n");
    } else {
        CWJAP_TRACE_MSG("[WiFi] CWJAP reason: timeout\r\n");
    }
    cwjap_trace_rx_tail();
}

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

static int uart2_wait_wifi_sta_connected(uint32_t timeout_ms)
{
    uint8_t ch = 0u;
    uint32_t start = HAL_GetTick();

    if(timeout_ms == 0u) {
        return 0;
    }
    /* 勿在此 clear：会擦掉 CWJAP 刚返回的 busy p / FAIL */
    while((HAL_GetTick() - start) < timeout_ms) {
        if(uart2_read_byte_timeout(&ch, 1u) != 0) {
            if(strstr(s_rx_win, "WIFI GOT IP") != NULL) {
                return 1;
            }
            if(strstr(s_rx_win, "+CWJAP:") != NULL ||
               strstr(s_rx_win, "FAIL") != NULL ||
               strstr(s_rx_win, "busy p") != NULL ||
               strstr(s_rx_win, "no ap") != NULL ||
               strstr(s_rx_win, "NO AP") != NULL) {
                printf("[ALIYUN] CWJAP wait fail token rx=%s\r\n", s_rx_win);
                return -1;
            }
        }
        uart2_coop_yield();
    }
    if(strstr(s_rx_win, "WIFI GOT IP") != NULL) {
        return 1;
    }
    if(strstr(s_rx_win, "+CWJAP:") != NULL ||
       strstr(s_rx_win, "FAIL") != NULL ||
       strstr(s_rx_win, "busy p") != NULL ||
       strstr(s_rx_win, "no ap") != NULL ||
       strstr(s_rx_win, "NO AP") != NULL) {
        printf("[ALIYUN] CWJAP wait fail(last) rx=%s\r\n", s_rx_win);
        return -1;
    }
    if(strstr(s_rx_win, "WIFI DISCONNECT") != NULL &&
       strstr(s_rx_win, "WIFI GOT IP") == NULL &&
       strstr(s_rx_win, "WIFI CONNECTED") == NULL) {
        printf("[ALIYUN] CWJAP wait disconnect rx=%s\r\n", s_rx_win);
        return -1;
    }
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

static uint8_t aliyun_send_ping_and_check(void)
{
    static const uint8_t ping_pkt[2] = {0xC0u, 0x00u}; /* MQTT PINGREQ */
    uint8_t resp[192];
    uint16_t rlen = 0u;
    uint16_t i;
    uint32_t deadline;

    if(!uart2_send_text("AT+CIPSEND=0,2\r\n")) return 0u;
    if(!uart2_wait_text(">", ALIYUN_WAIT_MID_MS)) return 0u;
    if(!uart2_send_bytes(ping_pkt, 2u)) return 0u;
    if(!uart2_wait_text("SEND OK", ALIYUN_WAIT_MID_MS)) return 0u;

    /* 先解析 NTP 应答，勿把 +IPD 当 ping 直接 consume 掉 */
    deadline = HAL_GetTick() + ALIYUN_WAIT_MID_MS;
    while((int32_t)(HAL_GetTick() - deadline) < 0) {
        uart2_drain_rx_ms(40u);
        if(s_ntp_synced == 0u && aliyun_ntp_try_parse_any() != 0u) {
            return 1u;
        }
        if(!uart2_recv_ipd_payload(resp, sizeof(resp), &rlen, 60u)) {
            continue;
        }
        if(s_ntp_synced == 0u && aliyun_ntp_apply_from_mqtt_chunk(resp, rlen) != 0u) {
            return 1u;
        }
        for(i = 0u; (uint16_t)(i + 1u) < rlen; i++) {
            if(resp[i] == 0xD0u && resp[i + 1u] == 0x00u) {
                return 1u; /* MQTT PINGRESP */
            }
        }
    }
    return 0u;
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

static uint16_t build_mqtt_connect(uint8_t *out, uint16_t out_sz, const char *client_id, const char *user_name, const char *password)
{
    uint8_t vh[16];
    uint16_t vh_len = 0u;
    uint8_t payload[220];
    uint16_t payload_len = 0u;
    uint16_t rem_len;
    uint16_t pos = 0u;

    if(out == NULL || client_id == NULL || user_name == NULL || password == NULL) return 0u;

    vh_len += mqtt_write_utf8(vh + vh_len, "MQTT");
    vh[vh_len++] = 0x04u;
    vh[vh_len++] = 0xC2u;
    vh[vh_len++] = 0x00u;
    vh[vh_len++] = 50u;

    payload_len += mqtt_write_utf8(payload + payload_len, client_id);
    payload_len += mqtt_write_utf8(payload + payload_len, user_name);
    payload_len += mqtt_write_utf8(payload + payload_len, password);

    rem_len = (uint16_t)(vh_len + payload_len);
    if(rem_len > 127u) return 0u;
    if((uint16_t)(rem_len + 2u) > out_sz) return 0u;

    out[pos++] = 0x10u;
    out[pos++] = (uint8_t)rem_len;
    memcpy(out + pos, vh, vh_len);
    pos = (uint16_t)(pos + vh_len);
    memcpy(out + pos, payload, payload_len);
    pos = (uint16_t)(pos + payload_len);
    return pos;
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
        TIME_TRACE_MSG("[TIME]Mbd\r\n");
        return 0u;
    }
    if(epoch_to_local_datetime(accurate_ms / 1000u, APP_ALIYUN_SNTP_TZ_HOURS, &y, &mo, &d, &h, &mi, &s) == 0u) {
        TIME_TRACE_MSG("[TIME]Mbd\r\n");
        return 0u;
    }
    if(y < 2020) {
        TIME_TRACE_MSG("[TIME]Mby\r\n");
        return 0u;
    }
    app_home_wall_clock_set(y, mo, d, h, mi, s);
    s_ntp_synced = 1u;
    s_sntp_synced = 1u;
    s_ntp_await_until_ms = 0u;
    s_ntp_give_up = 0u;
    TIME_TRACE_MSG("[TIME] sync ok (MQTT NTP)\r\n");
    cloud_ota_service_flush_unlock_pending();
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

/* 扫描 s_rx_win 中全部 +IPD；仅 NTP 解析成功才 consume（避免 SUBACK 吃掉应答） */
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
        if(aliyun_ntp_apply_from_mqtt_chunk(chunk, len) != 0u) {
            rx_win_consume((uint16_t)(hdr_end + len));
            return 1u;
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
    if(strstr(s_rx_win, "serverSendTime") != NULL &&
       aliyun_ntp_apply_from_buf((const uint8_t *)s_rx_win, s_rx_win_pos) != 0u) {
        return 1u;
    }
    return aliyun_ntp_try_all_ipd_in_rx_win();
}

/* r110 同款：MQTT 在线后非阻塞 NTP（Msb→Mrq→收 +IPD→Mok），不阻塞 UI */
static void aliyun_ntp_try_sync_online(void)
{
    uint8_t pkt[256];
    uint16_t pkt_len;
    uint8_t rx[384];
    uint16_t rx_len = 0u;
    static const char req_payload[] = "{\"id\":\"1\",\"version\":\"1.0\",\"params\":{}}";

    if(s_ntp_synced != 0u || app_wall_clock_valid() != 0u) {
        s_ntp_synced = 1u;
        return;
    }
    if(s_ntp_connect_tried == 0u) {
        return;
    }

    uart2_drain_rx_ms(30u);
    if(aliyun_ntp_try_parse_any() != 0u) {
        return;
    }

    if(s_ntp_sub_sess == 0u) {
        pkt_len = build_mqtt_subscribe_qos0(pkt, sizeof(pkt), s_mqtt_pkt_id++, s_ntp_resp_topic);
        if(pkt_len == 0u || mqtt_send_packet(pkt, pkt_len) == 0u) {
            return;
        }
        s_ntp_sub_sess = 1u;
        TIME_TRACE_MSG("[TIME]Msb\r\n");
    }

    if(s_ntp_next_request_ms == 0u ||
       (int32_t)(HAL_GetTick() - (int32_t)s_ntp_next_request_ms) >= (int32_t)ALIYUN_MQTT_NTP_RETRY_ONLINE_MS) {
        pkt_len = build_mqtt_publish_qos0(pkt, sizeof(pkt), s_ntp_req_topic, req_payload);
        if(pkt_len > 0u && mqtt_send_packet(pkt, pkt_len) != 0u) {
            s_ntp_req_dev_ms = HAL_GetTick();
            s_ntp_next_request_ms = HAL_GetTick();
            TIME_TRACE_MSG("[TIME]Mrq\r\n");
        }
    }

    if(uart2_recv_ipd_payload(rx, sizeof(rx), &rx_len, 20u) != 0) {
        (void)aliyun_ntp_apply_from_mqtt_chunk(rx, rx_len);
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
    snprintf(s_ntp_req_topic, sizeof(s_ntp_req_topic), "/ext/ntp/%s/%s/request",
             APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME);
    snprintf(s_ntp_resp_topic, sizeof(s_ntp_resp_topic), "/ext/ntp/%s/%s/response",
             APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME);
    s_mqtt_connect_pkt_len = build_mqtt_connect(s_mqtt_connect_pkt, sizeof(s_mqtt_connect_pkt),
                                                s_client_id, s_user_name, s_password);
}

void cloud_aliyun_at_init(void)
{
    uart2_mutex_init_once();
#if (APP_ALIYUN_AT_ENABLE == 1)
    esp_rst_gpio_init_once();
    esp_rst_set(0u);
    HAL_Delay(500u);
    esp_rst_set(1u);
    HAL_Delay(3000u);
    uart2_init_once();
    s_rx_win_pos = 0u;
    s_rx_win[0] = '\0';
    uart2_drain_rx_ms(1500u);
#if (APP_HOST_SILENCE_ALIYUN_TERMINAL == 0)
    uart2_log_rx_snapshot("after ESP boot");
#endif
    aliyun_prepare_params();
    s_step = ALIYUN_STEP_IDLE;
    s_wifi_ever_up = 0u;
    s_wifi_sta_ip_ok = 0u;
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
    s_sntp_retry_online_ms = HAL_GetTick();
    s_http_date_tried = 0u;
#endif
    s_ntp_synced = 0u;
    s_ntp_next_request_ms = 0u;
    s_mqtt_pkt_id = 1u;
    s_modem_at_ok = 0u;
    memset(s_rx_win, 0, sizeof(s_rx_win));
    s_rx_win_pos = 0u;
    if(uart2_send_text("AT\r\n") && uart2_wait_text("OK", 2500u)) {
        (void)uart2_send_text("ATE0\r\n");
        (void)uart2_wait_text("OK", 800u);
        s_modem_at_ok = 1u;
        printf("[ALIYUN] init: ESP AT OK modem_ready=1\r\n");
    } else {
        printf("[ALIYUN] init: ESP AT probe fail (retry on WiFi page)\r\n");
    }
    printf("[ALIYUN] mqtt host=%s user=%s\r\n", s_host, s_user_name);
    printf("[ALIYUN] wifi SSID=%s (app_wifi_cfg)\r\n", app_wifi_cfg_get_ssid());
    printf("[ALIYUN] uart2 baud=%lu\r\n", (unsigned long)s_uart2_baud);
    printf("[ALIYUN] cloud AT/MQTT poll deferred until WiFi page or STA up\r\n");
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
    /* CWJAP 已成功、尚未拿到 IP/上 MQTT：离开 WiFi 页也要继续跑 FSM */
    if(s_step >= ALIYUN_STEP_CIFSR && s_step <= ALIYUN_STEP_WAIT_CONNACK) {
        return 1u;
    }
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

void cloud_uart2_pump_rx(uint32_t duration_ms)
{
    uint8_t ch = 0u;
    uint32_t end;

#if (APP_ALIYUN_SNTP_ENABLE == 1)
    if(cloud_aliyun_sntp_step_active() != 0u) {
        sntp_uart_pump(duration_ms);
        return;
    }
#endif
    if(s_uart_ui_busy != 0u) {
        return;
    }
    uart2_init_once();
    if(duration_ms == 0u) return;
    end = HAL_GetTick() + duration_ms;
    while((HAL_GetTick() < end) != 0u) {
        (void)uart2_read_byte_timeout(&ch, 8u);
        uart2_coop_yield();
    }
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
    cloud_uart2_drain_until_idle(60u, 10u);
    (void)uart2_send_text("AT+CIPCLOSE=0\r\n");
    (void)uart2_wait_text("OK", 150u);
    (void)uart2_wait_text("CLOSED", 100u);
    cloud_uart2_drain_until_idle(ALIYUN_MQTT_DRAIN_MAX_MS, ALIYUN_MQTT_DRAIN_QUIET_MS);
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
        (void)uart2_send_text("AT+CIPCLOSE=0\r\n");
        (void)uart2_wait_text("OK", 1000u);
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
    (void)snprintf(quoted, sizeof(quoted), "\"%s\"", cfg);
    return (strstr(s_rx_win, quoted) != NULL) ? 1u : 0u;
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
        return;
    }
    p++;
    q = strchr(p, '"');
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
    if(out == NULL || out_sz < 2u) {
        return 0u;
    }
    out[0] = '\0';
    if(cloud_aliyun_at_wifi_joined() == 0u) {
        return 0u;
    }
    if(s_connected_sta_ssid[0] != '\0') {
        strncpy(out, s_connected_sta_ssid, (size_t)(out_sz - 1u));
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
    uart2_init_once();
    if(!s_uart2_inited) {
        return 0u;
    }
    cloud_uart2_rx_clear();
    if(uart2_send_text("AT\r\n") && uart2_wait_text("OK", 2000u)) {
        s_modem_at_ok = 1u;
        return 1u;
    }
    return 0u;
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
        s_scr11_mqtt_paused = 0u;
        return;
    }
    if(s_scr11_mqtt_paused != 0u) {
        return;
    }
    if(s_step >= ALIYUN_STEP_CIPSTART) {
        s_scr11_resume_step = s_step;
        cloud_uart2_hw_drain_ms(80u);
        cloud_uart2_rx_clear();
        (void)uart2_send_text("AT+CIPCLOSE=0\r\n");
        (void)uart2_wait_text("OK", 400u);
        cloud_uart2_hw_drain_ms(40u);
        cloud_uart2_rx_clear();
        s_step = ALIYUN_STEP_CIFSR;
        s_step_ms = HAL_GetTick();
        s_scr11_mqtt_paused = 1u;
    }
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
    if(s_modem_at_ok != 0u) {
        s_step = ALIYUN_STEP_CIFSR;
    } else if(s_scr11_resume_step >= ALIYUN_STEP_CIPSTART) {
        s_step = ALIYUN_STEP_CIPSTART;
    } else {
        s_step = s_scr11_resume_step;
    }
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
        (void)uart2_send_text("ATE0\r\n");
        s_cwlap_async.deadline = HAL_GetTick() + ALIYUN_WAIT_SHORT_MS;
        s_cwlap_async.st = CA_ATE0_WAIT;
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
        if(!uart2_send_text("AT+CWMODE=1\r\n")) {
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
               strstr(s_rx_win, "Ready") != NULL) {
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
    if(uart2_at_cmd_wait_ok("AT+CWMODE=1\r\n", 5000u) == 0u) {
        cloud_uart2_hw_drain_ms(500u);
        cloud_uart2_rx_clear();
        if(uart2_at_cmd_wait_ok("AT+CWMODE=1\r\n", 5000u) == 0u) {
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
    if(uart2_at_cmd_wait_ok("AT+CWMODE=1\r\n", ALIYUN_WAIT_MID_MS) == 0u) {
        return 0u;
    }
    cloud_uart2_rx_clear();
    if(uart2_at_cmd_wait_ok("AT+CIPMUX=0\r\n", ALIYUN_WAIT_SHORT_MS) == 0u) {
        return 0u;
    }
    cloud_uart2_rx_clear();
    if(uart2_at_cmd_wait_ok("AT+CIPSERVER=0\r\n", ALIYUN_WAIT_SHORT_MS) == 0u) {
        return 0u;
    }
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
    (void)uart2_send_text("ATE0\r\n");
    (void)uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS);
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
    (void)uart2_send_text("AT+CIPCLOSE=0\r\n");
    (void)uart2_wait_text("OK", 600u);
    (void)uart2_wait_text("ERROR", 300u);
    cloud_uart2_flush_rx();
    (void)uart2_send_text("ATE0\r\n");
    (void)uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS);
    cloud_uart2_flush_rx();
}

/* 有效帧：至少一行 +CWLAP:( 且收到 OK */
static uint8_t cwlap_frame_valid(const char *dst, uint16_t pos)
{
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
    if(s_cwk.dst != NULL && strstr(s_cwk.dst, "+CWLAP:(") != NULL) {
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
            if(!(uart2_send_text("AT+CWMODE=1\r\n") && uart2_wait_text("OK", ALIYUN_WAIT_LONG_MS))) {
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
        if(strstr(dst, "+CWLAP:(") != NULL) {
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
    uart2_init_once();
    if(!s_uart2_inited) {
        return -1;
    }

    *out_len = 0u;
    /* 与 CWJAP/MQTT 互斥：CWLAP 扫描必须持有 UART2 mutex，否则 ESP8266 会 busy p... */
    uart2_mutex_init_once();
    if(uart2_mutex_take(1200u) == 0u) {
        return -2;
    }

    g_wifi_scan_abort = 0u;
    cloud_uart2_set_ui_busy(1u);
    if(cloud_uart2_modem_ready() == 0u) {
        (void)cloud_uart2_try_modem_ready();
    }
    cloud_uart2_prepare_for_cwlap_only();
    cloud_uart2_quiet_for_cwlap();

    if(cwlap_scan_should_stop() != 0u) {
        rc = -9;
        goto cwlap_out;
    }

    rc = cwlap_try_direct(dst, dst_sz, out_len);
    if(rc == -9) {
        goto cwlap_out;
    }
    if(rc == 0) {
        goto cwlap_out;
    }

    /* 直扫 3 次仍无合法 +CWLAP:( 帧：走 CWMODE 恢复后再扫一次 */
    WIFI_DBG("cwlap direct fail -> recover path");
    cloud_uart2_flush_rx();
    if(!(uart2_send_text("AT+CWMODE=1\r\n") && uart2_wait_text("OK", ALIYUN_WAIT_LONG_MS))) {
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
    s_step = ALIYUN_STEP_WIFI_IDLE;
    app_wifi_cfg_clear_reconnect_request();
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
    if(g_app_scr == APP_SCR_11 && cloud_aliyun_at_wifi_link_ready() == 0u &&
       app_link_guard_mqtt() == 0u &&
       cloud_aliyun_at_wifi_bringup_active() == 0u) {
        return 1u;
    }
#endif
    return 0u;
}

/* WiFi 已拿到 IP 后：未校时则先 ESP SNTP，再建 MQTT */
static void cloud_aliyun_at_mqtt_kick_after_wifi(void)
{
    if(app_link_guard_wifi() != 0u) {
        return;
    }
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
#if (APP_ALIYUN_SNTP_ENABLE == 1) && (APP_ALIYUN_SNTP_BEFORE_MQTT != 0)
    if(app_wall_clock_valid() == 0u && s_sntp_synced == 0u && s_http_date_tried == 0u &&
       s_step != ALIYUN_STEP_SNTP_CFG && s_step != ALIYUN_STEP_SNTP_WAIT &&
       s_step != ALIYUN_STEP_SNTP_TIME) {
        TIME_TRACE_MSG("[TIME]Ego\r\n");
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
    s_cipstart_next_ms = 0u;
#else
    s_mqtt_fast_join = 0u;
    s_cipstart_next_ms = HAL_GetTick() + 150u;
#endif
    s_step = ALIYUN_STEP_CIPSTART;
    s_step_ms = 0u;
}

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
    if(s_step >= ALIYUN_STEP_CIPSTART) {
        (void)uart2_send_text("AT+CIPCLOSE=0\r\n");
        (void)uart2_wait_text("OK", 400u);
        cloud_uart2_hw_drain_ms(40u);
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

uint8_t cloud_aliyun_at_publish_property(const char *json_payload)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    uint8_t pkt[512];
    uint16_t pkt_len;
    char cmd[40];

    if(json_payload == NULL || json_payload[0] == '\0') return 0u;
    if(s_step != ALIYUN_STEP_ONLINE) {
        CLOUD_TRACE_MSG("[CLOUD] property skip (not online)\r\n");
        return 0u;
    }

    pkt_len = build_mqtt_publish_qos0(pkt, sizeof(pkt), s_prop_post_topic, json_payload);
    if(pkt_len == 0u) return 0u;

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%u\r\n", (unsigned)pkt_len);
    if(!(uart2_send_text(cmd) && uart2_wait_text(">", ALIYUN_WAIT_MID_MS))) return 0u;
    if(!uart2_send_bytes(pkt, pkt_len)) return 0u;
    if(!uart2_wait_text("SEND OK", ALIYUN_WAIT_MID_MS)) {
        CLOUD_TRACE_MSG("[CLOUD] property post fail\r\n");
        return 0u;
    }
    CLOUD_TRACE_MSG("[CLOUD] property post ok\r\n");
    uart2_drain_rx_ms(30u);
    if(s_ntp_synced == 0u) {
        (void)aliyun_ntp_try_parse_any();
    }
    return 1u;
#else
    (void)json_payload;
    return 0u;
#endif
}

void cloud_aliyun_at_poll_5ms(void)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    char cmd[180];
    extern volatile app_scr_t g_app_scr;
    uint8_t on_wifi_scr = (g_app_scr == APP_SCR_11) ? 1u : 0u;
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

    /* scr11 且尚未连 WiFi：仅允许用户 CWJAP/CIFSR，禁止 AT 链/MQTT 与 CWLAP 抢 UART2 */
    if(scr11_pre_wifi != 0u) {
        if(s_user_wifi_join_st != 1u &&
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
    if(app_wifi_cfg_request_reconnect() != 0u && scr11_pre_wifi == 0u) {
        app_wifi_cfg_clear_reconnect_request();
        s_step = ALIYUN_STEP_CWJAP_SET;
        s_step_ms = HAL_GetTick();
    }
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

    switch(s_step) {
    case ALIYUN_STEP_AT:
        s_rx_win_pos = 0u;
        s_rx_win[0] = '\0';
        if(uart2_send_text("AT\r\n") && uart2_wait_text("OK", 2000u)) {
            s_modem_at_ok = 1u;
            s_step = ALIYUN_STEP_ATE0;
            s_at_fail_cnt = 0u;
            printf("[ALIYUN] AT OK\r\n");
        } else {
            uart2_log_rx_snapshot("AT fail");
            s_at_fail_cnt++;
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
        if(uart2_send_text("ATE0\r\n") && uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS)) {
            s_step = ALIYUN_STEP_CWMODE;
        } else {
            printf("[ALIYUN] ATE0 fail\r\n");
        }
        break;
    case ALIYUN_STEP_CWMODE:
        if(uart2_send_text("AT+CWMODE=1\r\n") && uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS)) {
            s_step = ALIYUN_STEP_CIPMODE;
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
                      uart2_at_cmd_wait_ok("AT+CWMODE=1\r\n", ALIYUN_WAIT_MID_MS) &&
                      uart2_at_cmd_wait_ok("AT+CIPMUX=0\r\n", ALIYUN_WAIT_SHORT_MS) &&
                      uart2_at_cmd_wait_ok("AT+CIPSERVER=0\r\n", ALIYUN_WAIT_SHORT_MS) &&
                      uart2_at_cmd_wait_ok("AT+CIPMODE=0\r\n", ALIYUN_WAIT_SHORT_MS)) {
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
        if(s_cwjap_resend_only == 0u && cloud_aliyun_at_wifi_joined() != 0u) {
            cloud_uart2_rx_clear();
            (void)uart2_send_text("AT+CWQAP\r\n");
            (void)uart2_wait_text("OK", 2000u);
            cloud_uart2_hw_drain_ms(200u);
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
            if(strstr(s_rx_win, "WIFI GOT IP") == NULL) {
                (void)uart2_wait_text("WIFI GOT IP", CWJAP_GOT_IP_WAIT_MS);
            }
            if(strstr(s_rx_win, "WIFI GOT IP") == NULL) {
                printf("[ALIYUN] CWJAP no GOT IP after connect rx=%s\r\n", s_rx_win);
                CWJAP_TRACE_MSG("[WiFi] CWJAP no GOT IP\r\n");
                if(s_user_wifi_join_st == 1u) {
                    s_user_wifi_join_st = 3u;
                }
                s_step = ALIYUN_STEP_WIFI_IDLE;
                app_wifi_cfg_clear_reconnect_request();
                break;
            }
            CWJAP_TRACE_MSG("[WiFi] CWJAP GOT IP\r\n");
            if(joined_ssid != NULL && joined_ssid[0] != '\0') {
                strncpy(s_connected_sta_ssid, joined_ssid, sizeof(s_connected_sta_ssid) - 1u);
                s_connected_sta_ssid[sizeof(s_connected_sta_ssid) - 1u] = '\0';
            }
            printf("[ALIYUN] WIFI connected SSID=%s\r\n", joined_ssid);
            CWJAP_TRACE_MSG("[WiFi] CWJAP join ok\r\n");
            CLOUD_DBG("WIFI connected SSID=%s", joined_ssid);
            app_link_guard_wifi_end(1u);
            /* GOT IP 即可更新 scr11「已连接」栏；CIFSR 仅作校验 */
            s_wifi_sta_ip_ok = 1u;
            s_wifi_ever_up = 1u;
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
            cloud_aliyun_at_mqtt_kick_after_wifi();
            /* 本次连接请求已消费 */
            app_wifi_cfg_clear_reconnect_request();
        } else {
            if((strstr(s_rx_win, "busy p") != NULL ||
                (strstr(s_rx_win, "WIFI DISCONNECT") != NULL &&
                 strstr(s_rx_win, "WIFI GOT IP") == NULL)) &&
               s_cwjap_busy_retry < 2u) {
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
        if(uart2_send_text("AT+CIFSR\r\n") &&
           (uart2_wait_text("STAIP", ALIYUN_WAIT_MID_MS) || uart2_wait_text("+CIFSR:STAIP", ALIYUN_WAIT_MID_MS))) {
            s_cifsr_retry_cnt = 0u;
            s_wifi_sta_ip_ok = 1u;
            s_wifi_ever_up = 1u;
            if(s_user_wifi_join_st == 1u) {
                s_wifi_join_fail_latch = 0u;
                s_user_wifi_join_st = 2u;
                cloud_uart2_set_ui_busy(0u);
            }
            cloud_aliyun_at_mqtt_kick_after_wifi();
        } else {
            s_wifi_sta_ip_ok = 0u;
            s_cifsr_retry_cnt++;
            if(s_cifsr_retry_cnt < CIFSR_RETRY_MAX) {
                printf("[ALIYUN] no STA IP (CIFSR) retry %u/%u\r\n",
                       (unsigned)s_cifsr_retry_cnt, (unsigned)CIFSR_RETRY_MAX);
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
            s_sntp_wait_until_ms = HAL_GetTick() + ALIYUN_SNTP_POSTCFG_DELAY_MS;
            s_sntp_try_cnt = 0u;
            s_sntp_empty_cnt = 0u;
            s_sntp_next_query_ms = s_sntp_wait_until_ms;
            s_sntp_deadline_ms = s_sntp_wait_until_ms + 15000u;
            s_step = ALIYUN_STEP_SNTP_WAIT;
            TIME_TRACE_MSG("[TIME]Ecf\r\n");
        } else {
            TIME_TRACE_MSG("[TIME]EcX\r\n");
            cloud_aliyun_at_mqtt_kick_after_wifi();
        }
        break;
    case ALIYUN_STEP_SNTP_WAIT:
        if((int32_t)(HAL_GetTick() - (int32_t)s_sntp_wait_until_ms) >= 0) {
            s_step = ALIYUN_STEP_SNTP_TIME;
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
                    cloud_aliyun_at_mqtt_kick_after_wifi();
                }
            }
        }
        if((int32_t)(HAL_GetTick() - (int32_t)s_sntp_deadline_ms) >= 0) {
            TIME_TRACE_MSG("[TIME]Eto\r\n");
            cloud_aliyun_at_mqtt_kick_after_wifi();
        }
        break;
#endif
    case ALIYUN_STEP_CIPSTART:
        if(app_wifi_policy_may_run_mqtt() == 0u) {
            s_step = ALIYUN_STEP_WIFI_IDLE;
            CLOUD_TRACE_MSG("[CLOUD] MQTT blocked no ip\r\n");
            break;
        }
        if(s_cipstart_next_ms != 0u && HAL_GetTick() < s_cipstart_next_ms) {
            break;
        }
#if (APP_CLOUD_FAST_AFTER_WIFI_JOIN != 0)
        if(s_mqtt_fast_join != 0u) {
            CLOUD_TRACE_MSG("[CLOUD] CIPSTART fast\r\n");
        } else
#endif
        {
            CLOUD_TRACE_MSG("[CLOUD] CIPSTART run\r\n");
        }
        if(s_wifi_sta_ip_ok == 0u && aliyun_ensure_sta_ip() == 0u) {
            printf("[ALIYUN] CIPSTART blocked: no STA IP\r\n");
            CLOUD_TRACE_MSG("[CLOUD] MQTT TCP fail (no STA IP)\r\n");
            s_step = ALIYUN_STEP_CWJAP_SET;
            s_sta_radio_prep_done = 0u;
            break;
        }
        cloud_uart2_rx_clear();
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
        break;
    case ALIYUN_STEP_CIPSEND:
        if(s_password[0] == '\0' || s_mqtt_connect_pkt_len == 0u) {
            printf("[ALIYUN] MQTT password empty or pkt invalid\r\n");
            return;
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
        break;
    case ALIYUN_STEP_SEND_CONNECT:
        /* 兼容旧状态：与 CIPSEND 合并为一步，此处仅转发 */
        s_step = ALIYUN_STEP_CIPSEND;
        break;
    case ALIYUN_STEP_WAIT_CONNACK:
        if(aliyun_wait_mqtt_connack(ALIYUN_CONNACK_WAIT_MS) != 0u) {
            s_step = ALIYUN_STEP_ONLINE;
            s_mqtt_fast_join = 0u;
            s_ping_fail_streak = 0u;
            s_last_ping_ms = HAL_GetTick();
#if (APP_ALIYUN_SNTP_ENABLE == 1)
            s_sntp_cfg_sent = 0u;
            s_sntp_online_query_ms = 0u;
#endif
            printf("[ALIYUN] MQTT connected (CONNACK OK) -> cloud online\r\n");
            CLOUD_TRACE_MSG("[CLOUD] MQTT online ok\r\n");
            s_ntp_connect_tried = 1u;
            s_ntp_sub_sess = 0u;
            s_ntp_sync_running = 0u;
            s_ntp_next_request_ms = 0u;
            if(s_sntp_synced != 0u || app_wall_clock_valid() != 0u) {
                s_ntp_synced = 1u;
            }
            cloud_ota_service_flush_unlock_pending();
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
        /* MQTT 在线时勿发 AT+CIPSNTPCFG/CIPSNTPTIME?，会打断 TCP；仅用 MQTT-NTP */
        if(s_ntp_synced == 0u) {
            aliyun_ntp_try_sync_online();
        }
        cloud_ota_service_flush_unlock_pending();
        if((HAL_GetTick() - s_last_ping_ms) >= ALIYUN_KEEPALIVE_PING_MS) {
            s_last_ping_ms = HAL_GetTick();
            if(!aliyun_send_ping_and_check()) {
                s_ping_fail_streak++;
                if(s_ping_fail_streak < 2u) {
                    break;
                }
                s_ping_fail_streak = 0u;
                printf("[ALIYUN] ping timeout/reconnect MQTT\r\n");
                CLOUD_DBG("ping timeout/reconnect MQTT");
                s_cloud_scan_kick_done = 0u;
                aliyun_mqtt_close_link0();
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
        break;
    default:
        break;
    }
    uart2_mutex_give();
#endif
}
