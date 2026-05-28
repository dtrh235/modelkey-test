#ifndef BOOT_OTA_CONFIG_H
#define BOOT_OTA_CONFIG_H

#include <stdint.h>

/*
 * 片内 Flash 布局（与可选 BootLoader 预留区配合，供 APP_BOOT_VTOR_RELOCATE 使用）。
 * 外部 W25Q16 用户区地址由 bsp_w25q16 / app_user_ops 自行约定，不再在此定义 OTA 暂存区。
 */
#define BOOT_FLASH_BASE_ADDR      ((uint32_t)0x08000000u)
#define BOOT_FLASH_SIZE_REG_ADDR  ((uint32_t)0x1FFF7A22u)
#define BOOT_EXPECT_FLASH_KB      ((uint32_t)1024u)

#define BOOT_RESERVED_SIZE        ((uint32_t)0x00004000u)
#define BOOT_APP_VTOR_ADDR        (BOOT_FLASH_BASE_ADDR + BOOT_RESERVED_SIZE)
#define BOOT_APP_MAX_SIZE         ((uint32_t)0x00100000u - BOOT_RESERVED_SIZE)

#endif
