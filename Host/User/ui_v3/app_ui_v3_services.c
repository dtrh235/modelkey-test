#include "app_ui_v3_services.h"
#include "app_ui_v3_screens.h"

#if (APP_UI_V3_ENABLE == 1)

#include "app_ui_v3.h"
#include "app_ui_v3_widgets.h"
#include "app_ui_v3_anim.h"
#include "app_pair_bind.h"
#include "app_wifi_scan.h"
#include "app_wifi_remember.h"
#include "app_wifi_cfg.h"
#include "cloud_aliyun_at.h"
#include "app_link_guard.h"
#include "app_wifi_diag.h"
#include "app_state.h"
#include <string.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"

#if (APP_WIFI_UART_DEBUG == 0)
#undef printf
#define printf(...) ((void)0)
#endif

static char s_wifi_modal_ssid[33];
static uint8_t s_wifi_modal_on;
static char s_pair_status[48];
static uint32_t s_wifi_scan_ui_since_ms;
static volatile uint8_t s_wifi_pending_sta_up;
static volatile uint8_t s_wifi_pending_sta_down;
static volatile uint8_t s_wifi_pending_connect_fail;

static void ui3_wifi_connect_ok_ui(ui3_state_t *st)
{
    if(st == NULL) {
        return;
    }
    ui3_wifi_pwd_modal_close();
    st->wifi_modal = 0u;
    st->wifi_pending_ssid[0] = '\0';
    ui3_wifi_show_connect_result(st, 1u);
    ui3_wifi_refresh_scan_lbl(st);
}

static void ui3_wifi_connect_fail_ui(ui3_state_t *st)
{
    if(st == NULL) {
        return;
    }
    ui3_wifi_pwd_modal_close();
    st->wifi_modal = 0u;
    ui3_wifi_show_connect_result(st, 0u);
    ui3_wifi_refresh_list(st);
}

void ui3_wifi_show_connect_result(ui3_state_t *st, uint8_t ok)
{
    if(st == NULL) {
        return;
    }
    st->wifi_scanning = 0u;
    if(st->wifi_scan_lbl != NULL && lv_obj_is_valid(st->wifi_scan_lbl)) {
        lv_label_set_text(st->wifi_scan_lbl, ok != 0u ? "连接成功" : "连接失败");
    }
}

void ui3_wifi_notify_sta_up(void)
{
    /* CloudTask 只置位；LVGL 在 GuiTask 的 ui3_services_poll 里处理 */
    s_wifi_pending_sta_up = 1u;
}

void ui3_wifi_notify_connect_fail(void)
{
    s_wifi_pending_connect_fail = 1u;
}

void ui3_wifi_notify_sta_down(void)
{
    s_wifi_pending_sta_down = 1u;
}

uint8_t ui3_wifi_scan_ui_active(ui3_state_t *st)
{
    uint32_t now;

    if(st == NULL || st->scr != UI3_SCR_WIFI) {
        return 0u;
    }
    if(app_wifi_scan_busy() != 0u ||
       cloud_aliyun_at_cwlap_scan_async_active() != 0u) {
        s_wifi_scan_ui_since_ms = HAL_GetTick();
        return 1u;
    }
    if(st->wifi_scanning == 0u || app_wifi_scan_has_pending() == 0u) {
        s_wifi_scan_ui_since_ms = 0u;
        return 0u;
    }
    now = HAL_GetTick();
    if(s_wifi_scan_ui_since_ms == 0u) {
        s_wifi_scan_ui_since_ms = now;
    }
    /* pending 但模组被云端占用时，最多显示 6s「扫描中」后退回「刷新列表」 */
    if((now - s_wifi_scan_ui_since_ms) > 6000u) {
        return 0u;
    }
    return 1u;
}

static uint8_t wifi_rssi_to_bars(int8_t rssi)
{
    if(rssi >= -55) {
        return 4u;
    }
    if(rssi >= -65) {
        return 3u;
    }
    if(rssi >= -75) {
        return 2u;
    }
    return 1u;
}

