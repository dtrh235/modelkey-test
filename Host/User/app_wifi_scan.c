#include "app_wifi_scan.h"
#include "app_screen_wifi_flow.h"
#include "app_ccm_ram.h"
#include "app_cloud_session.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cloud_aliyun_at.h"
#include "app_config.h"
#if (APP_WIFI_CWJAP_TRACE != 0)
#include "SYSTEM/usart/usart.h"
#endif
#include "app_wifi_cfg.h"
#include "app_config.h"
#if (APP_WIFI_UART_DEBUG == 0)
#undef printf
#define printf(...) ((void)0)
#endif
#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif
#include "app_screen.h"
#include "app_wifi_diag.h"
#include "app_touch_ui.h"
#include "lvgl.h"
#include "app_wifi_policy.h"
#include "app_wifi_remember.h"
#include "app_state.h"
#include "app_link_guard.h"
#include "SYSTEM/usart/usart.h"
#include "stm32f4xx_hal.h"

extern volatile app_scr_t g_app_scr;

#if (APP_WIFI_UI_SCAN_ENABLE == 1)

static volatile uint8_t s_wifi_ui_active = 0u;

void app_wifi_scan_bind_cloud_task(TaskHandle_t task)
{
    (void)task;
}

void app_wifi_scan_ui_set_active(uint8_t on)
{
    s_wifi_ui_active = on ? 1u : 0u;
}

uint8_t app_wifi_scan_ui_active(void)
{
    return s_wifi_ui_active;
}

static uint8_t app_wifi_on_scr11(void)
{
    return (s_wifi_ui_active != 0u || g_app_scr == APP_SCR_11) ? 1u : 0u;
}

#define SCAN_BUF_SZ        3072u
#define SCAN_GAP_MS        5000u
#define SCAN_DEFER_MS      800u
#define SCAN_EMPTY_RETRY_MS 3000u
#define SCAN_ENTER_DEFER_MS 800u
#define CONNECT_TIMEOUT_MS      35000u
#define CONNECT_ABORT_SCAN_MS   1200u

typedef enum {
    SCAN_ST_IDLE = 0,
    SCAN_ST_BUSY
} scan_st_t;

typedef enum {
    CONN_ST_IDLE = 0,
    CONN_ST_BUSY
} conn_st_t;

typedef enum {
    CONN_PHASE_IDLE = 0,
    CONN_PHASE_ABORT_SCAN,
    CONN_PHASE_JOIN
} conn_phase_t;

static app_wifi_ap_t s_aps[APP_WIFI_SCAN_MAX];
static uint8_t s_ap_count = 0u;
static scan_st_t s_scan_st = SCAN_ST_IDLE;
static uint32_t s_scan_next_ms = 0u;
static APP_CCM_DATA char s_scan_buf[SCAN_BUF_SZ];
static uint16_t s_scan_pos = 0u;

static conn_st_t s_conn_st = CONN_ST_IDLE;
static conn_phase_t s_conn_phase = CONN_PHASE_IDLE;
static uint32_t s_conn_deadline = 0u;
static uint32_t s_conn_abort_deadline = 0u;
static uint8_t s_conn_result = 0u;
static uint8_t s_conn_finish_r = 0u;
static uint8_t s_conn_finish_pending = 0u;
static uint8_t s_conn_esp_idle = 0u;
static volatile uint8_t s_list_dirty = 0u;
static volatile uint8_t s_scan_pass = 0u;
static int s_scan_last_rc = 0;
/* 连接进行中：完全停扫，避免 CWLAP 与 CWJAP 抢 UART2 */
static uint8_t s_scan_hold_connect = 0u;
static uint8_t s_scan_user_refresh = 0u;

#if (APP_WIFI_CWJAP_TRACE != 0)
static void wifi_scan_trace_rc(int rc)
{
    switch(rc) {
    case -9:
        usart_debug_tx_str("[WiFi] scan rc=-9 abort(stop/exclusive)\r\n");
        break;
    case -6:
        usart_debug_tx_str("[WiFi] scan rc=-6 esp ERROR/FAIL/bad frame\r\n");
        break;
    case -5:
        usart_debug_tx_str("[WiFi] scan rc=-5 timeout/no data/not idle\r\n");
        break;
    case -4:
        usart_debug_tx_str("[WiFi] scan rc=-4 send AT+CWLAP fail\r\n");
        break;
    case -3:
        usart_debug_tx_str("[WiFi] scan rc=-3 recover path failed\r\n");
        break;
    case -1:
        usart_debug_tx_str("[WiFi] scan rc=-1 modem abort\r\n");
        break;
    case 0:
        usart_debug_tx_str("[WiFi] scan rc=0 success\r\n");
        break;
    default:
        usart_debug_tx_str("[WiFi] scan rc=other\r\n");
        break;
    }
}
#endif

static void app_wifi_scan_abort_inflight_no_policy(void)
{
    g_wifi_scan_abort = 1u;
    cloud_aliyun_cwlap_scan_abort();
    cloud_aliyun_at_cwlap_scan_async_abort();
    /* 不在 GuiTask 里 poll UART；CloudTask 的 cloud_tick 会收尾 */
}

static void app_wifi_scan_pause_for_connect(void)
{
    s_scan_hold_connect = 1u;
    g_wifi_scan_pending = 0u;
    app_wifi_scan_abort_inflight_no_policy();
    s_scan_next_ms = HAL_GetTick() + 60000u;
    WIFI_DBG("scan paused until connect done");
}

void app_wifi_scan_release_connect_hold(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    s_scan_hold_connect = 0u;
#endif
}

static void app_wifi_scan_resume_after_connect(void)
{
    if(s_scan_hold_connect == 0u) {
        return;
    }
    s_scan_hold_connect = 0u;
    g_wifi_scan_abort = 0u;
    if(g_app_scr != APP_SCR_11 || g_wifi_exclusive == 0u) {
        return;
    }
    /* 连接结束不自动重扫：仅进入页/用户点刷新按钮才触发 CWLAP */
    g_wifi_scan_pending = 0u;
    WIFI_DBG("scan hold released (manual refresh only)");
}

static uint8_t app_wifi_scan_connect_holds(void);

