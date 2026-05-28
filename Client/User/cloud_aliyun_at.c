#include "cloud_aliyun_at.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#if (APP_ALIYUN_AT_ENABLE == 1) || (APP_ALIYUN_SNTP_ENABLE == 1)
#include "app_wall_clock.h"
#endif
#include "stm32f4xx_hal.h"

typedef enum {
    ALIYUN_STEP_IDLE = 0,
    ALIYUN_STEP_AT,
    ALIYUN_STEP_ATE0,
    ALIYUN_STEP_CWMODE,
    ALIYUN_STEP_CIPMODE,
    ALIYUN_STEP_CIPMUX,
    ALIYUN_STEP_CWJAPQ,
    ALIYUN_STEP_CWJAP_SET,
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
static uint32_t s_uart2_baud = 115200u;
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
static uint8_t s_ntp_subscribed = 0u;
static uint8_t s_ntp_synced = 0u;
static uint32_t s_ntp_next_request_ms = 0u;
static uint16_t s_mqtt_pkt_id = 1u;

#define ALIYUN_ESP_RST_PORT GPIOA
#define ALIYUN_ESP_RST_PIN  GPIO_PIN_8
#define ALIYUN_KEEPALIVE_PING_MS 20000u
#define ALIYUN_POLL_INTERVAL_MS 30u
#define ALIYUN_WAIT_SHORT_MS 120u
#define ALIYUN_WAIT_MID_MS 300u
#define ALIYUN_WAIT_LONG_MS 800u
#define ALIYUN_SNTP_CMD_OK_MS 800u
#define ALIYUN_SNTP_QUERY_MS 600u
#define ALIYUN_SNTP_POSTCFG_DELAY_MS 4000u
#define ALIYUN_SNTP_ONLINE_RETRY_MS 30000u
#define ALIYUN_HTTP_DATE_HOST "www.baidu.com"
#define ALIYUN_MQTT_NTP_RETRY_MS 60000u

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

static uint8_t aliyun_try_fetch_http_date_once(void)
{
    char cmd[96];
    const char *req = "HEAD / HTTP/1.1\r\nHost: " ALIYUN_HTTP_DATE_HOST "\r\nConnection: close\r\n\r\n";
    char *p;
    char wk[4];
    char mon[4];
    int d = 1, y = 0, hh = 0, mm = 0, ss = 0, mo = 1;

    memset(s_rx_win, 0, sizeof(s_rx_win));
    s_rx_win_pos = 0u;

    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=1,\"TCP\",\"%s\",80\r\n", ALIYUN_HTTP_DATE_HOST);
    if(!uart2_send_text(cmd) || !(uart2_wait_text("CONNECT", 1200u) || uart2_wait_text("ALREADY CONNECTED", 300u))) {
        return 0u;
    }

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=1,%u\r\n", (unsigned)strlen(req));
    if(!uart2_send_text(cmd) || !uart2_wait_text(">", 400u)) {
        uart2_send_text("AT+CIPCLOSE=1\r\n");
        return 0u;
    }
    if(!uart2_send_text(req) || !uart2_wait_text("SEND OK", 600u)) {
        uart2_send_text("AT+CIPCLOSE=1\r\n");
        return 0u;
    }

    if(!uart2_wait_text("Date:", 1200u)) {
        uart2_send_text("AT+CIPCLOSE=1\r\n");
        return 0u;
    }

    p = strstr(s_rx_win, "Date:");
    if(p != NULL && sscanf(p, "Date: %3s, %d %3s %d %d:%d:%d", wk, &d, mon, &y, &hh, &mm, &ss) == 7) {
        mo = month_abbr_to_i(mon);
        add_hours_to_datetime(&y, &mo, &d, &hh, APP_ALIYUN_SNTP_TZ_HOURS);
        if(y >= 2020 && mo >= 1 && mo <= 12) {
            app_home_wall_clock_set(y, mo, d, hh, mm, ss);
            s_sntp_synced = 1u;
            printf("[ALIYUN] HTTP-Date synced: %04d-%02d-%02d %02d:%02d:%02d\r\n", y, mo, d, hh, mm, ss);
            uart2_send_text("AT+CIPCLOSE=1\r\n");
            return 1u;
        }
    }
    uart2_send_text("AT+CIPCLOSE=1\r\n");
    return 0u;
}

