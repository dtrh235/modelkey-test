#include "ota_control_port.h"

#include "boot_ota_config.h"
#include "stm32f4xx_hal.h"

#include <string.h>

/*
 * OTA 固件升级已关闭：不向片内/片外 Flash 写入元数据或镜像，
 * 保留 API 供链接与将来扩展，行为均为安全空操作。
 */

uint32_t ota_port_get_flash_kb(void)
{
    uint16_t sz = *(__IO uint16_t *)(BOOT_FLASH_SIZE_REG_ADDR);
    if(sz == 0u || sz == 0xFFFFu) {
        return BOOT_EXPECT_FLASH_KB;
    }
    return (uint32_t)sz;
}

bool ota_port_flash_layout_supported(void)
{
    return true;
}

bool ota_meta_write(const ota_meta_t *meta)
{
    (void)meta;
    return false;
}

bool ota_meta_read(ota_meta_t *out)
{
    if(out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    return false;
}

bool ota_meta_clear(void)
{
    return true;
}

bool ota_stage_erase(uint32_t image_size)
{
    (void)image_size;
    return false;
}

bool ota_stage_write(uint32_t offset, const uint8_t *data, uint32_t len)
{
    (void)offset;
    (void)data;
    (void)len;
    return false;
}

bool ota_stage_read(uint32_t offset, uint8_t *data, uint32_t len)
{
    (void)offset;
    if(data != NULL && len > 0u) {
        memset(data, 0xFF, len);
    }
    return false;
}

uint32_t ota_crc32_init(void)
{
    return 0xFFFFFFFFu;
}

uint32_t ota_crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t k;

    if(data == NULL) {
        return crc;
    }
    for(i = 0u; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for(k = 0u; k < 8u; k++) {
            if((crc & 1u) != 0u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

uint32_t ota_crc32_finish(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}