uint8_t app_wifi_scan_connect_hold_active(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    return app_wifi_scan_connect_holds();
#else
    return 0u;
#endif
}

uint8_t app_wifi_scan_connect_hold_scan_only(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    return s_scan_hold_connect;
#else
    return 0u;
#endif
}

static uint8_t app_wifi_scan_connect_holds(void)
{
    if(s_scan_hold_connect != 0u) {
        return 1u;
    }
    if(app_link_guard_blocks_scan() != 0u) {
        return 1u;
    }
    if(app_wifi_connect_busy() != 0u) {
        return 1u;
    }
    if(cloud_aliyun_at_user_wifi_join_active() != 0u) {
        return 1u;
    }
    return 0u;
}

/* 仅限制：WiFi 已连、云端未上线时的周期性重扫；首次/未连 WiFi 必须能扫 */
static uint8_t app_wifi_scan_allowed(void)
{
    if(g_app_scr != APP_SCR_11 || g_wifi_exclusive == 0u) {
        return 0u;
    }
    if(app_wifi_scan_connect_holds() != 0u) {
        return 0u;
    }
    if(app_wifi_remember_autoconnect_busy() != 0u) {
        return 0u;
    }
    if(app_cloud_session_busy() != 0u && cloud_aliyun_at_scr11_cloud_hold() == 0u) {
        return 0u;
    }
    return 1u;
}

/* 隐藏 SSID 为空；其余扫描结果均入列表 */
static uint8_t ssid_is_valid(const char *ssid)
{
    const uint8_t *p = (const uint8_t *)ssid;

    if(ssid == NULL || ssid[0] == '\0') {
        return 0u;
    }
    while(*p != 0u) {
        if(*p < 0x20u || *p == 0x7Fu) {
            return 0u;
        }
        p++;
    }
    return 1u;
}

static int parse_cwlap_line(const char *line, char *ssid, size_t ssid_sz, int8_t *rssi, uint8_t *channel)
{
    const char *p;
    const char *q;
    const char *mac_end;
    long rv;
    unsigned long ch;

    if(line == NULL || ssid == NULL || ssid_sz < 2u) return 0;
    if(channel != NULL) {
        *channel = 0u;
    }
    p = strstr(line, "+CWLAP");
    if(p == NULL) return 0;
    p = strchr(p, '"');
    if(p == NULL) return 0;
    p++;
    q = strchr(p, '"');
    if(q == NULL || q <= p) return 0;
    if((size_t)(q - p) >= ssid_sz) return 0;
    memcpy(ssid, p, (size_t)(q - p));
    ssid[q - p] = '\0';
    if(ssid[0] == '\0') return 0;
    p = q + 1;
    p = strchr(p, ',');
    if(p == NULL) return 0;
    p++;
    rv = strtol(p, NULL, 10);
    if(rssi != NULL) {
        *rssi = (int8_t)rv;
    }
    mac_end = strchr(p, '"');
    if(mac_end != NULL) {
        mac_end = strchr(mac_end + 1, '"');
        if(mac_end != NULL) {
            p = mac_end + 1;
            p = strchr(p, ',');
            if(p != NULL) {
                p++;
                (void)strtol(p, (char **)&p, 10);
                while(*p == ',') {
                    p++;
                }
                ch = strtoul(p, NULL, 10);
                if(ch >= 1u && ch <= 14u && channel != NULL) {
                    *channel = (uint8_t)ch;
                }
            }
        }
    }
    return 1;
}

static uint8_t ap_list_has(const char *ssid)
{
    uint8_t i;
    for(i = 0u; i < s_ap_count; i++) {
        if(strcmp(s_aps[i].ssid, ssid) == 0) return 1u;
    }
    return 0u;
}

static void ap_list_add(const char *ssid, int8_t rssi, uint8_t channel)
{
    if(s_ap_count >= APP_WIFI_SCAN_MAX || ssid == NULL || ssid[0] == '\0') {
        return;
    }
    if(!ssid_is_valid(ssid)) {
        return;
    }
    if(ap_list_has(ssid)) {
        return;
    }
    strncpy(s_aps[s_ap_count].ssid, ssid, sizeof(s_aps[0].ssid) - 1u);
    s_aps[s_ap_count].ssid[sizeof(s_aps[0].ssid) - 1u] = '\0';
    s_aps[s_ap_count].rssi = rssi;
    s_aps[s_ap_count].channel = channel;
#if (APP_WIFI_UART_DEBUG != 0)
    WIFI_DBG("parse add[%u] '%s' rssi=%d", (unsigned)s_ap_count, s_aps[s_ap_count].ssid,
             (int)s_aps[s_ap_count].rssi);
#endif
    s_ap_count++;
}

static void ap_list_sort(void)
{
    uint8_t i;
    uint8_t j;
    app_wifi_ap_t tmp;

    for(i = 0u; i < s_ap_count; i++) {
        for(j = (uint8_t)(i + 1u); j < s_ap_count; j++) {
            if(s_aps[j].rssi > s_aps[i].rssi) {
                tmp = s_aps[i];
                s_aps[i] = s_aps[j];
                s_aps[j] = tmp;
            }
        }
    }
}

static void wifi_log_raw_snippet(void)
{
#if (APP_WIFI_UART_DEBUG != 0)
    static char snippet[121];
    size_t n = strlen(s_scan_buf);

    if(n == 0u) {
        WIFI_DBG("raw empty");
        return;
    }
    if(n > 120u) {
        n = 120u;
    }
    memcpy(snippet, s_scan_buf, n);
    snippet[n] = '\0';
    WIFI_DBG("raw(%uB): %s", (unsigned)strlen(s_scan_buf), snippet);
#else
    (void)0;
#endif
}

static void scan_parse_incremental(void)
{
    const char *p;
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    uint8_t before = s_ap_count;

    s_scan_buf[SCAN_BUF_SZ - 1u] = '\0';
    p = s_scan_buf;
    while(p != NULL && *p != '\0') {
        const char *next = strstr(p, "+CWLAP");
        if(next == NULL) {
            break;
        }
        if(parse_cwlap_line(next, ssid, sizeof(ssid), &rssi, &channel)) {
            ap_list_add(ssid, rssi, channel);
        }
        p = next + 6;
    }
    if(s_ap_count != before) {
        ap_list_sort();
        s_list_dirty = 1u;
    }
}

