#ifndef APP_HOME_FP_POLL_H
#define APP_HOME_FP_POLL_H

#include <stdint.h>

uint8_t app_fp_hw_try_handshake(uint8_t *fp_hw_inited);
uint8_t app_fp_hw_try_handshake_retries(uint8_t *fp_hw_inited, uint8_t attempts, uint16_t gap_ms);
uint8_t app_fp_chip_tpl_count(uint16_t *n_out, uint8_t *fp_hw_inited);
void app_home_fp_poll_handle(uint32_t *home_fp_last_poll_ms, uint8_t *fp_hw_inited);

#endif