void ui3_services_sync_cloud(ui3_state_t *st)
{
    uint8_t online;
    uint8_t wifi_still_up;
    static uint32_t s_cloud_seen_ms;
    uint32_t now;

    if(st == NULL) {
        return;
    }
    now = HAL_GetTick();
    wifi_still_up = (uint8_t)(cloud_aliyun_at_wifi_link_ready() != 0u ||
                              cloud_aliyun_at_wifi_ui_up() != 0u);
    if(s_wifi_pending_sta_down != 0u) {
        s_wifi_pending_sta_down = 0u;
        s_cloud_seen_ms = 0u;
        online = 0u;
    } else if(cloud_aliyun_at_wifi_ui_up() == 0u &&
              cloud_aliyun_at_wifi_was_up() != 0u) {
        s_cloud_seen_ms = 0u;
        online = 0u;
    } else if(wifi_still_up == 0u) {
        s_cloud_seen_ms = 0u;
        online = 0u;
    } else if(app_pair_cloud_ready() != 0u && cloud_aliyun_at_wifi_ui_up() != 0u) {
        s_cloud_seen_ms = now;
        online = 1u;
    } else if(s_cloud_seen_ms != 0u && (now - s_cloud_seen_ms) < 20000u &&
              cloud_aliyun_at_wifi_link_ready() != 0u &&
              cloud_aliyun_at_wifi_ui_up() != 0u) {
        /* MQTT 发 CIPSEND 时 step 会短暂离开 ONLINE，避免顶栏闪「离线」 */
        online = 1u;
    } else {
        online = 0u;
    }
    if(online != st->mqtt_online) {
        st->mqtt_online = online;
        ui3_topbar_cloud_refresh(st);
    }
}

void ui3_services_poll(ui3_state_t *st)
{
    static uint8_t s_last_dirty;
    static uint32_t s_wifi_reload_ms;
    static uint8_t s_wifi_last_scan_busy;
    static uint8_t s_wifi_was_linked;
    uint32_t now;
    uint8_t wifi_reload;
    uint8_t linked;

    if(st == NULL) {
        return;
    }
    ui3_services_sync_cloud(st);
    now = HAL_GetTick();
    wifi_reload = 0u;
    linked = cloud_aliyun_at_wifi_link_ready();

    if(st->scr == UI3_SCR_WIFI) {
        if(s_wifi_pending_sta_up != 0u) {
            s_wifi_pending_sta_up = 0u;
            ui3_wifi_connect_ok_ui(st);
        }
        if(s_wifi_pending_connect_fail != 0u) {
            s_wifi_pending_connect_fail = 0u;
            ui3_wifi_connect_fail_ui(st);
        }
        if(linked != 0u) {
            if(s_wifi_was_linked == 0u) {
                st->wifi_scanning = 0u;
                s_wifi_scan_ui_since_ms = 0u;
                ui3_wifi_refresh_scan_lbl(st);
            } else if(st->wifi_scanning != 0u && app_wifi_scan_busy() == 0u &&
                      cloud_aliyun_at_cwlap_scan_async_active() == 0u) {
                st->wifi_scanning = 0u;
                ui3_wifi_refresh_scan_lbl(st);
            }
        }
        s_wifi_was_linked = linked;

        app_wifi_scan_gui_tick();
        app_wifi_connect_gui_recover();

        if(st->wifi_modal == 0u) {
            if(app_wifi_remember_scr11_poll() != 0u) {
                ui3_wifi_connect_ok_ui(st);
            } else {
                uint8_t conn_r = app_wifi_connect_take_result();
                uint8_t busy = app_wifi_scan_busy();

                if(conn_r == 2u) {
                    ui3_wifi_connect_fail_ui(st);
                } else if(conn_r == 1u) {
                    ui3_wifi_connect_ok_ui(st);
                } else if(app_wifi_scan_take_list_dirty() != 0u) {
                    st->wifi_scanning = 0u;
                    s_wifi_scan_ui_since_ms = 0u;
                    wifi_reload = 1u;
                } else if(linked == 0u && s_wifi_last_scan_busy != 0u && busy == 0u) {
                    st->wifi_scanning = 0u;
                    s_wifi_scan_ui_since_ms = 0u;
                    app_wifi_scan_release_uart2();
                    wifi_reload = 1u;
                } else if(linked == 0u && st->wifi_scanning != 0u && busy == 0u &&
                          app_wifi_scan_has_pending() == 0u &&
                          cloud_aliyun_at_cwlap_scan_async_active() == 0u) {
                    st->wifi_scanning = 0u;
                    s_wifi_scan_ui_since_ms = 0u;
                    wifi_reload = 1u;
                } else if(st->wifi_scanning != 0u && ui3_wifi_scan_ui_active(st) == 0u) {
                    st->wifi_scanning = 0u;
                    s_wifi_scan_ui_since_ms = 0u;
                    ui3_wifi_refresh_scan_lbl(st);
                }
                s_wifi_last_scan_busy = busy;
            }
        } else {
            s_wifi_last_scan_busy = app_wifi_scan_busy();
        }

        if(wifi_reload != 0u && (now - s_wifi_reload_ms) >= 280u) {
            s_wifi_reload_ms = now;
            ui3_wifi_refresh_list(st);
        }
    } else {
        s_wifi_last_scan_busy = 0u;
        s_wifi_was_linked = 0u;
    }

    if(st->scr == UI3_SCR_PAIR) {
        ui3_pair_sync(st);
        if(g_pair_ui_dirty != 0u) {
            g_pair_ui_dirty = 0u;
            ui3_reload_current();
        }
    }

    if(s_last_dirty != g_pair_ui_dirty) {
        s_last_dirty = g_pair_ui_dirty;
    }
}