static void scan_finish_parse(void)
{
    const char *p;
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    uint8_t line_n = 0u;

    s_ap_count = 0u;
    s_scan_buf[SCAN_BUF_SZ - 1u] = '\0';
    wifi_log_raw_snippet();

    p = s_scan_buf;
    while(p != NULL && *p != '\0') {
        const char *next = strstr(p, "+CWLAP");
        if(next == NULL) {
            break;
        }
        line_n++;
        if(parse_cwlap_line(next, ssid, sizeof(ssid), &rssi, &channel)) {
            ap_list_add(ssid, rssi, channel);
        } else {
            WIFI_DBG("parse line#%u CWLAP parse fail", (unsigned)line_n);
        }
        p = next + 6;
    }
    ap_list_sort();
    WIFI_DBG("parse total lines=%u ap_count=%u", (unsigned)line_n, (unsigned)s_ap_count);
}

void app_wifi_scan_release_uart2(void)
{
    cloud_aliyun_cwlap_scan_abort();
}

static void wifi_scan_finish(int rc, uint16_t pos)
{
    uint8_t connect_hold = app_wifi_scan_connect_holds();

    if(rc == -9) {
        WIFI_DBG("scan abort -9 pend=%u", (unsigned)g_wifi_scan_pending);
#if (APP_WIFI_CWJAP_TRACE != 0)
        wifi_scan_trace_rc(rc);
#endif
        s_scan_st = SCAN_ST_IDLE;
        app_wifi_scan_release_uart2();
        return;
    }
    if(rc == -1) {
        WIFI_DBG("scan abort rc=-1 modem");
#if (APP_WIFI_CWJAP_TRACE != 0)
        wifi_scan_trace_rc(rc);
#endif
        s_scan_st = SCAN_ST_IDLE;
        g_wifi_scan_pending = 0u;
        app_wifi_scan_release_uart2();
        return;
    }
    if(rc < 0) {
        printf("[WiFi] scan fail rc=%d\r\n", rc);
#if (APP_WIFI_CWJAP_TRACE != 0)
        wifi_scan_trace_rc(rc);
#endif
        s_scan_pass = 0u;
        s_scan_last_rc = rc;
        s_list_dirty = 1u;
        s_scan_st = SCAN_ST_IDLE;
        (void)connect_hold;
        g_wifi_scan_pending = 0u;
        app_wifi_scan_release_uart2();
        screen_wifi_notify_scan_done();
        return;
    }
#if (APP_WIFI_CWJAP_TRACE != 0)
    wifi_scan_trace_rc(0);
#endif
    s_scan_last_rc = 0;
    scan_finish_parse();
    s_scan_pass = 1u;
    s_list_dirty = 1u;
    printf("[WiFi] scan OK ap=%u\r\n", (unsigned)s_ap_count);
    if(connect_hold == 0u && s_ap_count > 0u) {
        app_wifi_policy_on_scan_done();
    }
    s_scan_st = SCAN_ST_IDLE;
    g_wifi_scan_pending = 0u;
    app_wifi_scan_release_uart2();
    screen_wifi_notify_scan_done();
}

static void wifi_run_scan_blocking(void)
{
    uint16_t pos = 0u;
    int rc;

    s_scan_pass = 0u;
    cloud_uart2_ensure_init();
    memset(s_scan_buf, 0, sizeof(s_scan_buf));
    rc = cloud_aliyun_at_run_cwlap_scan(s_scan_buf, SCAN_BUF_SZ, &pos);
    wifi_scan_finish(rc, pos);
}

uint8_t app_wifi_scan_take_list_dirty(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    uint8_t v = s_list_dirty;
    s_list_dirty = 0u;
    return v;
#else
    return 0u;
#endif
}

uint8_t app_wifi_scan_peek_list_dirty(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    return s_list_dirty;
#else
    return 0u;
#endif
}

uint8_t app_wifi_scan_pass_completed(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    return s_scan_pass;
#else
    return 0u;
#endif
}

int app_wifi_scan_last_rc(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    return s_scan_last_rc;
#else
    return 0;
#endif
}

uint8_t app_wifi_scan_last_failed(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    return (s_scan_last_rc < 0) ? 1u : 0u;
#else
    return 0u;
#endif
}

uint8_t app_wifi_scan_has_pending(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    return g_wifi_scan_pending;
#else
    return 0u;
#endif
}

uint8_t app_wifi_scan_has_ssid(const char *ssid)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    uint8_t i;

    if(ssid == NULL || ssid[0] == '\0') {
        return 0u;
    }
    for(i = 0u; i < s_ap_count; i++) {
        if(strcmp(s_aps[i].ssid, ssid) == 0) {
            return 1u;
        }
    }
    return 0u;
#else
    (void)ssid;
    return 0u;
#endif
}

uint8_t app_wifi_scan_get_ssid_channel(const char *ssid)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    uint8_t i;

    if(ssid == NULL || ssid[0] == '\0') {
        return 0u;
    }
    for(i = 0u; i < s_ap_count; i++) {
        if(strcmp(s_aps[i].ssid, ssid) == 0) {
            return s_aps[i].channel;
        }
    }
    return 0u;
#else
    (void)ssid;
    return 0u;
#endif
}

void app_wifi_scan_defer_rescan_ms(uint32_t ms)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    uint32_t target;

    if(ms == 0u) {
        return;
    }
    target = HAL_GetTick() + ms;
    if((int32_t)(target - s_scan_next_ms) > 0) {
        s_scan_next_ms = target;
    }
#else
    (void)ms;
#endif
}

uint8_t app_wifi_scan_in_rescan_wait(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(s_scan_st == SCAN_ST_BUSY) {
        return 0u;
    }
    if(s_scan_pass != 0u && g_wifi_scan_pending == 0u && (HAL_GetTick() < s_scan_next_ms)) {
        return 1u;
    }
    return 0u;
#else
    return 0u;
#endif
}

