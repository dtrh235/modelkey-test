#include "app_unlock_flash_queue.h"

#include <stdio.h>
#include <string.h>

#include "./BSP/W25Q16/bsp_w25q16.h"
#include "app_config.h"
#include "app_wall_clock.h"
#include "cloud_aliyun_at.h"
#include "app_wifi_diag.h"

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

#define UNLOCK_FLASH_ADDR       ((uint32_t)0x00010000u)
#define UNLOCK_FLASH_MAGIC      (0x314B4C55u) /* ULK1 */
#define UNLOCK_FLASH_VERSION    (1u)
#define UNLOCK_FLASH_MAX_RECS   (96u)
#define UNLOCK_FLASH_FULL_THRESH (UNLOCK_FLASH_MAX_RECS)

typedef struct {
    char account[24];
    uint8_t method_code;
    uint8_t device_code;
    uint8_t bridge_done;
    uint8_t reserved;
    uint32_t unlock_time_sec;
    uint32_t uptime_sec;
    uint32_t seq;
} unlock_flash_rec_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint16_t count;
    uint16_t reserved;
    uint32_t next_seq;
    uint32_t tail;
    unlock_flash_rec_t recs[UNLOCK_FLASH_MAX_RECS];
} unlock_flash_blob_t;

static unlock_flash_blob_t s_blob;
static uint8_t s_loaded = 0u;
static uint8_t s_flash_dirty = 0u;
#if (APP_USE_FREERTOS == 1)
static TaskHandle_t s_storage_task = NULL;
#endif

static void unlock_sanitize_account(const char *src, char *dst, size_t dst_sz)
{
    size_t i = 0u;

    if(dst_sz == 0u) {
        return;
    }
    if(src == NULL) {
        dst[0] = '\0';
        return;
    }
    while(src[i] != '\0' && i < (dst_sz - 1u)) {
        unsigned char c = (unsigned char)src[i];
        if(c == '"' || c == '\\') {
            dst[i] = '_';
        } else if(c < 0x20u || c > 0x7Eu) {
            dst[i] = '?';
        } else {
            dst[i] = (char)c;
        }
        i++;
    }
    dst[i] = '\0';
}

static uint8_t unlock_rec_valid(const unlock_flash_rec_t *rec)
{
    if(rec == NULL) {
        return 0u;
    }
    if(rec->method_code < 1u || rec->method_code > 5u) {
        return 0u;
    }
    if(rec->device_code < 1u || rec->device_code > 3u) {
        return 0u;
    }
    if(rec->account[0] == '\0') {
        return 0u;
    }
    /* 仅保留已同步时间后入队的记录（epoch 合法） */
    if(rec->unlock_time_sec < 1600000000u) {
        return 0u;
    }
    return 1u;
}

static void unlock_flash_drop_index(int16_t idx);
static void blob_purge_invalid(void);

static void flash_mark_dirty(void)
{
    s_flash_dirty = 1u;
#if (APP_USE_FREERTOS == 1)
    if(s_storage_task != NULL) {
        (void)xTaskNotifyGive(s_storage_task);
    }
#endif
}

static uint8_t flash_persist(void);

void app_unlock_flash_bind_storage_task(void *task_handle)
{
#if (APP_USE_FREERTOS == 1)
    s_storage_task = (TaskHandle_t)task_handle;
#else
    (void)task_handle;
#endif
}

uint8_t app_unlock_flash_flush_if_dirty(void)
{
    if(s_flash_dirty == 0u) {
        return 1u;
    }
    if(flash_persist() != 0u) {
        s_flash_dirty = 0u;
        return 1u;
    }
    s_flash_dirty = 1u;
    return 0u;
}

static uint8_t unlock_flash_has_seq(uint32_t seq)
{
    uint16_t i;

    if(seq == 0u) {
        return 0u;
    }
    for(i = 0u; i < s_blob.count; i++) {
        if(s_blob.recs[i].seq == seq) {
            return 1u;
        }
    }
    return 0u;
}