void ui3_wifi_on_enter(ui3_state_t *st)
{
    uint8_t linked;

    if(st == NULL) {
        return;
    }
    WIFI_TRACE("ui3 enter");
    printf("[WiFi] ui3 enter scr=%u link=%u\r\n",
           (unsigned)g_app_scr,
           (unsigned)cloud_aliyun_at_wifi_link_ready());
    st->wifi_index = 0u;
    st->wifi_scanning = 0u;
    st->wifi_pwd[0] = '\0';
    st->wifi_modal = 0u;
    st->wifi_pending_ssid[0] = '\0';
    s_wifi_modal_on = 0u;
    s_wifi_modal_ssid[0] = '\0';
    ui3_wifi_pwd_modal_close();

    app_wifi_scan_ui_set_active(1u);
    cloud_aliyun_at_scr11_enter();

    linked = cloud_aliyun_at_wifi_link_ready();
    if(linked != 0u) {
        /* 已连 WiFi：勿 reset exclusive / connect，避免打断 MQTT */
        st->wifi_scanning = 0u;
        WIFI_TRACE("ui3 enter linked skip scan");
        return;
    }

    app_wifi_scan_reset();
    app_wifi_connect_reset();
    app_wifi_remember_scr11_reset();
    app_wifi_cfg_init_defaults();
    WIFI_TRACE("ui3 enter auto scan");
    ui3_wifi_request_scan(st);
}

void ui3_wifi_on_leave(void)
{
    ui3_state_t *st = ui3_state();

    ui3_wifi_pwd_modal_close();
    if(st != NULL) {
        st->wifi_modal = 0u;
        st->wifi_pwd[0] = '\0';
        st->wifi_pending_ssid[0] = '\0';
        st->wifi_scanning = 0u;
    }
    s_wifi_modal_on = 0u;
    s_wifi_modal_ssid[0] = '\0';
    s_wifi_scan_ui_since_ms = 0u;
    app_wifi_scan_scr11_leave();
    cloud_aliyun_at_scr11_leave();
}

uint8_t ui3_wifi_row_count(void)
{
    if(app_wifi_scan_count() > 0u) {
        return app_wifi_scan_count();
    }
    if(cloud_aliyun_at_wifi_link_ready() != 0u) {
        const char *ssid = app_wifi_cfg_get_ssid();
        if(ssid != NULL && ssid[0] != '\0') {
            return 1u;
        }
    }
    return 0u;
}

