#ifndef APP_HOME_UNLOCK_H
#define APP_HOME_UNLOCK_H

#include <stdint.h>

void app_home_nfc_poll_handle(void);
void app_home_show_unlock_popup(void);
uint8_t app_home_unlock_popup_visible(void);
void app_home_unlock_set_poll_base(uint32_t now_ms);
/* 进入开锁页后立刻允许下一轮 NFC 轮询（与主机进首页后马上能刷同理） */
void app_home_nfc_poll_arm_immediate(void);
#if APP_RS485_IS_SLAVE
void app_home_fp_poll_arm_immediate(void);
uint8_t app_home_unlock_in_match_cooldown(void);
void app_home_unlock_arm_match_cooldown(void);
#endif
void app_home_unlock_housekeeping(void);

#endif