void app_wifi_scan_reset(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    g_wifi_exclusive = 1u;
    s_wifi_ui_active = 1u;
    s_scan_hold_connect = 0u;
    g_wifi_scan_abort = 0u;
    cloud_uart2_set_ui_busy(0u);
    s_scan_st = SCAN_ST_IDLE;
    s_ap_count = 0u;
    s_list_dirty = 0u;
    s_scan_pass = 0u;
    s_scan_next_ms = HAL_GetTick();
    g_wifi_scan_pending = 0u;
    __DMB();
#if (APP_WIFI_UART_DEBUG != 0)
    usart_debug_tx_str("[WiFi] reset exclusive=1 pending=0 (manual refresh)\r\n");
#endif
#else
    (void)0;
#endif
}

void app_wifi_scan_on_cloud_online(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    (void)0;
#endif
}

void app_wifi_scan_scr11_leave(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    g_wifi_scan_abort = 1u;
    cloud_aliyun_cwlap_scan_abort();
    if(app_wifi_connect_busy() != 0u &&
       cloud_aliyun_at_wifi_link_ready() == 0u &&
       cloud_aliyun_at_wifi_bringup_active() == 0u) {
        cloud_aliyun_at_user_wifi_join_abort();
    }
    if(cloud_aliyun_at_wifi_bringup_active() == 0u) {
        cloud_uart2_set_ui_busy(0u);
    }
    if(s_scan_st == SCAN_ST_BUSY) {
        s_scan_st = SCAN_ST_IDLE;
    }
    g_wifi_exclusive = 0u;
    s_wifi_ui_active = 0u;
    s_scan_hold_connect = 0u;
    g_wifi_scan_pending = 0u;
    WIFI_DBG("leave scr11 exclusive=0 pend=%u busy=%u", (unsigned)g_wifi_scan_pending,
             (unsigned)(s_scan_st == SCAN_ST_BUSY));
#else
    (void)0;
#endif
}

uint8_t app_wifi_scan_busy(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    return (s_scan_st == SCAN_ST_BUSY) ? 1u : 0u;
#else
    return 0u;
#endif
}

uint8_t app_wifi_exclusive_mode(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(g_wifi_exclusive != 0u) {
        return 1u;
    }
    if(s_scan_st == SCAN_ST_BUSY) {
        return 1u;
    }
    if(cloud_uart2_ui_busy() != 0u) {
        return 1u;
    }
    if(s_conn_st == CONN_ST_BUSY) {
        return 1u;
    }
    return 0u;
#else
    return 0u;
#endif
}

void app_wifi_scan_kick(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(app_wifi_on_scr11() == 0u || app_wifi_scan_connect_holds() != 0u) {
        return;
    }
    if(s_scan_st != SCAN_ST_IDLE) {
        return;
    }
    s_scan_next_ms = HAL_GetTick();
    g_wifi_scan_pending = 1u;
#endif
}

void app_wifi_scan_mark_user_refresh(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    s_scan_user_refresh = 1u;
    s_scan_last_rc = 0;
#endif
}

void app_wifi_scan_request_now(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(app_wifi_on_scr11() == 0u) {
        return;
    }
    if(app_wifi_connect_busy() != 0u) {
        printf("[WiFi] scan request blocked: connecting\r\n");
        return;
    }
    if(app_link_guard_mqtt() != 0u) {
        app_link_guard_mqtt_end(0u);
    }
    if(app_link_guard_blocks_scan() != 0u) {
        printf("[WiFi] scan request blocked: link guard\r\n");
        return;
    }
    if(s_scan_st == SCAN_ST_BUSY || cloud_aliyun_at_cwlap_scan_async_active() != 0u) {
        cloud_aliyun_cwlap_scan_abort();
        cloud_aliyun_at_cwlap_scan_async_abort();
        s_scan_st = SCAN_ST_IDLE;
        cloud_uart2_set_ui_busy(0u);
        (void)cloud_aliyun_at_cwlap_teardown_idle(2500u);
    }
    if(g_app_scr == APP_SCR_11) {
        g_wifi_exclusive = 1u;
    }
    g_wifi_scan_abort = 0u;
    s_scan_pass = 0u;
    s_scan_last_rc = 0;
    s_scan_next_ms = HAL_GetTick();
    g_wifi_scan_pending = 1u;
    printf("[WiFi] scan request_now pend=1\r\n");
#endif
}

void app_wifi_scan_kick_ui(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(app_wifi_on_scr11() == 0u) {
        WIFI_DBG("kick_ui ignored (scr=%u need 11)", (unsigned)g_app_scr);
        return;
    }
    if(app_wifi_scan_allowed() == 0u) {
        WIFI_DBG("kick_ui ignored (cloud not online)");
        return;
    }
    app_wifi_scan_kick();
    WIFI_DBG("kick_ui pending=1 scr=11");
#endif
}

void app_wifi_scan_run_blocking(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    s_scan_st = SCAN_ST_BUSY;
    wifi_run_scan_blocking();
#else
    (void)0;
#endif
}

void app_wifi_scan_poll(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    (void)0;
#else
    (void)0;
#endif
}

void app_wifi_scan_stuck_recover(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(g_wifi_exclusive == 0u) {
        if(s_scan_st == SCAN_ST_BUSY) {
            cloud_aliyun_cwlap_scan_abort();
            s_scan_st = SCAN_ST_IDLE;
        }
        return;
    }
    if(s_scan_st == SCAN_ST_BUSY) {
        return;
    }
    cloud_uart2_set_ui_busy(0u);
#else
    (void)0;
#endif
}