uint8_t ui3_wifi_row_at(uint8_t idx, ui3_wifi_row_t *out)
{
    const app_wifi_ap_t *ap;
    const char *ssid;

    if(out == NULL) {
        return 0u;
    }
    memset(out, 0, sizeof(*out));
    if(app_wifi_scan_count() > 0u) {
        if(idx >= app_wifi_scan_count()) {
            return 0u;
        }
        ap = app_wifi_scan_get(idx);
        if(ap == NULL) {
            return 0u;
        }
        (void)strncpy(out->ssid, ap->ssid, sizeof(out->ssid) - 1u);
        out->rssi = ap->rssi;
        out->connected = ui3_wifi_row_connected(out->ssid);
        return 1u;
    }
    if(idx != 0u) {
        return 0u;
    }
    ssid = app_wifi_cfg_get_ssid();
    if(ssid == NULL || ssid[0] == '\0') {
        return 0u;
    }
    (void)strncpy(out->ssid, ssid, sizeof(out->ssid) - 1u);
    out->rssi = -60;
    /* 无扫描结果时仅展示“记住的 SSID”；connected 必须跟随真实链路状态 */
    out->connected = (cloud_aliyun_at_wifi_link_ready() != 0u) ? 1u : 0u;
    return 1u;
}

uint8_t ui3_wifi_rssi_level(int8_t rssi)
{
    return wifi_rssi_to_bars(rssi);
}

uint8_t ui3_wifi_row_connected(const char *ssid)
{
    const char *cur;

    if(ssid == NULL || ssid[0] == '\0') {
        return 0u;
    }
    if(cloud_aliyun_at_wifi_link_ready() == 0u) {
        return 0u;
    }
    cur = app_wifi_cfg_get_ssid();
    if(cur == NULL || cur[0] == '\0') {
        return 0u;
    }
    return (strcmp(cur, ssid) == 0) ? 1u : 0u;
}

void ui3_wifi_request_scan(ui3_state_t *st)
{
    if(st == NULL) {
        return;
    }
    WIFI_TRACE("ui3 request scan");
    if(app_wifi_scan_busy() != 0u) {
        WIFI_TRACE("ui3 req busy");
        return;
    }
    if(cloud_aliyun_at_wifi_link_ready() == 0u) {
        cloud_aliyun_at_scr11_enter();
    }
    app_wifi_connect_gui_recover();
    if(app_wifi_connect_busy() != 0u) {
        if(cloud_aliyun_at_user_wifi_join_active() != 0u ||
           cloud_aliyun_at_wifi_bringup_active() != 0u) {
            WIFI_TRACE("ui3 req conn busy");
            return;
        }
        app_wifi_connect_reset();
    }
    if(app_link_guard_mqtt() != 0u) {
        app_link_guard_mqtt_end(0u);
    }
    if(app_link_guard_blocks_scan() != 0u) {
        WIFI_TRACE("ui3 req link_guard");
        printf("[WiFi] ui3 refresh blocked: link guard\r\n");
        return;
    }
    app_wifi_scan_release_connect_hold();
    g_wifi_scan_abort = 0u;
    st->wifi_scanning = 1u;
    s_wifi_scan_ui_since_ms = HAL_GetTick();
    app_wifi_scan_mark_user_refresh();
    app_wifi_scan_request_now();
}

static void wifi_begin_connect(const char *ssid, const char *pwd)
{
    if(ssid == NULL || ssid[0] == '\0') {
        return;
    }
    app_wifi_cfg_set(ssid, pwd != NULL ? pwd : "");
    app_wifi_connect_start(ssid, pwd != NULL ? pwd : "");
}

