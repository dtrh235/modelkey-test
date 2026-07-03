#ifndef APP_HOME_UNLOCK_H
#define APP_HOME_UNLOCK_H

#include <stdint.h>

void app_home_nfc_poll_handle(void);
void app_home_nfc_reset_debounce(void);
/* 首页 NFC/指纹 200ms 交叉轮询（0=NFC，1=指纹），由 HomeAuthTask 调用 */
void app_home_auth_poll_tick(void);
void app_home_show_unlock_popup(void);
uint8_t app_home_unlock_popup_visible(void);
void app_home_unlock_set_poll_base(uint32_t now_ms);
void app_home_unlock_housekeeping(void);
uint8_t app_home_unlock_in_match_cooldown(void);
void app_home_unlock_arm_match_cooldown(void);

#endif