void app_wifi_scan_service(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)

    if(g_wifi_exclusive == 0u) {
        if(s_scan_st == SCAN_ST_BUSY) {
            cloud_aliyun_cwlap_scan_abort();
            s_scan_st = SCAN_ST_IDLE;
        }
        return;
    }

    if(s_scan_st == SCAN_ST_BUSY) {
        return;
    }

    if(g_wifi_scan_pending == 0u) {
        return;
    }
    if(HAL_GetTick() < s_scan_next_ms) {
        return;
    }
    if(app_wifi_scan_allowed() == 0u) {
        return;
    }
    cloud_aliyun_at_scr11_enter();
    s_scan_pass = 0u;
    s_scan_pos = 0u;
    WIFI_DBG("scan CWLAP scr=%u", (unsigned)g_app_scr);
    {
        int rc = cloud_aliyun_at_cwlap_scan_async_start(s_scan_buf, SCAN_BUF_SZ, &s_scan_pos);
        if(rc == CWLAP_SCAN_DEFERRED ||
           (rc == CWLAP_SCAN_RUNNING && cloud_aliyun_at_cwlap_scan_async_active() == 0u)) {
            g_wifi_scan_pending = 1u;
            printf("[WiFi] scan defer (uart2 busy) step=%u ui=%u async=%u\r\n",
                   (unsigned)cloud_aliyun_at_user_wifi_join_state(),
                   (unsigned)cloud_uart2_ui_busy(),
                   (unsigned)cloud_aliyun_at_cwlap_scan_async_active());
            return;
        }
        g_wifi_scan_pending = 0u;
        printf("[WiFi] scan run start\r\n");
        screen_wifi_notify_scan_start();
        if(rc != CWLAP_SCAN_RUNNING) {
            s_scan_st = SCAN_ST_IDLE;
            wifi_scan_finish(rc, s_scan_pos);
#if (APP_WIFI_UART_DEBUG != 0)
            usart_debug_tx_str("[WiFi] scan done\r\n");
#endif
            return;
        }
        s_scan_st = SCAN_ST_BUSY;
    }
#else
    (void)0;
#endif
}

void app_wifi_scan_worker_run(void)
{
    (void)0;
}

void app_wifi_scan_notify_rx(const char *buf, uint16_t len)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(buf == NULL || len == 0u || s_scan_st != SCAN_ST_BUSY) {
        return;
    }
    if(buf != (const char *)s_scan_buf) {
        if(len >= SCAN_BUF_SZ) {
            len = (uint16_t)(SCAN_BUF_SZ - 1u);
        }
        memcpy(s_scan_buf, buf, len);
        s_scan_buf[len] = '\0';
    }
    scan_parse_incremental();
#else
    (void)buf;
    (void)len;
#endif
}

void app_wifi_scan_gui_tick(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    /* GuiTask：只清标志，禁止 UART/CWLAP（刷新/连接时触屏才不卡） */
    if(g_app_scr != APP_SCR_11) {
        return;
    }
    if(g_wifi_exclusive == 0u) {
        g_wifi_exclusive = 1u;
    }
    if(app_wifi_scan_connect_holds() != 0u) {
        if(s_scan_st == SCAN_ST_BUSY || cloud_aliyun_at_cwlap_scan_async_active() != 0u) {
            app_wifi_scan_abort_inflight_no_policy();
            s_scan_st = SCAN_ST_IDLE;
        }
        cloud_uart2_set_ui_busy(0u);
        return;
    }
#else
    (void)0;
#endif
}

uint8_t app_wifi_scan_uart_busy(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(g_wifi_scan_pending != 0u) {
        return 1u;
    }
    if(s_scan_st == SCAN_ST_BUSY) {
        return 1u;
    }
    if(app_wifi_connect_busy() != 0u) {
        return 1u;
    }
    if(cloud_aliyun_at_user_wifi_join_active() != 0u) {
        return 1u;
    }
    if(cloud_aliyun_at_cwlap_scan_async_active() != 0u) {
        return 1u;
    }
    return 0u;
#else
    return 0u;
#endif
}

static void app_wifi_scan_log_block(const char *why)
{
    printf("[WiFi] scan wait: %s pend=%u busy=%u hold=%u guard=%u session=%u async=%u ui=%u\r\n",
           why,
           (unsigned)g_wifi_scan_pending,
           (unsigned)(s_scan_st == SCAN_ST_BUSY),
           (unsigned)app_wifi_scan_connect_holds(),
           (unsigned)app_link_guard_active(),
           (unsigned)app_cloud_session_busy(),
           (unsigned)cloud_aliyun_at_cwlap_scan_async_active(),
           (unsigned)cloud_uart2_ui_busy());
}

void app_wifi_scan_cloud_tick(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    static uint32_t s_scan_block_log_ms;

    if(g_app_scr == APP_SCR_11 && g_wifi_scan_pending != 0u && app_link_guard_mqtt() != 0u) {
        app_link_guard_mqtt_end(0u);
    }

    if(app_link_guard_wifi() != 0u || app_link_guard_mqtt() != 0u) {
        if(g_wifi_scan_pending != 0u) {
            uint32_t now = HAL_GetTick();
            if(s_scan_block_log_ms == 0u || (now - s_scan_block_log_ms) >= 2000u) {
                s_scan_block_log_ms = now;
                app_wifi_scan_log_block("link_guard");
            }
        }
        return;
    }
    s_scan_block_log_ms = 0u;
    if(g_app_scr != APP_SCR_11 && g_wifi_exclusive == 0u) {
        return;
    }
    app_wifi_scan_stuck_recover();
    app_wifi_scan_poll();
    if(s_scan_st == SCAN_ST_BUSY) {
        if(cloud_aliyun_at_cwlap_scan_async_active() == 0u) {
            printf("[WiFi] scan stale busy -> recover\r\n");
            s_scan_st = SCAN_ST_IDLE;
            cloud_uart2_set_ui_busy(0u);
            if(g_wifi_scan_pending != 0u) {
                return;
            }
            wifi_scan_finish(-9, s_scan_pos);
            return;
        }
        {
            int rc = cloud_aliyun_at_cwlap_scan_async_poll();
            if(rc == CWLAP_SCAN_RUNNING) {
                return;
            }
            s_scan_st = SCAN_ST_IDLE;
            wifi_scan_finish(rc, s_scan_pos);
        }
        return;
    }
    if(g_wifi_scan_pending == 0u) {
        return;
    }
    if(HAL_GetTick() < s_scan_next_ms) {
        return;
    }
    if(app_wifi_scan_connect_holds() != 0u) {
        if(g_wifi_scan_pending != 0u) {
            uint32_t now = HAL_GetTick();
            if(s_scan_block_log_ms == 0u || (now - s_scan_block_log_ms) >= 2000u) {
                s_scan_block_log_ms = now;
                app_wifi_scan_log_block("connect_hold");
            }
        }
        return;
    }
    if(app_wifi_scan_allowed() == 0u) {
        if(g_wifi_scan_pending != 0u) {
            uint32_t now = HAL_GetTick();
            if(s_scan_block_log_ms == 0u || (now - s_scan_block_log_ms) >= 2000u) {
                s_scan_block_log_ms = now;
                printf("[WiFi] scan wait: not_allowed sess=%u hold=%u join=%u\r\n",
                       (unsigned)app_cloud_session_busy(),
                       (unsigned)app_wifi_scan_connect_holds(),
                       (unsigned)cloud_aliyun_at_user_wifi_join_active());
                app_wifi_scan_log_block("not_allowed");
            }
        }
        return;
    }
    app_wifi_scan_service();
#else
    (void)0;
#endif
}

