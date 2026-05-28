#ifndef APP_RS485_FP_SYNC_H
#define APP_RS485_FP_SYNC_H

#include <stdint.h>

#define RS485_CMD_FP_TEMPLATE     0x05u
#define RS485_CMD_FP_COMMIT_NOTIFY 0x11u
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

#define RS485_UNLOCK_NOTIFY_PLEN       15u
#define RS485_UNLOCK_DEVICE_MASTER     1u
#define RS485_UNLOCK_DEVICE_SLAVE      2u
typedef struct {
    uint8_t method_id;
    uint8_t device_id;
    char    acc[12];
} rs485_unlock_notify_t;
#pragma pack(pop)

#endif /* APP_RS485_FP_SYNC_H */
