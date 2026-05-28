#ifndef APP_FP_MIRROR_TX_H
#define APP_FP_MIRROR_TX_H

#include <stdint.h>
#include "app_config.h"

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER

void app_fp_mirror_tx_schedule_acc(const char *acc);
void app_fp_mirror_tx_schedule_page(uint16_t page_id);
void app_fp_mirror_tx_schedule_page_force(uint16_t page_id);
void app_fp_mirror_tx_prime_page(uint16_t page_id, const uint8_t *tpl);
uint8_t app_fp_mirror_tx_busy(void);
/* 首页开锁时暂停镜像，避免与 AS608 抢串口 */
void app_fp_mirror_tx_hold_for_unlock(uint8_t hold);
void app_fp_mirror_tx_tick(void);

#endif

#endif /* APP_FP_MIRROR_TX_H */