void app_wifi_cloud_tick(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    app_wifi_scan_cloud_tick();
#else
    (void)0;
#endif
}

uint8_t app_wifi_scan_count(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    return s_ap_count;
#else
    return 0u;
#endif
}

const app_wifi_ap_t *app_wifi_scan_get(uint8_t index)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(index >= s_ap_count) return NULL;
    return &s_aps[index];
#else
    (void)index;
    return NULL;
#endif
}

void app_wifi_scan_on_sta_link_down(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    g_wifi_scan_abort = 1u;
    g_wifi_scan_pending = 0u;
    s_scan_st = SCAN_ST_IDLE;
    s_scan_pos = 0u;
    s_scan_hold_connect = 0u;
    s_scan_pass = 0u;
    s_scan_last_rc = 0;
    s_list_dirty = 0u;
    s_conn_st = CONN_ST_IDLE;
    s_conn_phase = CONN_PHASE_IDLE;
    s_conn_result = 0u;
    s_conn_finish_pending = 0u;
    cloud_aliyun_cwlap_scan_abort();
    cloud_aliyun_at_cwlap_scan_async_abort();
    app_wifi_scan_release_connect_hold();
    cloud_uart2_set_ui_busy(0u);
    app_link_guard_wifi_end(0u);
#endif
}

void app_wifi_connect_reset(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(cloud_aliyun_at_wifi_link_ready() != 0u) {
        s_conn_st = CONN_ST_IDLE;
        s_conn_phase = CONN_PHASE_IDLE;
        s_conn_result = 1u;
        app_wifi_scan_resume_after_connect();
        return;
    }
    if(cloud_aliyun_at_wifi_bringup_active() != 0u) {
        return;
    }
    s_conn_st = CONN_ST_IDLE;
    s_conn_phase = CONN_PHASE_IDLE;
    s_conn_result = 0u;
    cloud_aliyun_at_user_wifi_join_abort();
    app_wifi_scan_resume_after_connect();
#endif
}

static void app_wifi_connect_uart_teardown(void)
{
    g_wifi_scan_abort = 1u;
    s_scan_st = SCAN_ST_IDLE;
    cloud_aliyun_cwlap_scan_abort();
    cloud_aliyun_at_cwlap_scan_async_abort();
    cloud_uart2_set_ui_busy(0u);
    cloud_uart2_drain_hw(250u);
    cloud_uart2_rx_clear();
    app_wifi_scan_release_uart2();
}

static void app_wifi_connect_fail(uint8_t result)
{
    app_link_guard_wifi_end(0u);
    s_conn_result = result;
    s_conn_st = CONN_ST_IDLE;
    s_conn_phase = CONN_PHASE_IDLE;
    s_conn_finish_r = result;
    s_conn_finish_pending = 1u;
    cloud_aliyun_at_user_wifi_join_abort();
    cloud_aliyun_at_user_wifi_join_fail_clear();
    app_wifi_connect_uart_teardown();
    app_wifi_scan_resume_after_connect();
    app_wifi_scan_release_connect_hold();
    g_wifi_scan_abort = 0u;
    screen_wifi_notify_connect_fail();
}

void app_wifi_connect_gui_recover(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(s_conn_st != CONN_ST_BUSY) {
        return;
    }
    if(cloud_aliyun_at_user_wifi_join_fail_pending() != 0u ||
       cloud_aliyun_at_user_wifi_join_state() == 3u) {
        app_wifi_connect_fail(2u);
        return;
    }
    if(cloud_aliyun_at_user_wifi_join_active() == 0u &&
       cloud_aliyun_at_wifi_bringup_active() == 0u &&
       s_conn_phase == CONN_PHASE_JOIN) {
        app_wifi_connect_fail(2u);
    }
#else
    (void)0;
#endif
}

uint8_t app_wifi_connect_take_result(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    uint8_t r;

    if(s_conn_finish_pending == 0u || s_conn_st == CONN_ST_BUSY) {
        return 0u;
    }
    r = s_conn_finish_r;
    s_conn_finish_pending = 0u;
    s_conn_finish_r = 0u;
    return r;
#else
    return 0u;
#endif
}

void app_wifi_scan_drop_for_connect(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    g_wifi_scan_abort = 1u;
    g_wifi_scan_pending = 0u;
#endif
}

void app_wifi_scan_abort_for_mqtt(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    g_wifi_scan_abort = 1u;
    g_wifi_scan_pending = 0u;
    s_scan_st = SCAN_ST_IDLE;
    s_scan_pos = 0u;
    cloud_aliyun_cwlap_scan_abort();
    cloud_aliyun_at_cwlap_scan_async_abort();
    cloud_uart2_set_ui_busy(0u);
#endif
}

uint8_t app_wifi_scan_abort_and_wait_idle(uint32_t timeout_ms)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    uint32_t t0 = HAL_GetTick();

    g_wifi_scan_abort = 1u;
    cloud_aliyun_cwlap_scan_abort();
    while((HAL_GetTick() - t0) < timeout_ms) {
        if(!app_wifi_scan_busy() &&
           cloud_aliyun_at_cwlap_scan_async_active() == 0u &&
           cloud_uart2_ui_busy() == 0u) {
            WIFI_DBG("scan mcu idle after abort (%lums)",
                     (unsigned long)(HAL_GetTick() - t0));
            return 1u;
        }
#if (APP_USE_FREERTOS == 1)
        vTaskDelay(pdMS_TO_TICKS(5u));
#else
        HAL_Delay(5u);
#endif
    }
    WIFI_DBG("scan mcu idle wait TIMEOUT %lums busy=%u async=%u ui_busy=%u",
             (unsigned long)timeout_ms,
             (unsigned)app_wifi_scan_busy(),
             (unsigned)cloud_aliyun_at_cwlap_scan_async_active(),
             (unsigned)cloud_uart2_ui_busy());
    return 0u;
