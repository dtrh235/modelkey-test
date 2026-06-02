#include "app_wifi_remember.h"

#include <stdio.h>
#include <string.h>

#include "./BSP/W25Q16/bsp_w25q16.h"
#include "app_wifi_scan.h"
#include "app_wifi_cfg.h"
#include "app_config.h"
#include "app_screen.h"
#include "app_state.h"
#include "app_screen_wifi_flow.h"
#include "cloud_aliyun_at.h"
#include "app_wifi_diag.h"
#include "stm32f4xx_hal.h"

#define WIFI_REMEMBER_FLASH_ADDR  ((uint32_t)0x00012000u)
#define WIFI_REMEMBER_MAGIC       (0x33575246u) /* WR3F */
#define WIFI_REMEMBER_VERSION     (1u)
#define AUTO_CONNECT_TIMEOUT_MS   25000u
#define AUTO_BATCH_SCAN_PAD_MS    1200u
#define AUTO_BATCH_DONE_GAP_MS    5000u

typedef struct {
    uint8_t valid;
    uint8_t reserved[3];
    uint32_t connect_serial;
    char ssid[33];
    char pwd[65];
} wifi_remember_slot_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t next_serial;
    wifi_remember_slot_t slots[APP_WIFI_REMEMBER_SLOTS];
    uint32_t tail;
} wifi_remember_blob_t;

typedef struct {
    int8_t slot_idx[APP_WIFI_REMEMBER_SLOTS];
    uint8_t count;
    uint8_t next_try;
} wifi_auto_batch_t;

static wifi_remember_blob_t s_blob;
static uint8_t s_loaded = 0u;
static uint8_t s_flash_valid = 0u;
static uint8_t s_scr11_auto_done = 0u;
static uint8_t s_manual_lock = 0u;
static char s_manual_ssid[33];
static uint8_t s_failed_slot_mask = 0u;
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
static wifi_auto_batch_t s_batch;
#endif

static void blob_reset(void)
{
    memset(&s_blob, 0, sizeof(s_blob));
    s_blob.magic = WIFI_REMEMBER_MAGIC;
    s_blob.version = WIFI_REMEMBER_VERSION;
    s_blob.next_serial = 1u;
    s_blob.tail = WIFI_REMEMBER_MAGIC ^ 0xA5A55A5Au;
}

static uint8_t blob_valid(const wifi_remember_blob_t *b)
{
    if(b == NULL) {
        return 0u;
    }
    if(b->magic != WIFI_REMEMBER_MAGIC || b->version != WIFI_REMEMBER_VERSION) {
        return 0u;
    }
    if(b->tail != (WIFI_REMEMBER_MAGIC ^ 0xA5A55A5Au)) {
        return 0u;
    }
    return 1u;
}

static void remember_log_slots(const char *tag)
{
    uint8_t i;
    uint8_t n = 0u;

    REMEMBER_DBG("%s flash_valid=%u next_serial=%lu failed_mask=0x%X auto_done=%u manual=%u batch=%u",
                 tag,
                 (unsigned)s_flash_valid,
                 (unsigned long)s_blob.next_serial,
                 (unsigned)s_failed_slot_mask,
                 (unsigned)s_scr11_auto_done,
                 (unsigned)s_manual_lock,
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
                 (unsigned)s_batch.count);
#else
                 0u);
#endif
    for(i = 0u; i < APP_WIFI_REMEMBER_SLOTS; i++) {
        if(s_blob.slots[i].valid == 0u) {
            continue;
        }
        n++;
        REMEMBER_DBG("  slot[%u] ssid=%s serial=%lu",
                     (unsigned)i,
                     s_blob.slots[i].ssid,
                     (unsigned long)s_blob.slots[i].connect_serial);
    }
    if(n == 0u) {
        REMEMBER_DBG("  (no saved WiFi)");
    }
}