static void blob_reset_empty(void)
{
    memset(&s_blob, 0, sizeof(s_blob));
    s_blob.magic = UNLOCK_FLASH_MAGIC;
    s_blob.version = UNLOCK_FLASH_VERSION;
    s_blob.next_seq = 1u;
}

static void blob_compact_dedupe(void)
{
    uint16_t i;
    uint16_t w = 0u;
    uint8_t changed = 0u;

    for(i = 0u; i < s_blob.count; i++) {
        uint16_t j;
        uint8_t dup = 0u;

        if(unlock_rec_valid(&s_blob.recs[i]) == 0u) {
            changed = 1u;
            continue;
        }
        for(j = 0u; j < w; j++) {
            if(s_blob.recs[j].seq == s_blob.recs[i].seq) {
                dup = 1u;
                break;
            }
        }
        if(dup != 0u) {
            changed = 1u;
            continue;
        }
        if(w != i) {
            s_blob.recs[w] = s_blob.recs[i];
            changed = 1u;
        }
        w++;
    }
    if(w != s_blob.count) {
        changed = 1u;
        s_blob.count = w;
    }
    if(changed != 0u) {
        flash_mark_dirty();
        WIFI_DBG("unlock flash compact cnt=%u", (unsigned)s_blob.count);
    }
}

static uint8_t blob_valid(const unlock_flash_blob_t *b)
{
    if(b == NULL) return 0u;
    if(b->magic != UNLOCK_FLASH_MAGIC) return 0u;
    if(b->version != UNLOCK_FLASH_VERSION) return 0u;
    if(b->count > UNLOCK_FLASH_MAX_RECS) return 0u;
    return 1u;
}

static uint8_t flash_persist(void)
{
    uint8_t ok = 0u;

    bsp_w25q16_lock();
    if(!bsp_w25q16_init()) {
        bsp_w25q16_unlock();
        return 0u;
    }
    if(!bsp_w25q16_erase_sector_4k(UNLOCK_FLASH_ADDR)) {
        bsp_w25q16_end_session();
        bsp_w25q16_unlock();
        return 0u;
    }
    {
        const uint8_t *p = (const uint8_t *)&s_blob;
        uint32_t off = 0u;
        ok = 1u;
        while(off < sizeof(s_blob)) {
            uint16_t chunk = (uint16_t)((sizeof(s_blob) - off) > 256u ? 256u : (sizeof(s_blob) - off));
            if(!bsp_w25q16_write_page(UNLOCK_FLASH_ADDR + off, p + off, chunk)) {
                ok = 0u;
                break;
            }
            off += chunk;
        }
    }
    bsp_w25q16_end_session();
    bsp_w25q16_unlock();
    return ok;
}

static void flash_load(void)
{
    unlock_flash_blob_t tmp;

    if(s_loaded != 0u) {
        return;
    }
    s_loaded = 1u;
    blob_reset_empty();
    bsp_w25q16_lock();
    if(!bsp_w25q16_init()) {
        bsp_w25q16_unlock();
        return;
    }
    if(!bsp_w25q16_read(UNLOCK_FLASH_ADDR, (uint8_t *)&tmp, sizeof(tmp))) {
        bsp_w25q16_end_session();
        bsp_w25q16_unlock();
        return;
    }
    bsp_w25q16_end_session();
    bsp_w25q16_unlock();
    if(blob_valid(&tmp) == 0u) {
        return;
    }
    s_blob = tmp;
    blob_compact_dedupe();
}

void app_unlock_flash_init(void)
{
    flash_load();
    blob_compact_dedupe();
    blob_purge_invalid();
    if(s_blob.count > 0u) {
        WIFI_DBG("unlock flash boot pending=%u", (unsigned)s_blob.count);
    }
}

uint16_t app_unlock_flash_count(void)
{
    flash_load();
    return s_blob.count;
}

