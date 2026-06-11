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
 *   0x05 FP_TEMPLATE  payload = rs485_fp_tpl_chunk_t
 * 从机→主机:
 *   0x04 MIRROR_SYNC_REQ payload 空 — 请求主机下发全量用户（账号/密码/NFC/指纹）
 *   0x10 UNLOCK_NOTIFY payload 15 字节: method_id + device_id(1=主,2=从) + acc[12] 填充末字节
 * 应答 cmd = 请求 | 0x80, len=0 表示成功；len=1 payload[0]=错误码 表示失败
 */

bool app_rs485_proto_ping_peer(uint32_t tout_ms);
bool app_rs485_proto_slave_user_add(const user_cred_t *u, uint32_t tout_ms);
bool app_rs485_proto_slave_user_delete(const char *acc, uint32_t tout_ms);
bool app_rs485_proto_slave_fp_template_chunk(const rs485_fp_tpl_chunk_t *chunk, uint32_t tout_ms);
bool app_rs485_proto_slave_unlock_notify(const char *acc, uint8_t method_id, uint32_t tout_ms);

#if APP_RS485_IS_SLAVE
/* 从机开锁后异步上报主机，不阻塞 NfcTask（原同步 notify 可卡 280ms）。 */
void app_rs485_slave_unlock_notify_async(const char *acc, uint8_t method_id);
void app_rs485_slave_lockout_alert_async(const char *last_acc);
void app_rs485_slave_flush_pending_notify(void);
void app_rs485_slave_upkeep_mirror_request(void);
bool app_rs485_proto_slave_fp_template_commit(uint16_t page_id, uint8_t status, uint32_t tout_ms);
void app_rs485_slave_fp_commit_async(uint16_t page_id, uint8_t status);
bool app_rs485_slave_fp_match_request(const uint8_t *feat512, rs485_fp_match_rsp_t *rsp, uint32_t tout_ms);
#endif

void app_rs485_master_poll_incoming(uint32_t read_tout_ms);
void app_rs485_slave_server_poll(uint32_t read_tout_ms);

#endif /* APP_RS485_ENABLE */

#endif /* APP_RS485_PROTO_H */
