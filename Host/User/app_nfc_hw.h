#ifndef APP_NFC_HW_H
#define APP_NFC_HW_H

#include <stdint.h>

void app_nfc_hw_wakeup_no_reset(void);
void app_nfc_hw_init_once(void);
/* 用 ms 级 RST 拉低/拉高的强力硬件复位 + soft reset + RxGain/ASK 全部重写一遍。
 * 当首页发现 MFRC522 进入"假死"（tx_ctrl 读出来不是 0x83、或连续 MI_ERR）时使用。 */
void app_nfc_hw_deep_recover(void);
uint8_t app_nfc_hw_ready(void);
uint8_t app_nfc_last_reset_ret(void);

#endif
