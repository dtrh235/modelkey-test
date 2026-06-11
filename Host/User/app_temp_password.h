#ifndef APP_TEMP_PASSWORD_H
#define APP_TEMP_PASSWORD_H

#include <stdint.h>

#define APP_TEMP_PASSWORD_MAX  8u
#define APP_TEMP_ACCOUNT_MAX   12u
#define APP_TEMP_PWD_MAX       10u

void app_temp_password_init(void);
void app_temp_password_tick(void);

/** valid_sec：从收到指令起有效秒数（用 HAL_GetTick，不依赖墙钟） */
uint8_t app_temp_password_add(const char *account, const char *password, uint32_t valid_sec);
uint8_t app_temp_password_revoke(const char *account);

/** 匹配成功则标记已用并返回 1 */
uint8_t app_temp_password_try_unlock(const char *account, const char *password);

#endif
