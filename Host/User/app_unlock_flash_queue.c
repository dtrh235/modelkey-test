#include "app_unlock_flash_queue.h"

#include <stdio.h>
#include <string.h>

#include "./BSP/W25Q16/bsp_w25q16.h"
#include "app_config.h"
#include "app_wall_clock.h"
#include "cloud_aliyun_at.h"
#include "app_wifi_diag.h"
#include "app_ccm_ram.h"

#define UNLOCK_FLASH_ADDR       ((uint32_t)0x00010000u)
#define UNLOCK_FLASH_MAGIC      (0x314B4C55u) /* ULK1 */
#define UNLOCK_FLASH_VERSION    (1u)
#define UNLOCK_FLASH_MAX_RECS   (96u)
#define UNLOCK_FLASH_FULL_THRESH (UNLOCK_FLASH_MAX_RECS)

typedef struct {
    char account[13];
    uint8_t method_code;
    uint8_t device_code;
    uint16_t reserved;
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

static APP_CCM_DATA unlock_flash_blob_t s_blob;
static uint8_t s_loaded = 0u;

static void blob_reset_empty(void)
{
    memset(&s_blob, 0, sizeof(s_blob));
    s_blob.magic = UNLOCK_FLASH_MAGIC;
    s_blob.version = UNLOCK_FLASH_VERSION;
    s_blob.next_seq = 1u;
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
    if(!bsp_w25q16_init()) {
        return 0u;
    }
    if(!bsp_w25q16_erase_sector_4k(UNLOCK_FLASH_ADDR)) {
        bsp_w25q16_end_session();
        return 0u;
    }
    {
        const uint8_t *p = (const uint8_t *)&s_blob;
        uint32_t off = 0u;
        while(off < sizeof(s_blob)) {
            uint16_t chunk = (uint16_t)((sizeof(s_blob) - off) > 256u ? 256u : (sizeof(s_blob) - off));
            if(!bsp_w25q16_write_page(UNLOCK_FLASH_ADDR + off, p + off, chunk)) {
                bsp_w25q16_end_session();
                return 0u;
            }
            off += chunk;
        }
    }
    bsp_w25q16_end_session();
    return 1u;
}

static void flash_load(void)
{
    unlock_flash_blob_t tmp;

    if(s_loaded != 0u) {
        return;
    }
    s_loaded = 1u;
    blob_reset_empty();
    if(!bsp_w25q16_init()) {
        return;
    }
    if(!bsp_w25q16_read(UNLOCK_FLASH_ADDR, (uint8_t *)&tmp, sizeof(tmp))) {
        bsp_w25q16_end_session();
        return;
    }
    bsp_w25q16_end_session();
    if(blob_valid(&tmp) == 0u) {
        return;
    }
    s_blob = tmp;
}

void app_unlock_flash_init(void)
{
    flash_load();
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

void app_unlock_flash_append(const char *account, uint8_t method_code, uint8_t device_code,
                             uint32_t unlock_time_sec, uint32_t uptime_sec, uint32_t seq)
{
    unlock_flash_rec_t *slot;

    flash_load();
    if(s_blob.magic != UNLOCK_FLASH_MAGIC) {
        blob_reset_empty();
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
    if(account != NULL) {
        strncpy(slot->account, account, sizeof(slot->account) - 1u);
    }
    slot->method_code = method_code;
    slot->device_code = device_code;
    slot->unlock_time_sec = unlock_time_sec;
    slot->uptime_sec = uptime_sec;
    slot->seq = (seq != 0u) ? seq : s_blob.next_seq++;
    if(seq == 0u) {
        (void)0;
    } else if(seq >= s_blob.next_seq) {
        s_blob.next_seq = seq + 1u;
    }
    s_blob.count++;

    if(flash_persist() == 0u) {
        WIFI_DBG("unlock flash save fail cnt=%u", (unsigned)s_blob.count);
    }
}

uint8_t app_unlock_flash_upload_next(int (*publish_fn)(const char *json, void *ctx), void *ctx)
{
    unlock_flash_rec_t rec;
    char time_str[24];
    char payload[384];
    int rc;

    flash_load();
    if(s_blob.count == 0u || publish_fn == NULL) {
        return 0u;
    }
    rec = s_blob.recs[0];
    if(cloud_aliyun_at_time_is_synced() == 0u || app_wall_clock_valid() == 0u) {
        return 0u;
    }
    if(app_wall_clock_format_unlock_event(time_str, sizeof(time_str),
                                          rec.unlock_time_sec, rec.uptime_sec) == 0u) {
        return 0u;
    }
    (void)snprintf(payload, sizeof(payload),
                   "{\"method\":\"thing.event.property.post\",\"id\":%lu,"
                   "\"params\":{\"unlock_account\":\"%s\",\"unlock_time\":\"%s\","
                   "\"unlock_method\":%u,\"unlock_device\":%u},\"version\":\"1.0\"}",
                   (unsigned long)rec.seq, rec.account, time_str,
                   (unsigned)rec.method_code, (unsigned)rec.device_code);
    rc = publish_fn(payload, ctx);
    if(rc == 0) {
        return 0u;
    }
    if(s_blob.count > 1u) {
        memmove(&s_blob.recs[0], &s_blob.recs[1],
                sizeof(s_blob.recs[0]) * (size_t)(s_blob.count - 1u));
    }
    s_blob.count--;
    (void)flash_persist();
    return 1u;
}