uint8_t app_unlock_flash_is_full(void)
{
    flash_load();
    return (s_blob.count >= UNLOCK_FLASH_FULL_THRESH) ? 1u : 0u;
}

uint8_t app_unlock_flash_is_nearly_full(void)
{
    flash_load();
    return (s_blob.count >= UNLOCK_FLASH_FULL_THRESH) ? 1u : 0u;
}

static void unlock_flash_append_internal(const char *account, uint8_t method_code,
                                         uint8_t device_code, uint32_t unlock_time_sec,
                                         uint32_t uptime_sec, uint32_t seq, uint8_t bridge_done)
{
    unlock_flash_rec_t *slot;
    char safe_acc[24];

    flash_load();
    if(s_blob.magic != UNLOCK_FLASH_MAGIC) {
        blob_reset_empty();
    }
    if(seq != 0u && unlock_flash_has_seq(seq) != 0u) {
        return;
    }
    if(unlock_time_sec < 1600000000u) {
        return;
    }

    if(s_blob.count >= UNLOCK_FLASH_MAX_RECS) {
        if(s_blob.count > 1u) {
            memmove(&s_blob.recs[0], &s_blob.recs[1],
                    sizeof(s_blob.recs[0]) * (size_t)(s_blob.count - 1u));
            s_blob.count--;
        } else {
            s_blob.count = 0u;
        }
    }

    slot = &s_blob.recs[s_blob.count];
    memset(slot, 0, sizeof(*slot));
    unlock_sanitize_account(account, safe_acc, sizeof(safe_acc));
    (void)strncpy(slot->account, safe_acc, sizeof(slot->account) - 1u);
    slot->method_code = method_code;
    slot->device_code = device_code;
    slot->bridge_done = bridge_done;
    slot->unlock_time_sec = unlock_time_sec;
    slot->uptime_sec = uptime_sec;
    slot->seq = (seq != 0u) ? seq : s_blob.next_seq++;
    if(seq == 0u) {
        (void)0;
    } else if(seq >= s_blob.next_seq) {
        s_blob.next_seq = seq + 1u;
    }
    s_blob.count++;

    blob_compact_dedupe();
    flash_mark_dirty();
    WIFI_DBG("unlock flash append seq=%lu cnt=%u",
             (unsigned long)slot->seq, (unsigned)s_blob.count);
}

void app_unlock_flash_append(const char *account, uint8_t method_code, uint8_t device_code,
                             uint32_t unlock_time_sec, uint32_t uptime_sec, uint32_t seq)
{
    unlock_flash_append_internal(account, method_code, device_code, unlock_time_sec, uptime_sec,
                                 seq, 0u);
}

void app_unlock_flash_append_bridge_done(const char *account, uint8_t method_code,
                                         uint8_t device_code, uint32_t unlock_time_sec,
                                         uint32_t uptime_sec, uint32_t seq)
{
    unlock_flash_append_internal(account, method_code, device_code, unlock_time_sec, uptime_sec,
                                 seq, 1u);
}

static int16_t unlock_flash_find_index(uint8_t bridge_done_only)
{
    uint16_t i;

    for(i = 0u; i < s_blob.count; i++) {
        if(s_blob.recs[i].bridge_done == bridge_done_only &&
           unlock_rec_valid(&s_blob.recs[i]) != 0u) {
            return (int16_t)i;
        }
    }
    return -1;
}

static void unlock_flash_drop_index(int16_t idx)
{
    if(idx < 0) {
        return;
    }
    if(s_blob.count > (uint16_t)(idx + 1)) {
        memmove(&s_blob.recs[(uint16_t)idx], &s_blob.recs[(uint16_t)(idx + 1)],
                sizeof(s_blob.recs[0]) * (size_t)(s_blob.count - (uint16_t)(idx + 1u)));
    }
    s_blob.count--;
}