/* Short, bounded SNTP query to avoid UI/key lag. */
static uint8_t aliyun_try_fetch_sntp_once(void)
{
    char wday[12];
    char mon[8];
    int day = 1;
    int month = 1;
    int hh = 0;
    int mm = 0;
    int ss = 0;
    int year = 0;
    char *p;

    /* Clear receive window to avoid matching stale content. */
    memset(s_rx_win, 0, sizeof(s_rx_win));
    s_rx_win_pos = 0u;
    if(!uart2_send_text("AT+CIPSNTPTIME?\r\n")) {
        printf("[ALIYUN] SNTP query send fail\r\n");
        return 0u;
    }
    if(!uart2_wait_text("+CIPSNTPTIME:", ALIYUN_SNTP_QUERY_MS)) {
        if(uart2_wait_text("ERROR", 80u)) {
            printf("[ALIYUN] SNTP query returned ERROR\r\n");
        } else {
            printf("[ALIYUN] SNTP query no time response yet\r\n");
        }
        return 0u;
    }
    p = strstr(s_rx_win, "+CIPSNTPTIME:");
    if(p == NULL) return 0u;
    printf("[ALIYUN] SNTP raw: %.80s\r\n", p);
    if(strncmp(p, "+CIPSNTPTIME:\r", 14u) == 0 || strncmp(p, "+CIPSNTPTIME:\n", 14u) == 0 ||
       strncmp(p, "+CIPSNTPTIME:", 13u) == 0) {
        s_sntp_empty_cnt++;
    } else {
        s_sntp_empty_cnt = 0u;
    }

    /* ESP AT variants may return different formats; try a few. */
    if(sscanf(p, "+CIPSNTPTIME:%11s %3s %d %d:%d:%d %d", wday, mon, &day, &hh, &mm, &ss, &year) == 7 ||
       sscanf(p, "+CIPSNTPTIME:\"%11s %3s %d %d:%d:%d %d\"", wday, mon, &day, &hh, &mm, &ss, &year) == 7) {
        month = month_abbr_to_i(mon);
    } else if(sscanf(p, "+CIPSNTPTIME:%d-%d-%d,%d:%d:%d", &year, &month, &day, &hh, &mm, &ss) == 6 ||
              sscanf(p, "+CIPSNTPTIME:\"%d-%d-%d,%d:%d:%d\"", &year, &month, &day, &hh, &mm, &ss) == 6) {
        /* already parsed as numeric Y-M-D H:M:S */
    } else {
        printf("[ALIYUN] SNTP parse failed\r\n");
        return 0u;
    }

    if(year >= 2020 && month >= 1 && month <= 12) {
        app_home_wall_clock_set(year, month, day, hh, mm, ss);
        printf("[ALIYUN] SNTP synced: %04d-%02d-%02d %02d:%02d:%02d\r\n",
               year, month, day, hh, mm, ss);
        s_sntp_synced = 1u;
        return 1u;
    }
    printf("[ALIYUN] SNTP got invalid time y=%d m=%d d=%d\r\n", year, month, day);
    return 0u;
}
#endif

static uint8_t mqtt_send_packet(const uint8_t *pkt, uint16_t pkt_len)
{
    char cmd[40];
    if(pkt == NULL || pkt_len == 0u) return 0u;
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%u\r\n", (unsigned)pkt_len);
    if(!(uart2_send_text(cmd) && uart2_wait_text(">", ALIYUN_WAIT_MID_MS))) return 0u;
    if(!uart2_send_bytes(pkt, pkt_len)) return 0u;
    return uart2_wait_text("SEND OK", ALIYUN_WAIT_MID_MS) ? 1u : 0u;
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

    while((HAL_GetTick() - start) < timeout_ms) {
        if(HAL_UART_Receive(&s_uart2, &ch, 1u, 1u) == HAL_OK) {
            if(s_rx_win_pos < (sizeof(s_rx_win) - 1u)) {
                s_rx_win[s_rx_win_pos++] = (char)ch;
            } else {
                memmove(s_rx_win, s_rx_win + 1, sizeof(s_rx_win) - 2u);
                s_rx_win[sizeof(s_rx_win) - 2u] = (char)ch;
            }
            s_rx_win[sizeof(s_rx_win) - 1u] = '\0';
            if(strstr(s_rx_win, needle) != NULL) return 1;
        }
    }

    if(strstr(s_rx_win, needle) != NULL) return 1;
    return 0;
}

