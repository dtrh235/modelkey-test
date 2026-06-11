#ifndef APP_UNLOCK_FLASH_QUEUE_H
#define APP_UNLOCK_FLASH_QUEUE_H

#include <stdint.h>

void app_unlock_flash_init(void);
uint16_t app_unlock_flash_count(void);
uint8_t app_unlock_flash_is_full(void);
uint8_t app_unlock_flash_is_nearly_full(void);

void app_unlock_flash_append(const char *account, uint8_t method_code, uint8_t device_code,
                             uint32_t unlock_time_sec, uint32_t uptime_sec, uint32_t seq);

/** bridge_done=1：桥接 unlock_record 已发过，仅补发物模型 */
void app_unlock_flash_append_bridge_done(const char *account, uint8_t method_code,
                                         uint8_t device_code, uint32_t unlock_time_sec,
                                         uint32_t uptime_sec, uint32_t seq);

/** 物模型 post 确认后删除对应 seq（防 Flash 重复补发） */
void app_unlock_flash_drop_seq(uint32_t seq);

/* 1=已上传可删 0=无记录/失败 */
uint8_t app_unlock_flash_upload_next(int (*publish_fn)(const char *json, void *ctx), void *ctx);
uint8_t app_unlock_flash_upload_next_property(int (*publish_fn)(const char *json, void *ctx),
                                              void *ctx);

#endif
