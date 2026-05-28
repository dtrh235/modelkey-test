#ifndef APP_RS485_FP_SYNC_H
#define APP_RS485_FP_SYNC_H

#include <stdint.h>

/* 主机→从机：AS608 模板分片（512B = 4×128B） */
#define RS485_CMD_FP_TEMPLATE     0x05u
#define RS485_CMD_FP_COMMIT_NOTIFY 0x11u
/* 从机→主机：当前手指特征分片（512B = 4×128B），page_id 字段复用为 match_seq */
#define RS485_CMD_FP_MATCH_CHAR    0x12u
#define RS485_FP_MATCH_RESULT_OK       0u
#define RS485_FP_MATCH_RESULT_NO_MATCH 1u
#define RS485_FP_MATCH_RESULT_BUSY     2u
#define RS485_FP_MATCH_RESULT_ERR      0xFFu
#define RS485_FP_CHUNK_BYTES      128u
#define RS485_FP_CHUNK_COUNT      4u

#pragma pack(push, 1)
typedef struct {
    uint16_t page_id;
    uint8_t chunk_idx;
    uint8_t chunk_count;
    uint8_t data[RS485_FP_CHUNK_BYTES];
} rs485_fp_tpl_chunk_t;

typedef struct {
    uint16_t page_id;
    uint8_t status; /* 0=write OK, other=AS608 error code */
} rs485_fp_commit_t;

typedef rs485_fp_tpl_chunk_t rs485_fp_char_chunk_t;

typedef struct {
    uint8_t  result;
    uint8_t  reserved;
    uint16_t page_id;
    uint16_t score;
    char     acc[13];
} rs485_fp_match_rsp_t;

/* 从机→主机 0x10 开锁通知：15 字节（含主从身份）；旧固件 14 字节无 device 字段 */
#define RS485_UNLOCK_NOTIFY_PLEN       15u
#define RS485_UNLOCK_DEVICE_MASTER     1u
#define RS485_UNLOCK_DEVICE_SLAVE      2u
typedef struct {
    uint8_t method_id; /* 1=密码 2=NFC 3=指纹 */
    uint8_t device_id; /* RS485_UNLOCK_DEVICE_*，与阿里云 unlock_device 一致 */
    char    acc[12];
} rs485_unlock_notify_t;
#pragma pack(pop)

#endif /* APP_RS485_FP_SYNC_H */
