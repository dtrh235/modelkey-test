#include "app_pair_bind.h"

#include "app_cloud_command.h"
#include "app_config.h"
#include "cloud_aliyun_at.h"

#include <stdio.h>
#include <string.h>

#include "../Drivers/BSP/W25Q16/bsp_w25q16.h"
#include "stm32f4xx_hal.h"

#define PAIR_CODE_TTL_MS        (10u * 60u * 1000u)
#define PAIR_BIND_FLASH_ADDR    ((uint32_t)0x00013000u)
#define PAIR_BIND_FLASH_MAGIC   (0x5042494Eu) /* PBIN */
#define PAIR_BIND_FLASH_VERSION (1u)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t bound;
    uint8_t reserved[3];
    char app_account[13];
    uint32_t tail;
} pair_bind_flash_t;

volatile uint8_t g_pair_ui_dirty = 0u;

static char s_pair_code[APP_PAIR_CODE_LEN + 1u];
static char s_bound_account[13];
static uint32_t s_pair_expire_ms;
static uint8_t s_pair_bound;
static uint8_t s_pair_flash_inited;

static uint32_t pair_rand_u32(void)
{
    static uint32_t seed = 0u;

    if(seed == 0u) {
        seed = HAL_GetTick() ^ 0xA5C3E7B1u;
    }
    seed = seed * 1664525u + 1013904223u;
    return seed;
}

static void pair_make_code(void)
{
    uint32_t n;

    n = pair_rand_u32() % 1000000u;
    (void)snprintf(s_pair_code, sizeof(s_pair_code), "%06lu", (unsigned long)n);
    s_pair_expire_ms = HAL_GetTick() + PAIR_CODE_TTL_MS;
}

static uint8_t pair_flash_blob_valid(const pair_bind_flash_t *blob)
{
    if(blob == NULL) {
        return 0u;
    }
    if(blob->magic != PAIR_BIND_FLASH_MAGIC ||
       blob->version != PAIR_BIND_FLASH_VERSION ||
       blob->tail != PAIR_BIND_FLASH_MAGIC) {
        return 0u;
    }
    if(blob->bound == 0u) {
        return 1u;
    }
    return (blob->app_account[0] != '\0') ? 1u : 0u;
}

static void pair_bind_flash_save(void)
{
    pair_bind_flash_t blob;
    const uint8_t *p = (const uint8_t *)&blob;
    uint32_t off;

    memset(&blob, 0, sizeof(blob));
    blob.magic = PAIR_BIND_FLASH_MAGIC;
    blob.version = PAIR_BIND_FLASH_VERSION;
    blob.bound = s_pair_bound;
    if(s_pair_bound != 0u && s_bound_account[0] != '\0') {
        (void)snprintf(blob.app_account, sizeof(blob.app_account), "%s", s_bound_account);
    }
    blob.tail = PAIR_BIND_FLASH_MAGIC;

    if(!bsp_w25q16_init()) {
        return;
    }
    if(!bsp_w25q16_erase_sector_4k(PAIR_BIND_FLASH_ADDR)) {
        bsp_w25q16_end_session();
        return;
    }
    for(off = 0u; off < sizeof(blob); off += 256u) {
        uint16_t chunk = (uint16_t)((sizeof(blob) - off) > 256u ? 256u : (sizeof(blob) - off));
        if(!bsp_w25q16_write_page(PAIR_BIND_FLASH_ADDR + off, p + off, chunk)) {
            bsp_w25q16_end_session();
            return;
        }
    }
    bsp_w25q16_end_session();
}

static void pair_bind_flash_load(void)
{
    pair_bind_flash_t blob;

    memset(&blob, 0, sizeof(blob));
    if(!bsp_w25q16_init()) {
        return;
    }
    if(!bsp_w25q16_read(PAIR_BIND_FLASH_ADDR, (uint8_t *)&blob, sizeof(blob))) {
        bsp_w25q16_end_session();
        return;
    }
    bsp_w25q16_end_session();
    if(pair_flash_blob_valid(&blob) == 0u) {
        return;
    }
    if(blob.bound != 0u) {
        s_pair_bound = 1u;
        (void)snprintf(s_bound_account, sizeof(s_bound_account), "%s", blob.app_account);
    } else {
        s_pair_bound = 0u;
        s_bound_account[0] = '\0';
    }
}

void app_pair_bind_init(void)
{
    if(s_pair_flash_inited != 0u) {
        return;
    }
    s_pair_flash_inited = 1u;
    pair_bind_flash_load();
}

void app_pair_mark_ui_dirty(void)
{
    g_pair_ui_dirty = 1u;
}