static uint8_t flash_save(void)
{
    const uint8_t *p = (const uint8_t *)&s_blob;
    uint32_t off = 0u;

    if(!bsp_w25q16_init()) {
        REMEMBER_DBG("flash save fail: w25q16 init");
        return 0u;
    }
    if(!bsp_w25q16_erase_sector_4k(WIFI_REMEMBER_FLASH_ADDR)) {
        bsp_w25q16_end_session();
        REMEMBER_DBG("flash save fail: erase 0x%lX", (unsigned long)WIFI_REMEMBER_FLASH_ADDR);
        return 0u;
    }
    while(off < sizeof(s_blob)) {
        uint16_t chunk = (uint16_t)((sizeof(s_blob) - off) > 256u ? 256u : (sizeof(s_blob) - off));
        if(!bsp_w25q16_write_page(WIFI_REMEMBER_FLASH_ADDR + off, p + off, chunk)) {
            bsp_w25q16_end_session();
            REMEMBER_DBG("flash save fail: write off=%lu", (unsigned long)off);
            return 0u;
        }
        off += chunk;
    }
    bsp_w25q16_end_session();
    s_flash_valid = 1u;
    return 1u;
}

static void flash_load(void)
{
    wifi_remember_blob_t tmp;

    if(s_loaded != 0u) {
        return;
    }
    s_loaded = 1u;
    blob_reset();
    s_flash_valid = 0u;
    if(!bsp_w25q16_init()) {
        REMEMBER_DBG("flash load fail: w25q16 init");
        return;
    }
    if(!bsp_w25q16_read(WIFI_REMEMBER_FLASH_ADDR, (uint8_t *)&tmp, sizeof(tmp))) {
        bsp_w25q16_end_session();
        REMEMBER_DBG("flash load fail: read 0x%lX", (unsigned long)WIFI_REMEMBER_FLASH_ADDR);
        return;
    }
    bsp_w25q16_end_session();
    if(blob_valid(&tmp) == 0u) {
        REMEMBER_DBG("flash load: empty or invalid blob at 0x%lX", (unsigned long)WIFI_REMEMBER_FLASH_ADDR);
        return;
    }
    s_blob = tmp;
    s_flash_valid = 1u;
    remember_log_slots("flash load OK");
}

static int8_t find_slot_by_ssid(const char *ssid)
{
    uint8_t i;

    if(ssid == NULL || ssid[0] == '\0') {
        return -1;
    }
    for(i = 0u; i < APP_WIFI_REMEMBER_SLOTS; i++) {
        if(s_blob.slots[i].valid != 0u && strcmp(s_blob.slots[i].ssid, ssid) == 0) {
            return (int8_t)i;
        }
    }
    return -1;
}

static int8_t find_empty_slot(void)
{
    uint8_t i;

    for(i = 0u; i < APP_WIFI_REMEMBER_SLOTS; i++) {
        if(s_blob.slots[i].valid == 0u) {
            return (int8_t)i;
        }
    }
    return -1;
}

static int8_t find_oldest_slot(void)
{
    uint8_t i;
    int8_t oldest = -1;
    uint32_t min_serial = 0xFFFFFFFFu;

    for(i = 0u; i < APP_WIFI_REMEMBER_SLOTS; i++) {
        if(s_blob.slots[i].valid == 0u) {
            continue;
        }
        if(s_blob.slots[i].connect_serial <= min_serial) {
            min_serial = s_blob.slots[i].connect_serial;
            oldest = (int8_t)i;
        }
    }
    return oldest;
}

