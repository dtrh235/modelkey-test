#ifndef APP_RS485_PROTO_H
#define APP_RS485_PROTO_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "user_auth_portable.h"
#include "app_rs485_fp_sync.h"

#if (APP_RS485_ENABLE == 1)

/*
 * 与从机约定（从机需实现相同帧格式）：
 *   帧: A5 5A | dst | src | seq | cmd | len_lo len_hi | payload[len] | crc_lo crc_hi
 *   CRC16-MODBUS: 对 dst..payload 末字节多项式 0xA001, 初值 0xFFFF
 * 命令字（主机→从机）:
 *   0x01 PING        payload 空
 *   0x02 USER_ADD    payload = user_cred_t 原始二进制
 *   0x03 USER_DELETE payload = 账号 13 字节（与 cred.acc 相同布局，0 填充）
 *   0x05 FP_TEMPLATE  payload = rs485_fp_tpl_chunk_t（128B 分片 ×4 = 512B 模板）
 * 从机→主机:
 *   0x04 MIRROR_SYNC_REQ payload 空 — 请主机尽快下发全量用户（账号/密码/NFC/指纹）
 *   0x10 SLAVE_UNLOCK_NOTIFY payload 15 字节 rs485_unlock_notify_t:
 *        method_id(1=pwd,2=nfc,3=fp) + device_id(1=主机,2=从机) + acc[12]
 * 应答 cmd = 请求 | 0x80, len=0 表示成功；len=1 payload[0]=错误码 表示失败
 */

bool app_rs485_proto_ping_peer(uint32_t tout_ms);
bool app_rs485_proto_slave_user_add(const user_cred_t *u, uint32_t tout_ms);
bool app_rs485_proto_slave_user_delete(const char *acc, uint32_t tout_ms);
bool app_rs485_proto_slave_fp_template_chunk(const rs485_fp_tpl_chunk_t *chunk, uint32_t tout_ms);
bool app_rs485_proto_slave_unlock_notify(const char *acc, uint8_t method_id, uint32_t tout_ms);

void app_rs485_master_poll_incoming(uint32_t read_tout_ms);
uint8_t app_rs485_master_cmd_in_progress(void);
void app_rs485_master_mirror_if_requested(void);
void app_rs485_master_clear_fp_commit(void);
uint8_t app_rs485_master_take_fp_commit(uint16_t *page_out, uint8_t *status_out);
void app_rs485_slave_server_poll(uint32_t read_tout_ms);

#endif

#endif /* APP_RS485_PROTO_H */
