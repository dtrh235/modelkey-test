/**
 * Keil 编译 Host/User 下源文件时，会先找到 Host/app_config.h（RS485=0）。
 * 本文件通过预包含强制使用测试工程配置。
 */
#ifndef APP_CONFIG_H
#include "app_config.h"
#endif

#undef APP_RS485_ENABLE
#define APP_RS485_ENABLE 1

#include "door.h"
