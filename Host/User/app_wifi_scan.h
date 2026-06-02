#ifndef APP_WIFI_SCAN_H
#define APP_WIFI_SCAN_H

#include <stdint.h>

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

#define APP_WIFI_SCAN_MAX   20u

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t channel; /* 0=未知；来自 CWLAP 解析 */
} app_wifi_ap_t;

void app_wifi_scan_reset(void);
/* 进入/离开 WiFi 页（Cloud 任务据此判断是否允许 CWLAP） */
void app_wifi_scan_ui_set_active(uint8_t on);
uint8_t app_wifi_scan_ui_active(void);
/* 离开 WiFi 页：取消未执行的扫描请求 */
void app_wifi_scan_scr11_leave(void);
/* 云端 MQTT 上线后（仍在 WiFi 页）再启动 CWLAP 刷新 */
void app_wifi_scan_on_cloud_online(void);
uint8_t app_wifi_scan_busy(void);
/* 1=连接/建链占用中，扫描应等待（不含已排队的 pending） */
uint8_t app_wifi_scan_connect_hold_active(void);
/* 1=仅 s_scan_hold_connect（诊断用） */
uint8_t app_wifi_scan_connect_hold_scan_only(void);
void app_wifi_scan_release_connect_hold(void);
/* 1=WiFi 页/扫描/连接进行中：Cloud 仅跑 WiFi，禁止 MQTT/OTA/UART2 抢占 */
uint8_t app_wifi_exclusive_mode(void);
/* 仅 scr11 有效；其它界面调用无效 */
void app_wifi_scan_kick(void);
/* 刷新按钮：立即排队一次 CWLAP（不依赖 kick 内部条件） */
void app_wifi_scan_request_now(void);
/* 用户点「刷新」：下次扫描结束强制按新结果重绘列表（失败则清空） */
void app_wifi_scan_mark_user_refresh(void);
/* 进入 WiFi 设置页时调用 */
void app_wifi_scan_kick_ui(void);
void app_wifi_scan_run_blocking(void);
void app_wifi_scan_poll(void);
void app_wifi_scan_worker_run(void);
void app_wifi_scan_service(void);
void app_wifi_scan_stuck_recover(void);
/* WiFi 页：GuiTask 仅置标志，不做 UART（避免阻塞触屏） */
void app_wifi_scan_gui_tick(void);
/* CloudTask：CWLAP/CWJAP 轮询（含阻塞 AT，禁止在 GuiTask 调用） */
void app_wifi_scan_cloud_tick(void);
/* 1=扫描/连接/join 需要 CloudTask 跑 UART */
uint8_t app_wifi_scan_uart_busy(void);
/* 扫描收包过程中增量解析并刷新 UI 列表 */
void app_wifi_scan_notify_rx(const char *buf, uint16_t len);
#if (APP_USE_FREERTOS == 1)
void app_wifi_scan_bind_cloud_task(TaskHandle_t task);
#endif
void app_wifi_cloud_tick(void);
uint8_t app_wifi_scan_count(void);
const app_wifi_ap_t *app_wifi_scan_get(uint8_t index);
/* ????????????????UI ???????? */
uint8_t app_wifi_scan_take_list_dirty(void);
uint8_t app_wifi_scan_peek_list_dirty(void);
/* ??????????? CWLAP??????? AT ??????????? */
uint8_t app_wifi_scan_pass_completed(void);
int app_wifi_scan_last_rc(void);
uint8_t app_wifi_scan_last_failed(void);
uint8_t app_wifi_scan_has_pending(void);
/* 1=两次扫描间隔内，尚未到下次重扫时刻 */
uint8_t app_wifi_scan_in_rescan_wait(void);
uint8_t app_wifi_scan_has_ssid(const char *ssid);
/* 扫描列表中查 SSID 对应信道；无则返回 0 */
uint8_t app_wifi_scan_get_ssid_channel(const char *ssid);

/* Push next CWLAP refresh later (auto-connect batch uses this) */
void app_wifi_scan_defer_rescan_ms(uint32_t ms);
/* 中止 CWLAP 并等待 UART2/模组空闲（连接前必须调用） */
uint8_t app_wifi_scan_abort_and_wait_idle(uint32_t timeout_ms);
/* CloudTask 内调用：仅停扫/清标志，不 poll（避免 mutex 重入） */
void app_wifi_scan_drop_for_connect(void);
/* 扫描/UI 结束后释放 UART2（清 async/ui_busy/尾包），便于下次扫描或连接 */
void app_wifi_scan_release_uart2(void);

/* STA 掉线：停扫/停连，清 UART2 占用（不改变 g_wifi_exclusive） */
void app_wifi_scan_on_sta_link_down(void);
void app_wifi_connect_reset(void);
uint8_t app_wifi_connect_busy(void);
/* GuiTask：CWJAP 已失败时立刻释放 connect_busy / link guard，恢复扫描 */
void app_wifi_connect_gui_recover(void);
void app_wifi_connect_start(const char *ssid, const char *password);
/* 0=busy 1=ok 2=fail */
uint8_t app_wifi_connect_poll(void);
/* 连接结束且 idle 时取一次结果（1=ok 2=fail），供 GuiTask 更新 UI */
uint8_t app_wifi_connect_take_result(void);

/* Debug helpers removed to save Flash */

#endif