static void sort_slots_by_recent(int8_t order[APP_WIFI_REMEMBER_SLOTS], uint8_t *out_cnt)
{
    uint8_t cnt = 0u;
    uint8_t i;
    uint8_t j;

    for(i = 0u; i < APP_WIFI_REMEMBER_SLOTS; i++) {
        if(s_blob.slots[i].valid != 0u) {
            order[cnt++] = (int8_t)i;
        }
    }
    for(i = 0u; i < cnt; i++) {
        for(j = (uint8_t)(i + 1u); j < cnt; j++) {
            if(s_blob.slots[order[j]].connect_serial > s_blob.slots[order[i]].connect_serial) {
                int8_t t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    }
    *out_cnt = cnt;
}

static uint8_t remember_wifi_already_connected(void)
{
    /* �����õ� STA IP��CWJAP �ɹ��� CIFSR ��δ���ʱ���ܵ��������� */
    return cloud_aliyun_at_wifi_link_ready();
}

#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)

static void auto_batch_clear(void)
{
    if(s_batch.count != 0u) {
        REMEMBER_DBG("auto batch clear (had %u candidates)", (unsigned)s_batch.count);
    }
    memset(&s_batch, 0, sizeof(s_batch));
}

static void auto_batch_finish(uint8_t connected_ok)
{
    if(connected_ok != 0u) {
        REMEMBER_DBG("auto batch done: CONNECT OK");
    } else {
        REMEMBER_DBG("auto batch done: all candidates failed");
    }
    /* 无论成败，本次进入 WiFi 页只自动连一次，避免失败后反复 CWJAP */
    s_scr11_auto_done = 1u;
    auto_batch_clear();
    app_wifi_scan_defer_rescan_ms(AUTO_BATCH_DONE_GAP_MS);
}

static void auto_batch_mark_failed(uint8_t batch_idx)
{
    int8_t slot_idx;

    if(batch_idx >= s_batch.count) {
        return;
    }
    slot_idx = s_batch.slot_idx[batch_idx];
    if(slot_idx >= 0 && slot_idx < (int8_t)APP_WIFI_REMEMBER_SLOTS) {
        s_failed_slot_mask |= (uint8_t)(1u << (uint8_t)slot_idx);
        REMEMBER_DBG("auto connect FAIL ssid=%s (%u/%u) -> skip rest of visit",
                     s_blob.slots[slot_idx].ssid,
                     (unsigned)(batch_idx + 1u),
                     (unsigned)s_batch.count);
    }
}

static void auto_batch_build_from_scan(void)
{
    int8_t order[APP_WIFI_REMEMBER_SLOTS];
    uint8_t cnt = 0u;
    uint8_t i;

    auto_batch_clear();
    sort_slots_by_recent(order, &cnt);
    REMEMBER_DBG("scan match: saved=%u visible_in_scan checking...", (unsigned)cnt);
    for(i = 0u; i < cnt; i++) {
        int8_t slot_idx = order[i];
        const wifi_remember_slot_t *slot;

        if((s_failed_slot_mask & (uint8_t)(1u << (uint8_t)slot_idx)) != 0u) {
            REMEMBER_DBG("  skip ssid=%s (failed earlier this visit)",
                         s_blob.slots[slot_idx].ssid);
            continue;
        }
        slot = &s_blob.slots[slot_idx];
        if(app_wifi_scan_has_ssid(slot->ssid) == 0u) {
            REMEMBER_DBG("  skip ssid=%s (not in scan list)", slot->ssid);
            continue;
        }
        REMEMBER_DBG("  match ssid=%s serial=%lu -> batch[%u]",
                     slot->ssid,
                     (unsigned long)slot->connect_serial,
                     (unsigned)s_batch.count);
        s_batch.slot_idx[s_batch.count] = slot_idx;
        s_batch.count++;
    }
    s_batch.next_try = 0u;
    REMEMBER_DBG("scan match result: batch_count=%u", (unsigned)s_batch.count);
}

static void auto_batch_start_at(uint8_t batch_idx)
{
    int8_t slot_idx;
    const wifi_remember_slot_t *slot;
    uint32_t defer_ms;

    if(batch_idx >= s_batch.count) {
        return;
    }
    slot_idx = s_batch.slot_idx[batch_idx];
    slot = &s_blob.slots[slot_idx];
    defer_ms = AUTO_CONNECT_TIMEOUT_MS + AUTO_BATCH_SCAN_PAD_MS;
    defer_ms = (uint32_t)(defer_ms * (uint32_t)(s_batch.count - batch_idx));
    if(defer_ms < AUTO_BATCH_DONE_GAP_MS) {
        defer_ms = AUTO_BATCH_DONE_GAP_MS;
    }
    app_wifi_scan_defer_rescan_ms(defer_ms);
    REMEMBER_DBG("auto connect START ssid=%s (%u/%u) defer_scan=%lums",
                 slot->ssid,
                 (unsigned)(batch_idx + 1u),
                 (unsigned)s_batch.count,
                 (unsigned long)defer_ms);
    app_wifi_connect_start(slot->ssid, slot->pwd);
}

static void auto_batch_begin(void)
{
    if(s_batch.count == 0u) {
        REMEMBER_DBG("auto batch begin: no candidate");
        return;
    }
    REMEMBER_DBG("auto batch begin: try %u AP(s) this round", (unsigned)s_batch.count);
    auto_batch_start_at(0u);
}

static void auto_batch_try_resume(void)
{
    if(s_scr11_auto_done != 0u || s_manual_lock != 0u) {
        return;
    }
    if(screen_wifi_popup_is_active()) {
        return;
    }
    if(remember_wifi_already_connected() != 0u) {
        s_scr11_auto_done = 1u;
        return;
    }
    flash_load();
    REMEMBER_DBG("resume auto after manual fail");
    auto_batch_build_from_scan();
    if(s_batch.count == 0u) {
        REMEMBER_DBG("resume auto: no other candidate in scan list");
        return;
    }
    auto_batch_begin();
}

static void auto_batch_on_connect_fail(void)
{
    uint8_t done_idx;

    if(s_batch.count == 0u || s_batch.next_try >= s_batch.count) {
        auto_batch_finish(0u);
        return;
    }
    done_idx = s_batch.next_try;
    auto_batch_mark_failed(done_idx);
    s_batch.next_try = (uint8_t)(done_idx + 1u);
    if(s_batch.next_try < s_batch.count) {
        auto_batch_start_at(s_batch.next_try);
        return;
    }
    auto_batch_finish(0u);
}

#endif /* APP_WIFI_AUTO_CONNECT_ENABLE */

void app_wifi_remember_init(void)
{
    flash_load();
}

void app_wifi_remember_on_wifi_down(void)
{
    REMEMBER_DBG("STA down: clear scr11 connect state");
    s_scr11_auto_done = 0u;
    s_manual_lock = 0u;
    s_manual_ssid[0] = '\0';
    s_failed_slot_mask = 0u;
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
    auto_batch_clear();
#endif
}

void app_wifi_remember_scr11_reset(void)
{
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
    REMEMBER_DBG("scr11 enter: reset auto state");
    if(remember_wifi_already_connected() != 0u) {
        s_scr11_auto_done = 1u;
        s_manual_lock = 0u;
        s_failed_slot_mask = 0u;
        auto_batch_clear();
        return;
    }
    s_scr11_auto_done = 0u;
#else
    REMEMBER_DBG("scr11 enter: manual connect only");
#endif
    s_manual_lock = 0u;
    s_manual_ssid[0] = '\0';
    s_failed_slot_mask = 0u;
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
    auto_batch_clear();
#endif
}

uint8_t app_wifi_remember_autoconnect_busy(void)
{
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
    return (s_batch.count != 0u && s_scr11_auto_done == 0u && s_manual_lock == 0u) ? 1u : 0u;
#else
    return 0u;
#endif
}

void app_wifi_remember_popup_open(void)
{
    if(s_manual_lock != 0u) {
        return;
    }
    if(app_wifi_connect_busy() != 0u) {
        app_wifi_connect_reset();
    }
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
    REMEMBER_DBG("popup open: pause auto batch");
    auto_batch_clear();
#endif
}

void app_wifi_remember_manual_begin(const char *ssid)
{
    s_manual_lock = 1u;
    if(ssid != NULL) {
        strncpy(s_manual_ssid, ssid, sizeof(s_manual_ssid) - 1u);
        s_manual_ssid[sizeof(s_manual_ssid) - 1u] = '\0';
    } else {
        s_manual_ssid[0] = '\0';
    }
    REMEMBER_DBG("manual connect START ssid=%s", s_manual_ssid);
    if(app_wifi_connect_busy() != 0u) {
        app_wifi_connect_reset();
    }
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
    auto_batch_clear();
#endif
}

void app_wifi_remember_on_connect_success(const char *ssid, const char *password)
{
    int8_t idx;
    wifi_remember_slot_t *slot;
    const char *pwd = (password != NULL) ? password : "";
    uint8_t was_manual = s_manual_lock;

    flash_load();
    if(ssid == NULL || ssid[0] == '\0') {
        REMEMBER_DBG("connect success ignored: empty ssid");
        return;
    }

    idx = find_slot_by_ssid(ssid);
    if(idx < 0) {
        idx = find_empty_slot();
        if(idx < 0) {
            idx = find_oldest_slot();
            if(idx >= 0) {
                REMEMBER_DBG("flash full: evict oldest ssid=%s", s_blob.slots[idx].ssid);
            }
        } else {
            REMEMBER_DBG("flash add new slot[%d] ssid=%s", (int)idx, ssid);
        }
    } else {
        REMEMBER_DBG("flash update slot[%d] ssid=%s", (int)idx, ssid);
    }
    if(idx < 0) {
        REMEMBER_DBG("connect success: no flash slot available");
        return;
    }

    slot = &s_blob.slots[idx];
    memset(slot, 0, sizeof(*slot));
    slot->valid = 1u;
    strncpy(slot->ssid, ssid, sizeof(slot->ssid) - 1u);
    strncpy(slot->pwd, pwd, sizeof(slot->pwd) - 1u);
    slot->connect_serial = s_blob.next_serial++;

    if(flash_save() == 0u) {
        REMEMBER_DBG("flash save FAIL ssid=%s", ssid);
    } else {
        REMEMBER_DBG("flash save OK ssid=%s serial=%lu",
                     ssid, (unsigned long)slot->connect_serial);
        remember_log_slots("after save");
    }

    app_wifi_cfg_set(ssid, pwd);
    app_wifi_cfg_clear_reconnect_request();
    s_manual_lock = 0u;
    if(was_manual != 0u) {
        REMEMBER_DBG("manual connect OK ssid=%s", ssid);
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
        s_scr11_auto_done = 1u;
        auto_batch_clear();
#endif
    }
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
    else {
        REMEMBER_DBG("auto connect OK ssid=%s -> stop auto", ssid);
        auto_batch_finish(1u);
    }
#endif
}

uint8_t app_wifi_remember_load_primary(char *ssid, uint16_t ssid_sz,
                                       char *pwd, uint16_t pwd_sz)
{
    int8_t order[APP_WIFI_REMEMBER_SLOTS];
    uint8_t cnt = 0u;

    flash_load();
    if(ssid == NULL || ssid_sz < 2u || pwd == NULL || pwd_sz < 1u) {
        return 0u;
    }
    ssid[0] = '\0';
    pwd[0] = '\0';
    sort_slots_by_recent(order, &cnt);
    if(cnt == 0u) {
        REMEMBER_DBG("boot load primary: none saved");
        return 0u;
    }
    strncpy(ssid, s_blob.slots[order[0]].ssid, (size_t)(ssid_sz - 1u));
    strncpy(pwd, s_blob.slots[order[0]].pwd, (size_t)(pwd_sz - 1u));
    ssid[ssid_sz - 1u] = '\0';
    pwd[pwd_sz - 1u] = '\0';
    REMEMBER_DBG("boot load primary: ssid=%s", ssid);
    return 1u;
}

void app_wifi_remember_on_scan_done(void)
{
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 0)
    return;
#else
    if(g_app_scr != APP_SCR_11) {
        return;
    }
    if(s_scr11_auto_done != 0u) {
        REMEMBER_DBG("scan done: skip auto (already connected this visit)");
        return;
    }
    if(s_manual_lock != 0u) {
        REMEMBER_DBG("scan done: skip auto (manual lock)");
        return;
    }
    if(app_wifi_connect_busy() != 0u) {
        REMEMBER_DBG("scan done: skip auto (connect busy)");
        return;
    }
    if(app_wifi_remember_autoconnect_busy() != 0u) {
        REMEMBER_DBG("scan done: skip auto (batch in progress)");
        return;
    }
    if(screen_wifi_popup_is_active()) {
        REMEMBER_DBG("scan done: skip auto (password popup open)");
        return;
    }
    if(remember_wifi_already_connected() != 0u) {
        REMEMBER_DBG("scan done: skip auto (WiFi already up)");
        s_scr11_auto_done = 1u;
        return;
    }

    flash_load();
    REMEMBER_DBG("scan done: check remembered APs in list");
    auto_batch_build_from_scan();
    if(s_batch.count == 0u) {
        REMEMBER_DBG("scan done: no remembered AP in scan list");
        return;
    }
    auto_batch_begin();
#endif /* APP_WIFI_AUTO_CONNECT_ENABLE */
}

void app_wifi_remember_try_save_linked(void)
{
    const char *ssid;
    const char *pwd;

    flash_load();
    if(remember_wifi_already_connected() == 0u) {
        return;
    }
    ssid = app_wifi_cfg_get_ssid();
    pwd = app_wifi_cfg_get_password();
    if(ssid == NULL || ssid[0] == '\0') {
        return;
    }
    if(find_slot_by_ssid(ssid) >= 0) {
        return;
    }
    if(s_scr11_auto_done != 0u) {
        return;
    }
    REMEMBER_DBG("WiFi linked but not saved -> flash ssid=%s", ssid);
    app_wifi_remember_on_connect_success(ssid, pwd);
}

uint8_t app_wifi_remember_scr11_poll(void)
{
    uint8_t conn_r;

    if(g_app_scr != APP_SCR_11) {
        return 0u;
    }

    if(remember_wifi_already_connected() != 0u) {
        if(app_wifi_connect_busy() == 0u) {
            app_wifi_remember_try_save_linked();
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
            if(s_scr11_auto_done == 0u) {
                REMEMBER_DBG("poll: WiFi already up -> stop auto");
                s_scr11_auto_done = 1u;
                s_manual_lock = 0u;
                auto_batch_clear();
            }
#else
            s_manual_lock = 0u;
#endif
            return 0u;
        }
    }

    if(!app_wifi_connect_busy()) {
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 0)
        if(s_manual_lock == 0u) {
            return 0u;
        }
#endif
        conn_r = app_wifi_connect_take_result();
        if(conn_r == 0u) {
            return 0u;
        }
    } else {
        return 0u;
    }

    if(conn_r == 1u) {
        REMEMBER_DBG("poll: connect OK ssid=%s manual=%u",
                     app_wifi_cfg_get_ssid(),
                     (unsigned)s_manual_lock);
        app_wifi_remember_on_connect_success(app_wifi_cfg_get_ssid(),
                                             app_wifi_cfg_get_password());
        return 1u;
    }

    {
        char rx_tail[96];
        cloud_uart2_copy_rx_win(rx_tail, sizeof(rx_tail));
        REMEMBER_DBG("poll: connect FAIL ssid=%s manual=%u batch=%u conn_r=%u rx=%s",
                     app_wifi_cfg_get_ssid(),
                     (unsigned)s_manual_lock,
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
                     (unsigned)s_batch.count,
#else
                     0u,
#endif
                     (unsigned)conn_r,
                     rx_tail);
    }

    if(s_manual_lock != 0u) {
        if(cloud_aliyun_at_user_wifi_join_active() != 0u ||
           cloud_aliyun_at_wifi_bringup_active() != 0u) {
            return 0u;
        }
        int8_t idx = find_slot_by_ssid(s_manual_ssid);
        if(idx >= 0) {
            s_failed_slot_mask |= (uint8_t)(1u << (uint8_t)idx);
        }
        s_manual_lock = 0u;
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
        auto_batch_try_resume();
#endif
        return 0u;
    }

#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
    auto_batch_on_connect_fail();
#endif
    return 0u;
}
