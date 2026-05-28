#ifndef APP_SLAVE_FP_SYNC_H
#define APP_SLAVE_FP_SYNC_H

#include <stdint.h>
#include "app_config.h"
#include "app_rs485_fp_sync.h"

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE

uint8_t app_slave_fp_template_chunk_rx(const rs485_fp_tpl_chunk_t *chunk);
uint8_t app_slave_fp_template_apply_now(uint8_t *fp_hw_inited);
void app_slave_fp_template_apply_pending(uint8_t *fp_hw_inited);
uint8_t app_slave_fp_queue_pending(void);
uint8_t app_slave_fp_quiet_active(void);
uint8_t app_slave_fp_write_ever_ok(void);
uint8_t app_slave_fp_write_busy(void);
uint8_t app_slave_fp_mirror_hold(void);

#endif

#endif /* APP_SLAVE_FP_SYNC_H */