#else
    (void)timeout_ms;
    return 1u;
#endif
}

uint8_t app_wifi_connect_busy(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    return (s_conn_st == CONN_ST_BUSY) ? 1u : 0u;
#else
    return 0u;
#endif
}

void app_wifi_connect_start(const char *ssid, const char *password)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(ssid == NULL || ssid[0] == '\0') {
        return;
    }
    if(password == NULL) {
        password = "";
    }
    if(app_link_guard_blocks_wifi_connect() != 0u) {
        return;
    }
    if(s_conn_st == CONN_ST_BUSY) {
        return;
    }
    app_wifi_cfg_set(ssid, password);
    WIFI_DBG("connect start ssid=%s pwd_len=%u", ssid, (unsigned)strlen(app_wifi_cfg_get_password()));
    app_wifi_scan_pause_for_connect();
    app_link_guard_wifi_begin();
    s_conn_esp_idle = 0u;
    s_conn_st = CONN_ST_BUSY;
    s_conn_result = 0u;
    s_conn_finish_r = 0u;
    s_conn_finish_pending = 0u;
    s_conn_deadline = HAL_GetTick() + CONNECT_TIMEOUT_MS;
    s_conn_abort_deadline = HAL_GetTick() + 25000u;
    s_conn_phase = CONN_PHASE_ABORT_SCAN;
#if (APP_WIFI_CWJAP_TRACE != 0)
    usart_debug_tx_str("[WiFi] connect abort wait\r\n");
#endif
    WIFI_DBG("connect queued phase=abort scan=%u async=%u pending=%u",
             (unsigned)app_wifi_scan_busy(),
             (unsigned)cloud_aliyun_at_cwlap_scan_async_active(),
             (unsigned)g_wifi_scan_pending);
#else
    (void)ssid;
    (void)password;
#endif
}

uint8_t app_wifi_connect_poll(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    uint8_t jr;

    if(s_conn_st == CONN_ST_IDLE) {
        return s_conn_result;
    }

    if(s_conn_st == CONN_ST_BUSY &&
       (cloud_aliyun_at_user_wifi_join_fail_pending() != 0u ||
        cloud_aliyun_at_user_wifi_join_state() == 3u)) {
        app_wifi_connect_fail(2u);
        return 2u;
    }

    if(s_conn_phase == CONN_PHASE_ABORT_SCAN) {
        if(s_scan_st == SCAN_ST_BUSY || cloud_aliyun_at_cwlap_scan_async_active() != 0u) {
            return 0u;
        }
        if(s_conn_esp_idle == 0u) {
            if(cloud_aliyun_at_cwlap_teardown_idle(3800u) != 0u) {
                s_conn_esp_idle = 1u;
#if (APP_WIFI_CWJAP_TRACE != 0)
                usart_debug_tx_str("[WiFi] connect esp idle ok\r\n");
#endif
            } else if(HAL_GetTick() >= s_conn_abort_deadline) {
                if(cloud_uart2_rx_has("+CWLAP") != 0 ||
                   cloud_uart2_rx_has("busy p") != 0 ||
                   cloud_uart2_rx_has("busy") != 0) {
                    return 0u;
                }
#if (APP_WIFI_CWJAP_TRACE != 0)
                usart_debug_tx_str("[WiFi] connect esp idle timeout\r\n");
#endif
                WIFI_DBG("connect esp idle timeout -> join anyway");
                s_conn_esp_idle = 1u;
            } else {
                return 0u;
            }
        }
#if (APP_WIFI_CWJAP_TRACE != 0)
        if(app_wifi_scan_has_ssid(app_wifi_cfg_get_ssid()) == 0u) {
            usart_debug_tx_str("[WiFi] connect warn: ssid not in last scan\r\n");
        }
#endif
        s_conn_phase = CONN_PHASE_JOIN;
        /* teardown 已成功：跳过 CWJAP 前重复 modem_purge */
        cloud_aliyun_at_user_wifi_join_set_esp_idle_ok();
        cloud_aliyun_at_user_wifi_join_start();
        return 0u;
    }

    jr = cloud_aliyun_at_user_wifi_join_poll();
    if(jr == 2u) {
        if(cloud_aliyun_at_user_wifi_join_fail_pending() != 0u) {
            /* 保持 jr=2，立即收尾 */
        } else if(cloud_aliyun_at_wifi_link_ready() != 0u) {
            jr = 1u;
        } else if(cloud_aliyun_at_user_wifi_join_active() != 0u) {
            jr = 0u;
        }
    }
    if(jr == 0u) {
        static uint32_t s_conn_stale_ms;
        static uint32_t s_conn_wait_log_ms;
        uint32_t now = HAL_GetTick();

        if(cloud_aliyun_at_user_wifi_join_fail_pending() != 0u) {
            app_wifi_connect_fail(2u);
            return 2u;
        }
        if(cloud_aliyun_at_user_wifi_join_active() == 0u &&
           cloud_aliyun_at_wifi_bringup_active() == 0u &&
           cloud_aliyun_at_wifi_link_ready() == 0u &&
           s_conn_phase == CONN_PHASE_JOIN) {
            if(s_conn_stale_ms == 0u) {
                s_conn_stale_ms = now;
            } else if((now - s_conn_stale_ms) >= 2500u) {
                char rx_hint[96];
                s_conn_stale_ms = 0u;
                cloud_uart2_copy_rx_win(rx_hint, sizeof(rx_hint));
                if(strstr(rx_hint, "NO AP") != NULL || strstr(rx_hint, "no ap") != NULL) {
                    WIFI_DBG("connect fail: No AP (SSID/密码/2.4GHz?)");
#if (APP_WIFI_CWJAP_TRACE != 0)
                    usart_debug_tx_str("[WiFi] connect fail detail: NO AP\r\n");
#endif
                } else if(strstr(rx_hint, "busy p") != NULL) {
                    WIFI_DBG("connect fail: ESP busy (scan tail? retry)");
#if (APP_WIFI_CWJAP_TRACE != 0)
                    usart_debug_tx_str("[WiFi] connect fail detail: busy p\r\n");
#endif
                } else {
                    WIFI_DBG("connect fail: join ended (see rx)");
#if (APP_WIFI_CWJAP_TRACE != 0)
                    usart_debug_tx_str("[WiFi] connect fail detail: join ended\r\n");
#endif
                }
                app_wifi_connect_fail(2u);
                return 2u;
            }
        } else {
            s_conn_stale_ms = 0u;
        }
        if(s_conn_wait_log_ms == 0u || (now - s_conn_wait_log_ms) >= 3000u) {
            s_conn_wait_log_ms = now;
            WIFI_DBG("connect wait join_st=%u bringup=%u scan=%u async=%u ui=%u",
                     (unsigned)cloud_aliyun_at_user_wifi_join_state(),
                     (unsigned)cloud_aliyun_at_wifi_bringup_active(),
                     (unsigned)app_wifi_scan_busy(),
                     (unsigned)cloud_aliyun_at_cwlap_scan_async_active(),
                     (unsigned)cloud_uart2_ui_busy());
            cloud_aliyun_at_wifi_join_diag_printf();
        }
        if(HAL_GetTick() >= s_conn_deadline) {
            s_conn_wait_log_ms = 0u;
            char rx_tail[128];
            cloud_uart2_copy_rx_win(rx_tail, sizeof(rx_tail));
            WIFI_DBG("connect timeout ssid=%s pwd_len=%u phase=%u join=%u rx=%s",
                     app_wifi_cfg_get_ssid(),
                     (unsigned)strlen(app_wifi_cfg_get_password()),
                     (unsigned)s_conn_phase,
                     (unsigned)cloud_aliyun_at_user_wifi_join_active(),
                     rx_tail);
            cloud_aliyun_at_wifi_join_diag_printf();
            app_wifi_connect_fail(2u);
            return 2u;
        }
        return 0u;
    }
    if(jr == 2u) {
        app_wifi_connect_fail(2u);
        return 2u;
    }
    s_conn_result = jr;
    s_conn_st = CONN_ST_IDLE;
    s_conn_phase = CONN_PHASE_IDLE;
    s_conn_finish_r = jr;
    s_conn_finish_pending = 1u;
    app_wifi_scan_resume_after_connect();
    if(jr == 1u) {
        app_link_guard_wifi_end(1u);
        screen_wifi_notify_sta_up();
    }
    return jr;
#else
    return 2u;
#endif
}

