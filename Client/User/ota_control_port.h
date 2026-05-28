#ifndef OTA_CONTROL_PORT_H
#define OTA_CONTROL_PORT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t state;
    uint32_t error_code;
    uint32_t update_flag;
    uint32_t board_flash_kb;
    uint32_t app_addr;
    uint32_t app_max_size;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t downloaded_size;
    uint32_t apply_offset;
    char target_version[32];
    char module[32];
    char url[256];
} ota_meta_t;

enum {
    OTA_META_STATE_IDLE = 0u,
    OTA_META_STATE_DOWNLOADING = 1u,
    OTA_META_STATE_STAGED = 2u,
    OTA_META_STATE_APPLYING = 3u,
    OTA_META_STATE_DONE = 4u,
    OTA_META_STATE_FAILED = 0xE0u
};

uint32_t ota_port_get_flash_kb(void);
bool ota_port_flash_layout_supported(void);

bool ota_meta_write(const ota_meta_t *meta);
bool ota_meta_read(ota_meta_t *out);
bool ota_meta_clear(void);

bool ota_stage_erase(uint32_t image_size);
bool ota_stage_write(uint32_t offset, const uint8_t *data, uint32_t len);
bool ota_stage_read(uint32_t offset, uint8_t *data, uint32_t len);

uint32_t ota_crc32_init(void);
uint32_t ota_crc32_update(uint32_t crc, const uint8_t *data, uint32_t len);
uint32_t ota_crc32_finish(uint32_t crc);

#endif