void app_pair_publish_lock_unbind(void)
{
    char topic[96];
    char json[96];

    if(cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    (void)snprintf(topic, sizeof(topic), "/%s/%s%s",
                   APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
                   APP_BRIDGE_TOPIC_TERMINAL_PUSH);
    (void)snprintf(json, sizeof(json),
                   "{\"cmd\":\"lock_unbind\",\"id\":\"%lu\"}",
                   (unsigned long)HAL_GetTick());
    (void)cloud_aliyun_at_mqtt_publish_qos0(topic, json);
}

void app_pair_regenerate_code(void)
{
    uint8_t was_bound = s_pair_bound;

    pair_make_code();
    s_pair_bound = 0u;
    s_bound_account[0] = '\0';
    if(was_bound != 0u) {
        app_pair_publish_lock_unbind();
    }
    pair_bind_flash_save();
    app_pair_mark_ui_dirty();
}

void app_pair_clear_bind(void)
{
    uint8_t was_bound = s_pair_bound;

    s_pair_bound = 0u;
    s_bound_account[0] = '\0';
    if(was_bound != 0u) {
        app_pair_publish_lock_unbind();
    }
    pair_bind_flash_save();
    app_pair_ensure_code();
    app_pair_mark_ui_dirty();
}

void app_pair_ensure_code(void)
{
    uint32_t now = HAL_GetTick();

    app_pair_bind_init();
    if(s_pair_code[0] == '\0' || now >= s_pair_expire_ms) {
        /* 仅刷新配对码，不清除已绑定账号（重启后仍需远程开锁） */
        pair_make_code();
    }
}

uint8_t app_pair_is_bound(void)
{
    app_pair_bind_init();
    return s_pair_bound;
}

uint8_t app_pair_remote_unlock_allowed(const char *app_account)
{
    const char *bound;

    if(app_pair_is_bound() == 0u) {
        return 0u;
    }
    if(app_account == NULL || app_account[0] == '\0') {
        return 0u;
    }
    bound = app_pair_get_bound_account();
    if(bound[0] == '\0') {
        return 1u;
    }
    return (strcmp(app_account, bound) == 0) ? 1u : 0u;
}

uint8_t app_pair_code_valid(void)
{
    uint32_t now = HAL_GetTick();

    if(s_pair_code[0] == '\0' || now >= s_pair_expire_ms) {
        return 0u;
    }
    return 1u;
}

uint8_t app_pair_cloud_ready(void)
{
    return cloud_aliyun_at_is_online();
}

const char *app_pair_get_code(void)
{
    return s_pair_code;
}

const char *app_pair_get_bound_account(void)
{
    return s_bound_account;
}

void app_pair_publish_bind_ack(uint8_t success, const char *req_id)
{
    char topic[96];
    char json[192];
    const char *acc;
    const char *id;

    if(cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    acc = (s_bound_account[0] != '\0') ? s_bound_account : "";
    id = (req_id != NULL && req_id[0] != '\0') ? req_id : "";
    (void)snprintf(topic, sizeof(topic), "/%s/%s%s",
                   APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
                   APP_BRIDGE_TOPIC_TERMINAL_PUSH);
    (void)snprintf(json, sizeof(json),
                   "{\"cmd\":\"bind_ack\",\"ok\":%u,\"appAccount\":\"%s\",\"id\":\"%s\"}",
                   success ? 1u : 0u,
                   acc,
                   id);
    (void)cloud_aliyun_at_mqtt_publish_qos0(topic, json);
}

void app_pair_publish_unlock_alert(uint8_t device_id, const char *last_account)
{
    char topic[96];
    char json[220];
    const char *acc;
    uint32_t id;

    if(cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    acc = (last_account != NULL && last_account[0] != '\0') ? last_account : "";
    id = HAL_GetTick();
    (void)snprintf(topic, sizeof(topic), "/%s/%s%s",
                   APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
                   APP_BRIDGE_TOPIC_TERMINAL_PUSH);
    (void)snprintf(json, sizeof(json),
                   "{\"cmd\":\"unlock_alert\",\"device\":%u,\"failCount\":5,"
                   "\"lastAccount\":\"%s\",\"id\":\"%lu\"}",
                   (unsigned)device_id,
                   acc,
                   (unsigned long)id);
    (void)cloud_aliyun_at_mqtt_publish_qos0(topic, json);
}

void app_pair_publish_unbind_ack(uint8_t success, const char *req_id)
{
    char topic[96];
    char json[160];
    const char *id;

    if(cloud_aliyun_at_is_online() == 0u) {
        return;
    }
    id = (req_id != NULL && req_id[0] != '\0') ? req_id : "";
    (void)snprintf(topic, sizeof(topic), "/%s/%s%s",
                   APP_ALIYUN_PRODUCT_KEY, APP_ALIYUN_DEVICE_NAME,
                   APP_BRIDGE_TOPIC_TERMINAL_PUSH);
    (void)snprintf(json, sizeof(json),
                   "{\"cmd\":\"unbind_ack\",\"ok\":%u,\"id\":\"%s\"}",
                   success ? 1u : 0u,
                   id);
    (void)cloud_aliyun_at_mqtt_publish_qos0(topic, json);
}

uint8_t app_pair_try_bind(const char *code, const char *app_account)
{
    if(app_pair_cloud_ready() == 0u) {
        return 0u;
    }
    if(code == NULL || strlen(code) != APP_PAIR_CODE_LEN) {
        return 0u;
    }
    app_pair_ensure_code();
    if(app_pair_code_valid() == 0u) {
        return 0u;
    }
    if(strcmp(code, s_pair_code) != 0) {
        return 0u;
    }

    s_pair_bound = 1u;
    s_bound_account[0] = '\0';
    if(app_account != NULL && app_account[0] != '\0') {
        (void)snprintf(s_bound_account, sizeof(s_bound_account), "%.12s", app_account);
    }
    pair_bind_flash_save();
    app_pair_mark_ui_dirty();
    return 1u;
}