void ui3_wifi_try_connect(ui3_state_t *st)
{
    ui3_wifi_row_t row;

    if(st == NULL || app_wifi_connect_busy() != 0u || app_wifi_scan_busy() != 0u) {
        return;
    }
    if(ui3_wifi_row_at(st->wifi_index, &row) == 0u) {
        return;
    }

    (void)strncpy(st->wifi_pending_ssid, row.ssid, sizeof(st->wifi_pending_ssid) - 1u);
    st->wifi_pending_ssid[sizeof(st->wifi_pending_ssid) - 1u] = '\0';
    st->wifi_pwd[0] = '\0';
    st->wifi_modal = 1u;
    (void)strncpy(s_wifi_modal_ssid, row.ssid, sizeof(s_wifi_modal_ssid) - 1u);
    s_wifi_modal_ssid[sizeof(s_wifi_modal_ssid) - 1u] = '\0';
    s_wifi_modal_on = 1u;
    app_wifi_remember_manual_begin(row.ssid);
}

uint8_t ui3_wifi_modal_active(void)
{
    return s_wifi_modal_on;
}

void ui3_wifi_modal_close(void)
{
    s_wifi_modal_on = 0u;
    s_wifi_modal_ssid[0] = '\0';
}

const char *ui3_wifi_modal_ssid(void)
{
    return s_wifi_modal_ssid;
}

void ui3_wifi_modal_confirm(ui3_state_t *st)
{
    if(st == NULL || st->wifi_pending_ssid[0] == '\0') {
        return;
    }
    wifi_begin_connect(st->wifi_pending_ssid, st->wifi_pwd);
    st->wifi_modal = 0u;
    ui3_wifi_modal_close();
}

void ui3_pair_on_enter(ui3_state_t *st)
{
    if(st == NULL) {
        return;
    }
    if(cloud_aliyun_at_wifi_link_ready() != 0u) {
        cloud_aliyun_at_request_mqtt_connect();
    }
    ui3_pair_sync(st);
}

void ui3_pair_sync(ui3_state_t *st)
{
    const char *code;

    if(st == NULL) {
        return;
    }
    ui3_services_sync_cloud(st);
    app_pair_ensure_code();
    code = app_pair_get_code();
    if(code != NULL) {
        size_t n = strlen(code);
        if(n > 6u) {
            n = 6u;
        }
        memcpy(st->pair_code, code, n);
        st->pair_code[n] = '\0';
    } else {
        st->pair_code[0] = '\0';
    }
}

void ui3_pair_regen(ui3_state_t *st)
{
    (void)st;
    if(app_pair_is_bound() != 0u) {
        app_pair_regenerate_code();
        if(cloud_aliyun_at_wifi_link_ready() != 0u && app_pair_cloud_ready() == 0u) {
            cloud_aliyun_at_request_mqtt_connect();
        }
        return;
    }
    if(cloud_aliyun_at_wifi_link_ready() == 0u) {
        return;
    }
    app_pair_regenerate_code();
    if(app_pair_cloud_ready() == 0u) {
        cloud_aliyun_at_request_mqtt_connect();
    }
}

const char *ui3_pair_status_line(void)
{
    uint8_t wifi_ok = cloud_aliyun_at_wifi_link_ready();
    uint8_t cloud_ok = app_pair_cloud_ready();

    s_pair_status[0] = '\0';
    if(app_pair_is_bound() != 0u) {
        if(cloud_ok != 0u) {
            (void)snprintf(s_pair_status, sizeof(s_pair_status), "已绑定");
        } else if(wifi_ok != 0u) {
            (void)snprintf(s_pair_status, sizeof(s_pair_status), "云端重连中");
        } else {
            (void)snprintf(s_pair_status, sizeof(s_pair_status), "已绑定·WiFi离线");
        }
        return s_pair_status;
    }
    if(wifi_ok == 0u) {
        (void)snprintf(s_pair_status, sizeof(s_pair_status), "请先完成WiFi设置");
        return s_pair_status;
    }
    if(cloud_ok == 0u) {
        (void)snprintf(s_pair_status, sizeof(s_pair_status), "不可配对");
    } else {
        (void)snprintf(s_pair_status, sizeof(s_pair_status), "可配对");
    }
    return s_pair_status;
}

#endif