#else /* APP_WIFI_UI_SCAN_ENABLE == 0 */

void app_wifi_scan_ui_set_active(uint8_t on)
{
    (void)on;
}

uint8_t app_wifi_scan_ui_active(void)
{
    return 0u;
}

void app_wifi_scan_reset(void) { (void)0; }
void app_wifi_scan_scr11_leave(void) { (void)0; }
void app_wifi_scan_on_cloud_online(void) { (void)0; }
uint8_t app_wifi_scan_busy(void) { return 0u; }
void app_wifi_scan_kick(void) { (void)0; }
void app_wifi_scan_request_now(void) { (void)0; }
void app_wifi_scan_mark_user_refresh(void) { (void)0; }
void app_wifi_scan_kick_ui(void) { (void)0; }
void app_wifi_scan_run_blocking(void) { (void)0; }
void app_wifi_scan_poll(void) { (void)0; }
void app_wifi_scan_worker_run(void) { (void)0; }
void app_wifi_scan_service(void) { (void)0; }
void app_wifi_scan_stuck_recover(void) { (void)0; }
void app_wifi_scan_gui_tick(void) { (void)0; }
void app_wifi_scan_cloud_tick(void) { (void)0; }
uint8_t app_wifi_scan_uart_busy(void) { return 0u; }
void app_wifi_scan_bind_cloud_task(TaskHandle_t task) { (void)task; }
void app_wifi_scan_notify_rx(const char *buf, uint16_t len)
{
    (void)buf;
    (void)len;
}
void app_wifi_cloud_tick(void) { (void)0; }
uint8_t app_wifi_exclusive_mode(void) { return 0u; }
uint8_t app_wifi_scan_count(void) { return 0u; }
const app_wifi_ap_t *app_wifi_scan_get(uint8_t index) { (void)index; return NULL; }
uint8_t app_wifi_scan_take_list_dirty(void) { return 0u; }
uint8_t app_wifi_scan_peek_list_dirty(void) { return 0u; }
uint8_t app_wifi_scan_pass_completed(void) { return 0u; }
uint8_t app_wifi_scan_has_pending(void) { return 0u; }
uint8_t app_wifi_scan_in_rescan_wait(void) { return 0u; }
uint8_t app_wifi_scan_has_ssid(const char *ssid) { (void)ssid; return 0u; }
void app_wifi_scan_defer_rescan_ms(uint32_t ms) { (void)ms; }
void app_wifi_scan_drop_for_connect(void) { (void)0; }
void app_wifi_scan_abort_for_mqtt(void) { (void)0; }
uint8_t app_wifi_scan_abort_and_wait_idle(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return 1u;
}
void app_wifi_scan_on_sta_link_down(void) { (void)0; }
void app_wifi_connect_reset(void) { (void)0; }
void app_wifi_scan_release_uart2(void) { (void)0; }
uint8_t app_wifi_connect_busy(void) { return 0u; }
uint8_t app_wifi_scan_connect_hold_active(void) { return 0u; }
uint8_t app_wifi_scan_connect_hold_scan_only(void) { return 0u; }
void app_wifi_scan_release_connect_hold(void) { (void)0; }
void app_wifi_connect_gui_recover(void) { (void)0; }
void app_wifi_connect_start(const char *ssid, const char *password) { (void)ssid; (void)password; }
uint8_t app_wifi_connect_poll(void) { return 2u; }
uint8_t app_wifi_connect_take_result(void) { return 0u; }

#endif /* APP_WIFI_UI_SCAN_ENABLE */