static int uart2_read_byte_timeout(uint8_t *out, uint32_t timeout_ms)
{
    if(out == NULL) return 0;
    return (HAL_UART_Receive(&s_uart2, out, 1u, timeout_ms) == HAL_OK) ? 1 : 0;
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

    if(out_len != NULL) *out_len = 0u;
    if(!uart2_wait_text("+IPD,", timeout_ms)) return 0;

    /* Parse header: support both +IPD,<id>,<len>: and +IPD,<len>: */
    while((HAL_GetTick() - start) < timeout_ms) {
        if(!uart2_read_byte_timeout(&ch, 2u)) continue;
        if(ch == ':') {
            if(saw_digit) last_num = curr_num;
            break;
        }
        if(ch >= '0' && ch <= '9') {
            saw_digit = 1u;
            curr_num = (uint16_t)(curr_num * 10u + (uint16_t)(ch - '0'));
            if(curr_num > 2048u) return 0;
        } else if(ch == ',') {
            if(saw_digit) {
                last_num = curr_num;
                curr_num = 0u;
                saw_digit = 0u;
            }
        }
    }
    if(ch != ':') return 0;
    len = last_num;
    if(len == 0u) return 0;
    copy_n = (len < payload_sz) ? len : payload_sz;

    for(i = 0u; i < len; i++) {
        if(!uart2_read_byte_timeout(&ch, 20u)) return 0;
        if(i < copy_n && payload != NULL) payload[i] = ch;
    }
    if(out_len != NULL) *out_len = copy_n;
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

    /* There may be other downlink packets (e.g. NTP response) before PINGRESP. */
    deadline = HAL_GetTick() + ALIYUN_WAIT_MID_MS;
    while((int32_t)(HAL_GetTick() - deadline) < 0) {
        if(!uart2_recv_ipd_payload(resp, sizeof(resp), &rlen, 60u)) continue;
        for(i = 0u; (uint16_t)(i + 1u) < rlen; i++) {
            if(resp[i] == 0xD0u && resp[i + 1u] == 0x00u) return 1u; /* MQTT PINGRESP */
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

static uint8_t parse_ntp_server_send_time_ms(const uint8_t *buf, uint16_t len, uint64_t *out_ms)
{
    const char key[] = "serverSendTime";
    uint16_t i;
    if(buf == NULL || out_ms == NULL) return 0u;
    for(i = 0u; i + (uint16_t)sizeof(key) < len; i++) {
        if(memcmp(&buf[i], key, sizeof(key) - 1u) == 0) {
            uint16_t j = (uint16_t)(i + (uint16_t)sizeof(key) - 1u);
            uint64_t v = 0u;
            uint8_t hit_digit = 0u;
            while(j < len && (buf[j] == ' ' || buf[j] == '"' || buf[j] == ':' || buf[j] == '\t')) j++;
            while(j < len && buf[j] >= '0' && buf[j] <= '9') {
                v = v * 10u + (uint64_t)(buf[j] - '0');
                hit_digit = 1u;
                j++;
            }
            if(hit_digit) {
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
static void aliyun_ntp_try_sync_online(void)
{
    uint8_t pkt[256];
    uint16_t pkt_len;
    uint8_t rx[320];
    uint16_t rx_len = 0u;
    uint64_t ms = 0u;
    int y, mo, d, h, mi, s;
    const char req_payload[] = "{\"id\":\"1\",\"version\":\"1.0\",\"params\":{}}";

    if(s_ntp_synced) return;

    if(!s_ntp_subscribed) {
        pkt_len = build_mqtt_subscribe_qos0(pkt, sizeof(pkt), s_mqtt_pkt_id++, s_ntp_resp_topic);
        if(pkt_len > 0u && mqtt_send_packet(pkt, pkt_len)) {
            s_ntp_subscribed = 1u;
        } else {
            return;
        }
    }

    if((HAL_GetTick() - s_ntp_next_request_ms) >= ALIYUN_MQTT_NTP_RETRY_MS || s_ntp_next_request_ms == 0u) {
        pkt_len = build_mqtt_publish_qos0(pkt, sizeof(pkt), s_ntp_req_topic, req_payload);
        if(pkt_len > 0u && mqtt_send_packet(pkt, pkt_len)) {
            s_ntp_next_request_ms = HAL_GetTick();
        }
    }

    if(uart2_recv_ipd_payload(rx, sizeof(rx), &rx_len, 10u)) {
        if(parse_ntp_server_send_time_ms(rx, rx_len, &ms)) {
            if(epoch_to_local_datetime(ms / 1000u, APP_ALIYUN_SNTP_TZ_HOURS, &y, &mo, &d, &h, &mi, &s)) {
                app_home_wall_clock_set(y, mo, d, h, mi, s);
                s_ntp_synced = 1u;
                printf("[ALIYUN] MQTT-NTP synced: %04d-%02d-%02d %02d:%02d:%02d\r\n", y, mo, d, h, mi, s);
            }
        }
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
#if (APP_ALIYUN_AT_ENABLE == 1)
    esp_rst_gpio_init_once();
    esp_rst_set(0u);
    HAL_Delay(500u);
    esp_rst_set(1u);
    HAL_Delay(3000u);
    uart2_init_once();
    aliyun_prepare_params();
    s_step = ALIYUN_STEP_AT;
    s_step_ms = HAL_GetTick();
    s_last_step_log = ALIYUN_STEP_IDLE;
    s_at_fail_cnt = 0u;
    s_cipmode_fail_cnt = 0u;
    s_last_ping_ms = HAL_GetTick();
#if (APP_ALIYUN_SNTP_ENABLE == 1)
    s_sntp_synced = 0u;
    s_sntp_retry_online_ms = HAL_GetTick();
    s_http_date_tried = 0u;
#endif
    s_ntp_subscribed = 0u;
    s_ntp_synced = 0u;
    s_ntp_next_request_ms = 0u;
    s_mqtt_pkt_id = 1u;
    memset(s_rx_win, 0, sizeof(s_rx_win));
    s_rx_win_pos = 0u;
    printf("[ALIYUN] init: host=%s user=%s\r\n", s_host, s_user_name);
    printf("[ALIYUN] uart2 baud=%lu\r\n", (unsigned long)s_uart2_baud);
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

uint8_t cloud_aliyun_at_publish_property(const char *json_payload)
{
#if (APP_ALIYUN_AT_ENABLE == 1)
    uint8_t pkt[512];
    uint16_t pkt_len;
    char cmd[40];

    if(json_payload == NULL || json_payload[0] == '\0') return 0u;
    if(s_step != ALIYUN_STEP_ONLINE) return 0u;

    pkt_len = build_mqtt_publish_qos0(pkt, sizeof(pkt), s_prop_post_topic, json_payload);
    if(pkt_len == 0u) return 0u;

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%u\r\n", (unsigned)pkt_len);
    if(!(uart2_send_text(cmd) && uart2_wait_text(">", ALIYUN_WAIT_MID_MS))) return 0u;
    if(!uart2_send_bytes(pkt, pkt_len)) return 0u;
    if(!uart2_wait_text("SEND OK", ALIYUN_WAIT_MID_MS)) {
        s_step = ALIYUN_STEP_CIPSTART;
        return 0u;
    }
    /* Drain one possible downlink frame to avoid RX desync after publish. */
    {
        uint8_t dump[16];
        uint16_t dump_len = 0u;
        (void)uart2_recv_ipd_payload(dump, sizeof(dump), &dump_len, 120u);
        (void)dump_len;
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
    if(!s_uart2_inited) return;
    if((HAL_GetTick() - s_step_ms) < ALIYUN_POLL_INTERVAL_MS) return;
    s_step_ms = HAL_GetTick();
    if(s_step != s_last_step_log) {
        s_last_step_log = s_step;
        printf("[ALIYUN] step=%s\r\n", aliyun_step_name(s_step));
    }

    switch(s_step) {
    case ALIYUN_STEP_AT:
        if(uart2_send_text("AT\r\n") && uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS)) {
            s_step = ALIYUN_STEP_ATE0;
            s_at_fail_cnt = 0u;
        } else {
            printf("[ALIYUN] AT fail\r\n");
            s_at_fail_cnt++;
            if(s_at_fail_cnt >= 2u) {
                if(s_uart2_baud == 115200u) uart2_set_baud(9600u);
                else uart2_set_baud(115200u);
                printf("[ALIYUN] switch baud=%lu and retry\r\n", (unsigned long)s_uart2_baud);
                s_at_fail_cnt = 0u;
            }
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
            s_step = ALIYUN_STEP_CWJAPQ;
        } else {
            printf("[ALIYUN] CIPMUX fail\r\n");
        }
        break;
    case ALIYUN_STEP_CWJAPQ:
        if(uart2_send_text("AT+CWJAP?\r\n") && uart2_wait_text("+CWJAP:", ALIYUN_WAIT_MID_MS)) {
            s_step = ALIYUN_STEP_CIFSR;
        } else {
            /* Not associated to AP yet */
            printf("[ALIYUN] WIFI not connected (CWJAP?)\r\n");
#if (APP_ALIYUN_WIFI_AUTO_JOIN == 1)
            uart2_send_text("AT+CWAUTOCONN=1\r\n");
            uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS);
            s_step = ALIYUN_STEP_CWJAP_SET;
#else
            s_step = ALIYUN_STEP_CWJAPQ;
#endif
        }
        break;
    case ALIYUN_STEP_CWJAP_SET:
        snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n",
                 APP_ALIYUN_WIFI_SSID, APP_ALIYUN_WIFI_PASSWORD);
        if(uart2_send_text(cmd) &&
           (uart2_wait_text("WIFI CONNECTED", ALIYUN_WAIT_LONG_MS) || uart2_wait_text("WIFI GOT IP", ALIYUN_WAIT_LONG_MS) || uart2_wait_text("OK", ALIYUN_WAIT_LONG_MS))) {
            printf("[ALIYUN] WIFI connected\r\n");
            s_step = ALIYUN_STEP_CIFSR;
        } else {
            printf("[ALIYUN] CWJAP connect fail\r\n");
            s_step = ALIYUN_STEP_CWJAPQ;
        }
        break;
    case ALIYUN_STEP_CIFSR:
        if(uart2_send_text("AT+CIFSR\r\n") &&
           (uart2_wait_text("STAIP", ALIYUN_WAIT_SHORT_MS) || uart2_wait_text("+CIFSR:STAIP", ALIYUN_WAIT_SHORT_MS))) {
            s_step = ALIYUN_STEP_CIPSTART;
        } else {
            printf("[ALIYUN] no STA IP (CIFSR)\r\n");
            s_step = ALIYUN_STEP_CWJAPQ;
        }
        break;
#if (APP_ALIYUN_SNTP_ENABLE == 1)
    case ALIYUN_STEP_SNTP_CFG:
        snprintf(cmd, sizeof(cmd), "AT+CIPSNTPCFG=1,%d,\"ntp.aliyun.com\",\"cn.pool.ntp.org\"\r\n",
                 APP_ALIYUN_SNTP_TZ_HOURS);
        if(uart2_send_text(cmd) && uart2_wait_text("OK", ALIYUN_SNTP_CMD_OK_MS)) {
            s_sntp_wait_until_ms = HAL_GetTick() + ALIYUN_SNTP_POSTCFG_DELAY_MS;
            s_sntp_try_cnt = 0u;
            s_sntp_empty_cnt = 0u;
            s_sntp_next_query_ms = s_sntp_wait_until_ms;
            s_sntp_deadline_ms = s_sntp_wait_until_ms + 15000u;
            s_step = ALIYUN_STEP_SNTP_WAIT;
        } else {
            printf("[ALIYUN] SNTP config fail, continue MQTT\r\n");
            s_step = ALIYUN_STEP_CIPSTART;
        }
        break;
    case ALIYUN_STEP_SNTP_WAIT:
        if((int32_t)(HAL_GetTick() - s_sntp_wait_until_ms) >= 0) {
            s_step = ALIYUN_STEP_SNTP_TIME;
        }
        break;
    case ALIYUN_STEP_SNTP_TIME:
        if((int32_t)(HAL_GetTick() - s_sntp_next_query_ms) >= 0) {
            s_sntp_next_query_ms = HAL_GetTick() + 1000u;
            if(aliyun_try_fetch_sntp_once()) {
                s_step = ALIYUN_STEP_CIPSTART;
            } else {
                s_sntp_try_cnt++;
                if(s_sntp_empty_cnt >= 3u && !s_http_date_tried) {
                    printf("[ALIYUN] SNTP empty response, switch to HTTP-Date\r\n");
                    s_http_date_tried = 1u;
                    (void)aliyun_try_fetch_http_date_once();
                    s_step = ALIYUN_STEP_CIPSTART;
                }
            }
        }
        if((int32_t)(HAL_GetTick() - s_sntp_deadline_ms) >= 0) {
            printf("[ALIYUN] SNTP timeout, continue MQTT (tries=%u)\r\n", (unsigned)s_sntp_try_cnt);
            if(!s_sntp_synced && !s_http_date_tried) {
                s_http_date_tried = 1u;
                (void)aliyun_try_fetch_http_date_once();
            }
            s_step = ALIYUN_STEP_CIPSTART;
        }
        break;
#endif
    case ALIYUN_STEP_CIPSTART:
        snprintf(cmd, sizeof(cmd), "AT+CIPSTART=0,\"TCP\",\"%s\",1883\r\n", s_host);
        if(uart2_send_text(cmd) && (uart2_wait_text("CONNECT", ALIYUN_WAIT_LONG_MS) || uart2_wait_text("ALREADY CONNECTED", ALIYUN_WAIT_SHORT_MS))) {
            s_step = ALIYUN_STEP_CIPSEND;
            printf("[ALIYUN] TCP connected\r\n");
        } else {
            printf("[ALIYUN] CIPSTART fail\r\n");
            if(uart2_wait_text("ERROR", ALIYUN_WAIT_SHORT_MS)) {
                printf("[ALIYUN] CIPSTART got ERROR\r\n");
            }
            s_step = ALIYUN_STEP_CWJAPQ;
        }
        break;
    case ALIYUN_STEP_CIPSEND:
        if(s_password[0] == '\0' || s_mqtt_connect_pkt_len == 0u) {
            printf("[ALIYUN] MQTT password empty or pkt invalid\r\n");
            return;
        }
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%u\r\n", (unsigned)s_mqtt_connect_pkt_len);
        if(uart2_send_text(cmd) && uart2_wait_text(">", ALIYUN_WAIT_SHORT_MS)) {
            s_step = ALIYUN_STEP_SEND_CONNECT;
        } else {
            printf("[ALIYUN] CIPSEND prompt fail\r\n");
        }
        break;
    case ALIYUN_STEP_SEND_CONNECT:
        if(uart2_send_bytes(s_mqtt_connect_pkt, s_mqtt_connect_pkt_len)) {
            s_step = ALIYUN_STEP_WAIT_CONNACK;
        } else {
            printf("[ALIYUN] send mqtt connect fail\r\n");
        }
        break;
    case ALIYUN_STEP_WAIT_CONNACK:
        {
            uint8_t ack[24];
            uint16_t ack_len = 0u;
            uint16_t i;
            uint8_t ok = 0u;
            if(uart2_recv_ipd_payload(ack, sizeof(ack), &ack_len, ALIYUN_WAIT_LONG_MS)) {
                for(i = 0u; (uint16_t)(i + 3u) < ack_len; i++) {
                    if(ack[i] == 0x20u && ack[i + 1u] == 0x02u && ack[i + 2u] == 0x00u && ack[i + 3u] == 0x00u) {
                        ok = 1u;
                        break;
                    }
                }
            }
            if(ok) {
                s_step = ALIYUN_STEP_ONLINE;
                s_last_ping_ms = HAL_GetTick();
                printf("[ALIYUN] MQTT connected (CONNACK OK)\r\n");
            } else {
                s_step = ALIYUN_STEP_CIPSTART;
                printf("[ALIYUN] CONNACK invalid\r\n");
            }
        }
        break;
    case ALIYUN_STEP_ONLINE:
        aliyun_ntp_try_sync_online();
        if((HAL_GetTick() - s_last_ping_ms) >= ALIYUN_KEEPALIVE_PING_MS) {
            s_last_ping_ms = HAL_GetTick();
            if(!aliyun_send_ping_and_check()) {
                printf("[ALIYUN] ping timeout/reconnect\r\n");
                uart2_send_text("AT+CIPCLOSE=0\r\n");
                uart2_wait_text("OK", ALIYUN_WAIT_SHORT_MS);
                s_step = ALIYUN_STEP_CIPSTART;
            }
        }
        break;
    default:
        break;
    }
#endif
}