static void blob_purge_invalid(void)
{
    uint16_t i;
    uint8_t changed = 0u;

    for(i = 0u; i < s_blob.count; ) {
        if(unlock_rec_valid(&s_blob.recs[i]) == 0u) {
            unlock_flash_drop_index((int16_t)i);
            changed = 1u;
        } else {
            i++;
        }
    }
    if(changed != 0u) {
        flash_mark_dirty();
    }
}

static void unlock_flash_drop_all_seq(uint32_t seq)
{
    int16_t i;

    if(seq == 0u) {
        return;
    }
    for(i = (int16_t)((int16_t)s_blob.count - 1); i >= 0; i--) {
        if(s_blob.recs[(uint16_t)i].seq == seq) {
            unlock_flash_drop_index(i);
        }
    }
}

void app_unlock_flash_drop_seq(uint32_t seq)
{
    if(seq == 0u) {
        return;
    }
    flash_load();
    if(unlock_flash_has_seq(seq) == 0u) {
        return;
    }
    unlock_flash_drop_all_seq(seq);
    flash_mark_dirty();
}

static uint8_t unlock_flash_upload_one(int (*publish_fn)(const char *json, void *ctx), void *ctx,
                                       uint8_t bridge_done_only)
{
    unlock_flash_rec_t rec;
    char payload[384];
    char time_txt[24];
    char safe_acc[24];
    int16_t idx;
    int rc;

    flash_load();
    if(s_blob.count == 0u || publish_fn == NULL) {
        return 0u;
    }
    idx = unlock_flash_find_index(bridge_done_only);
    if(idx < 0) {
        /* 丢弃损坏记录，避免反复上传乱码 */
        for(idx = (int16_t)((int16_t)s_blob.count - 1); idx >= 0; idx--) {
            if(s_blob.recs[(uint16_t)idx].bridge_done == bridge_done_only &&
               unlock_rec_valid(&s_blob.recs[(uint16_t)idx]) == 0u) {
                unlock_flash_drop_index(idx);
                flash_mark_dirty();
            }
        }
        return 0u;
    }
    rec = s_blob.recs[(uint16_t)idx];
    if(cloud_aliyun_at_time_is_synced() == 0u || app_wall_clock_valid() == 0u) {
        return 0u;
    }
    if(app_wall_clock_epoch_sec() < 1600000000u) {
        return 0u;
    }
    if(app_wall_clock_format_unlock_event(time_txt, sizeof(time_txt),
                                          rec.unlock_time_sec, rec.uptime_sec) == 0u) {
        return 0u;
    }
    unlock_sanitize_account(rec.account, safe_acc, sizeof(safe_acc));
    (void)snprintf(payload, sizeof(payload),
                   "{\"method\":\"thing.event.property.post\",\"id\":%lu,"
                   "\"params\":{\"unlock_account\":\"%s\",\"unlock_time\":\"%s\","
                   "\"unlock_method\":%u,\"unlock_device\":%u},\"version\":\"1.0\"}",
                   (unsigned long)rec.seq, safe_acc, time_txt,
                   (unsigned)rec.method_code, (unsigned)rec.device_code);
    rc = publish_fn(payload, ctx);
    if(rc == 0) {
        WIFI_DBG("unlock flash pub fail seq=%lu", (unsigned long)rec.seq);
        return 0u;
    }
    unlock_flash_drop_all_seq(rec.seq);
    WIFI_DBG("unlock flash pub ok seq=%lu left=%u",
             (unsigned long)rec.seq, (unsigned)s_blob.count);
    flash_mark_dirty();
    return 1u;
}

uint8_t app_unlock_flash_upload_next(int (*publish_fn)(const char *json, void *ctx), void *ctx)
{
    return unlock_flash_upload_one(publish_fn, ctx, 0u);
}

uint8_t app_unlock_flash_upload_next_property(int (*publish_fn)(const char *json, void *ctx),
                                              void *ctx)
{
    return unlock_flash_upload_one(publish_fn, ctx, 1u);
}
