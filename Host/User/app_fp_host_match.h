#ifndef APP_FP_HOST_MATCH_H
#define APP_FP_HOST_MATCH_H

#include <stdint.h>
#include "app_config.h"
#include "app_rs485_fp_sync.h"

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER

/* 处理从机发来的特征分片；最后一包时填充 rsp_out，返回 1 */
uint8_t app_fp_host_match_char_chunk(const rs485_fp_char_chunk_t *chunk, rs485_fp_match_rsp_t *rsp_out);

#endif

#endif /* APP_FP_HOST_MATCH_H */
