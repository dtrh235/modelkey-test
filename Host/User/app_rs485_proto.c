#include "app_rs485_proto.h"

#if (APP_RS485_ENABLE == 1)

#include <string.h>

#include "app_host_diag.h"
#include "app_rs485_link.h"

#if APP_RS485_IS_SLAVE
#include "app_user_ops.h"
#endif

#if APP_RS485_IS_MASTER
#include "cloud_ota_service.h"
#include "app_user_ops.h"
#include "app_fp_host_match.h"
#endif

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

#define RS485_SOF0 0xA5u
#define RS485_SOF1 0x5Au

#define RS485_HDR_FIXED 8u
#define RS485_CRC_LEN   2u

#define RS485_CMD_SLAVE_UNLOCK_NOTIFY 0x10u
#define RS485_CMD_MIRROR_SYNC_REQ     0x04u

#if APP_RS485_IS_MASTER
static volatile uint8_t s_mirror_sync_pending;
static volatile uint8_t s_fp_commit_pending;
static volatile uint16_t s_fp_commit_page;
static volatile uint8_t s_fp_commit_status;
static volatile uint8_t s_rs485_master_cmd_active;
static struct {
    uint8_t buf[RS485_HDR_FIXED + 32u + RS485_CRC_LEN];
    uint16_t len;
    uint8_t valid;
} s_rs485_pending_rsp;
#endif

static uint8_t s_tx_seq;

#if APP_RS485_IS_MASTER
static uint8_t rs485_master_try_enter_cmd(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if(s_rs485_master_cmd_active != 0u) {
        if(primask == 0u) {
            __enable_irq();
        }
        return 0u;
    }
    s_rs485_master_cmd_active = 1u;
    if(primask == 0u) {
        __enable_irq();
    }
    return 1u;
}

static void rs485_master_leave_cmd(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    s_rs485_master_cmd_active = 0u;
    if(primask == 0u) {
        __enable_irq();
    }
}
#endif

/* RS485 仅在 CloudTask 调用；勿 vTaskSuspendAll，否则录入 NFC/弹窗时 LVGL 整页卡死 */
#define rs485_proto_mutex_take() ((void)0)
#define rs485_proto_mutex_give() ((void)0)

static uint16_t rs485_crc16_modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    uint16_t j;

    for(i = 0u; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for(j = 0u; j < 8u; j++) {
            if((crc & 1u) != 0u) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

static uint16_t rs485_pack_frame_ex(uint8_t *buf, uint16_t buf_sz,
                                    uint8_t dst, uint8_t src, uint8_t seq, uint8_t cmd,
                                    const uint8_t *payload, uint16_t payload_len)
{
    uint16_t need = (uint16_t)(RS485_HDR_FIXED + payload_len + RS485_CRC_LEN);
    uint16_t crc;

    if(buf == NULL || buf_sz < need) {
        return 0u;
    }

    buf[0] = RS485_SOF0;
    buf[1] = RS485_SOF1;
    buf[2] = dst;
    buf[3] = src;
    buf[4] = seq;
    buf[5] = cmd;
    buf[6] = (uint8_t)(payload_len & 0xFFu);
    buf[7] = (uint8_t)((payload_len >> 8) & 0xFFu);
    if(payload_len > 0u && payload != NULL) {
        memcpy(buf + RS485_HDR_FIXED, payload, payload_len);
    }
    crc = rs485_crc16_modbus(buf + 2u, (uint16_t)(6u + payload_len));
    buf[RS485_HDR_FIXED + payload_len] = (uint8_t)(crc & 0xFFu);
    buf[RS485_HDR_FIXED + payload_len + 1u] = (uint8_t)((crc >> 8) & 0xFFu);
    return need;
}

static uint16_t rs485_pack_frame(uint8_t *buf, uint16_t buf_sz, uint8_t cmd, const uint8_t *payload, uint16_t payload_len)
{
    uint8_t seq = s_tx_seq++;
    return rs485_pack_frame_ex(buf, buf_sz, APP_RS485_PEER_ADDR, APP_RS485_LOCAL_ADDR, seq, cmd, payload, payload_len);
}

static uint8_t rs485_frame_crc_ok(const uint8_t *f, uint16_t n)
{
    uint16_t plen = (uint16_t)f[6] | ((uint16_t)f[7] << 8);
    uint16_t need = (uint16_t)(RS485_HDR_FIXED + plen + RS485_CRC_LEN);
    uint16_t crc_ok;
    uint16_t crc_rx;

    if(n < need) {
        return 0u;
    }
    crc_ok = rs485_crc16_modbus(f + 2u, (uint16_t)(6u + plen));
    crc_rx = (uint16_t)f[RS485_HDR_FIXED + plen] | ((uint16_t)f[RS485_HDR_FIXED + plen + 1u] << 8);
    return (crc_rx == crc_ok) ? 1u : 0u;
}

static uint8_t rs485_parse_rsp(const uint8_t *rsp, uint16_t rsp_len, uint8_t expect_rsp_cmd, uint8_t req_seq)
{
    uint16_t plen;
    uint16_t need;

    if(rsp_len < (uint16_t)(RS485_HDR_FIXED + RS485_CRC_LEN)) {
        return 0u;
    }
    if(rsp[0] != RS485_SOF0 || rsp[1] != RS485_SOF1) {
        return 0u;
    }
    if(rsp[2] != APP_RS485_LOCAL_ADDR || rsp[3] != APP_RS485_PEER_ADDR) {
        return 0u;
    }
    if(rsp[4] != req_seq || rsp[5] != expect_rsp_cmd) {
        return 0u;
    }
    plen = (uint16_t)rsp[6] | ((uint16_t)rsp[7] << 8);
    need = (uint16_t)(RS485_HDR_FIXED + plen + RS485_CRC_LEN);
    if(rsp_len < need) {
        return 0u;
    }
    if(rs485_frame_crc_ok(rsp, rsp_len) == 0u) {
        return 0u;
    }
    if(plen >= 1u && rsp[RS485_HDR_FIXED] != 0u) {
        return 0u;
    }
    return 1u;
}

static bool rs485_send_reply(uint8_t seq, uint8_t rsp_cmd, uint8_t err_byte);
static bool rs485_send_reply_pl(uint8_t seq, uint8_t rsp_cmd, const void *payload, uint16_t payload_len);

#if APP_RS485_IS_MASTER
static const char *rs485_unlock_method_name(uint8_t method_id)
{
    if(method_id == 2u) {
        return "nfc";
    }
    if(method_id == 3u) {
        return "fingerprint";
    }
    return "password";
}

static void host_rs485_slave_unlock_forward_cloud(const uint8_t *pl, uint16_t plen)
{
    char acc[13];
    uint8_t mid;
    int device_id;
    const char *mtd;

    if(pl == NULL) {
        return;
    }
    memset(acc, 0, sizeof(acc));
    if(plen == (uint16_t)sizeof(rs485_unlock_notify_t)) {
        const rs485_unlock_notify_t *un = (const rs485_unlock_notify_t *)pl;

        mid = un->method_id;
        device_id = (int)un->device_id;
        memcpy(acc, un->acc, 12u);
    } else if(plen == 14u) {
        mid = pl[0];
        device_id = CLOUD_UNLOCK_DEVICE_SLAVE;
        memcpy(acc, pl + 1u, 12u);
    } else {
        return;
    }
    acc[12] = '\0';
    if(device_id != CLOUD_UNLOCK_DEVICE_MASTER && device_id != CLOUD_UNLOCK_DEVICE_SLAVE) {
        device_id = CLOUD_UNLOCK_DEVICE_SLAVE;
    }
    if(mid < 1u || mid > 3u) {
        return;
    }
    {
        uint8_t i;
        for(i = 0u; i < 12u; i++) {
            if(acc[i] == '\0') {
                break;
            }
            if(acc[i] < 0x20u || acc[i] > 0x7Eu) {
                return;
            }
        }
    }
    mtd = rs485_unlock_method_name(mid);
    HOST_RS485_LOG("[RS485] SLAVE_UNLOCK_NOTIFY acc=%s method=%s device=%d -> Aliyun\r\n",
                   acc, mtd, device_id);
    cloud_ota_service_report_event(CLOUD_EVT_UNLOCK_OK, acc);
    cloud_ota_service_report_unlock_record_ex(acc, mtd, HAL_GetTick(), device_id);
}
#endif

#if APP_RS485_IS_MASTER
static void rs485_master_stash_rsp_frame(const uint8_t *rxb, uint16_t n)
{
    if(rxb == NULL || n < (uint16_t)(RS485_HDR_FIXED + RS485_CRC_LEN) ||
       n > (uint16_t)sizeof(s_rs485_pending_rsp.buf)) {
        return;
    }
    memcpy(s_rs485_pending_rsp.buf, rxb, n);
    s_rs485_pending_rsp.len = n;
    s_rs485_pending_rsp.valid = 1u;
}

static uint8_t rs485_master_take_pending_rsp(uint8_t *rxb, uint16_t cap, uint16_t *out_n)
{
    if(out_n == NULL || rxb == NULL || s_rs485_pending_rsp.valid == 0u) {
        return 0u;
    }
    if(s_rs485_pending_rsp.len > cap) {
        s_rs485_pending_rsp.valid = 0u;
        return 0u;
    }
    memcpy(rxb, s_rs485_pending_rsp.buf, s_rs485_pending_rsp.len);
    *out_n = s_rs485_pending_rsp.len;
    s_rs485_pending_rsp.valid = 0u;
    return 1u;
}
#endif

#if APP_RS485_IS_MASTER
/* During command wait, there may be unsolicited frames from slave.
 * Handle them immediately so they don't poison req/rsp matching. */
static uint8_t rs485_handle_unsolicited_master_frame(const uint8_t *rxb, uint16_t n)
{
    uint16_t plen;
    uint8_t cmd;
    uint8_t seq;
    const uint8_t *pl;

    if(rxb == NULL || n < (uint16_t)(RS485_HDR_FIXED + RS485_CRC_LEN)) {
        return 0u;
    }
    if(rs485_frame_crc_ok(rxb, n) == 0u) {
        return 0u;
    }
    if(rxb[2] != APP_RS485_LOCAL_ADDR || rxb[3] != APP_RS485_PEER_ADDR) {
        return 0u;
    }

    cmd = rxb[5];
    seq = rxb[4];
    plen = (uint16_t)rxb[6] | ((uint16_t)rxb[7] << 8);
    pl = rxb + RS485_HDR_FIXED;

    if(cmd == RS485_CMD_MIRROR_SYNC_REQ && plen == 0u) {
        s_mirror_sync_pending = 1u;
        (void)rs485_send_reply(seq, (uint8_t)(RS485_CMD_MIRROR_SYNC_REQ | 0x80u), 0u);
        return 1u;
    }
    if(cmd == RS485_CMD_FP_COMMIT_NOTIFY && plen == (uint16_t)sizeof(rs485_fp_commit_t)) {
        const rs485_fp_commit_t *cm = (const rs485_fp_commit_t *)pl;
        s_fp_commit_page = cm->page_id;
        s_fp_commit_status = cm->status;
        s_fp_commit_pending = 1u;
        (void)rs485_send_reply(seq, (uint8_t)(RS485_CMD_FP_COMMIT_NOTIFY | 0x80u), 0u);
        return 1u;
    }
    if(cmd == RS485_CMD_SLAVE_UNLOCK_NOTIFY &&
       (plen == 14u || plen == (uint16_t)sizeof(rs485_unlock_notify_t))) {
        host_rs485_slave_unlock_forward_cloud(pl, plen);
        (void)rs485_send_reply(seq, (uint8_t)(RS485_CMD_SLAVE_UNLOCK_NOTIFY | 0x80u), 0u);
        return 1u;
    }
    if(cmd == RS485_CMD_FP_MATCH_CHAR && plen == (uint16_t)sizeof(rs485_fp_char_chunk_t)) {
        const rs485_fp_char_chunk_t *ch = (const rs485_fp_char_chunk_t *)pl;
        rs485_fp_match_rsp_t mrsp;
        uint8_t done;

        memset(&mrsp, 0, sizeof(mrsp));
        done = app_fp_host_match_char_chunk(ch, &mrsp);
        if(done == 0u) {
            (void)rs485_send_reply(seq, (uint8_t)(RS485_CMD_FP_MATCH_CHAR | 0x80u), 0u);
        } else {
            (void)rs485_send_reply_pl(seq, (uint8_t)(RS485_CMD_FP_MATCH_CHAR | 0x80u),
                                      &mrsp, (uint16_t)sizeof(mrsp));
        }
        return 1u;
    }
    return 0u;
}
#endif

#if (APP_USE_FREERTOS == 1)
static void rs485_post_tx_gap(void)
{
    vTaskDelay(pdMS_TO_TICKS(APP_RS485_POST_TX_GAP_MS));
}
#else
static void rs485_post_tx_gap(void)
{
    HAL_Delay(APP_RS485_POST_TX_GAP_MS);
}
#endif

static bool rs485_do_cmd(uint8_t cmd, const uint8_t *payload, uint16_t payload_len, uint32_t tout_ms)
{
    uint8_t txb[RS485_HDR_FIXED + 160u + RS485_CRC_LEN];
    uint8_t rxb[RS485_HDR_FIXED + 32u + RS485_CRC_LEN];
    uint16_t tx_n;
    uint8_t seq_save;
    uint16_t plen;
    uint16_t total;
    HAL_StatusTypeDef st;
    bool ok_ret = false;
    uint8_t attempt;
    uint8_t last_rsp_cmd = 0u;
    uint8_t last_rsp_seq = 0u;
    uint16_t last_rsp_plen = 0u;

    if(app_rs485_link_ready() == 0u) {
        return false;
    }

    rs485_proto_mutex_take();
#if APP_RS485_IS_MASTER
    if(rs485_master_try_enter_cmd() == 0u) {
        rs485_proto_mutex_give();
        return false;
    }
#endif

    tx_n = rs485_pack_frame(txb, (uint16_t)sizeof(txb), cmd, payload, payload_len);
    if(tx_n == 0u) {
        goto out;
    }
    seq_save = txb[4];

    for(attempt = 0u; attempt < APP_RS485_CMD_RETRY_MAX; attempt++) {
        if(attempt > 0u) {
#if (APP_USE_FREERTOS == 1)
            vTaskDelay(pdMS_TO_TICKS(APP_RS485_MIRROR_INTER_CMD_MS));
#endif
            app_rs485_flush_rx();
        } else {
            app_rs485_flush_rx();
        }

        st = app_rs485_raw_send(txb, tx_n, (payload_len > 64u) ? 200u : 80u);
        if(st != HAL_OK) {
            if(cmd != 0x01u) {
                HOST_RS485_LOG("[RS485] tx cmd=0x%02X send_st=%d try=%u\r\n",
                               cmd, (int)st, (unsigned)(attempt + 1u));
            } else {
                HOST_RS485_LOG("[RS485] PING tx fail st=%d try=%u\r\n", (int)st, (unsigned)(attempt + 1u));
            }
            continue;
        }

        /* 发完立刻收：RX 中断环形缓冲会保存从机快速 ACK，禁止再长 delay 后才开始收 */
        rs485_post_tx_gap();
        if(payload_len > 64u) {
#if (APP_USE_FREERTOS == 1)
            vTaskDelay(pdMS_TO_TICKS(3u));
#else
            HAL_Delay(3u);
#endif
        }
        memset(rxb, 0, sizeof(rxb));
        {
            uint8_t saw_frame = 0u;
            uint32_t wait_deadline_ms = HAL_GetTick() + tout_ms;
#if APP_RS485_IS_MASTER
            uint16_t pending_n = 0u;

            if(rs485_master_take_pending_rsp(rxb, (uint16_t)sizeof(rxb), &pending_n) != 0u) {
                uint16_t pending_total;
                uint16_t pending_plen;

                saw_frame = 1u;
                pending_plen = (uint16_t)rxb[6] | ((uint16_t)rxb[7] << 8);
                pending_total = (uint16_t)(RS485_HDR_FIXED + pending_plen + RS485_CRC_LEN);
                if(pending_total <= pending_n &&
                   rs485_parse_rsp(rxb, pending_total, (uint8_t)(cmd | 0x80u), seq_save) != 0u) {
                    ok_ret = true;
                }
            }
#endif

            for(;;) {
                if(ok_ret) {
                    break;
                }
                uint32_t now_ms = HAL_GetTick();
                uint32_t left_ms;
                uint32_t slice_ms;
                uint16_t nfr;

                if((int32_t)(wait_deadline_ms - now_ms) <= 0) {
                    break;
                }
                left_ms = (uint32_t)(wait_deadline_ms - now_ms);
                slice_ms = (left_ms > 60u) ? 60u : left_ms;
                if(slice_ms == 0u) {
                    slice_ms = 1u;
                }

                nfr = app_rs485_recv_frame(rxb, (uint16_t)sizeof(rxb), slice_ms);
                if(nfr < (uint16_t)(RS485_HDR_FIXED + RS485_CRC_LEN)) {
                    continue;
                }
                saw_frame = 1u;
                plen = (uint16_t)rxb[6] | ((uint16_t)rxb[7] << 8);
                total = (uint16_t)(RS485_HDR_FIXED + plen + RS485_CRC_LEN);
                if(total > (uint16_t)sizeof(rxb) || nfr < total) {
                    continue;
                }
                last_rsp_cmd = rxb[5];
                last_rsp_seq = rxb[4];
                last_rsp_plen = plen;

                if(rs485_parse_rsp(rxb, total, (uint8_t)(cmd | 0x80u), seq_save) != 0u) {
                    ok_ret = true;
                    break;
                }
#if APP_RS485_IS_MASTER
                if(rs485_handle_unsolicited_master_frame(rxb, total) != 0u) {
                    continue;
                }
#endif
            }

            if(!ok_ret && attempt + 1u >= APP_RS485_CMD_RETRY_MAX) {
                if(!saw_frame) {
                    if(cmd == 0x01u) {
                        HOST_RS485_LOG("[RS485] PING no rsp tout=%lums (no A5 5A frame on bus)\r\n",
                                       (unsigned long)tout_ms);
                    } else {
                        HOST_RS485_LOG("[RS485] cmd=0x%02X no rsp try=%u\r\n",
                                       cmd, (unsigned)(attempt + 1u));
                    }
                } else {
                    HOST_RS485_LOG("[RS485] cmd=0x%02X rsp mismatch/timeout last_cmd=0x%02X last_seq=%u last_plen=%u\r\n",
                                   cmd, last_rsp_cmd, (unsigned)last_rsp_seq, (unsigned)last_rsp_plen);
                }
            }
        }
        if(ok_ret) {
            break;
        }
    }
out:
    (void)last_rsp_cmd;
    (void)last_rsp_seq;
    (void)last_rsp_plen;
#if APP_RS485_IS_MASTER
    rs485_master_leave_cmd();
#endif
    rs485_proto_mutex_give();
    return ok_ret;
}

#if APP_RS485_IS_MASTER
uint8_t app_rs485_master_cmd_in_progress(void)
{
    return s_rs485_master_cmd_active;
}
#endif

static bool rs485_send_reply(uint8_t seq, uint8_t rsp_cmd, uint8_t err_byte)
{
    uint8_t txb[RS485_HDR_FIXED + 4u + RS485_CRC_LEN];
    uint16_t tx_n;
    const uint8_t *pl = NULL;
    uint16_t plen = 0u;
    uint8_t eb;

    if(err_byte != 0u) {
        eb = err_byte;
        pl = &eb;
        plen = 1u;
    }
    tx_n = rs485_pack_frame_ex(txb, (uint16_t)sizeof(txb), APP_RS485_PEER_ADDR, APP_RS485_LOCAL_ADDR,
                               seq, rsp_cmd, pl, plen);
    if(tx_n == 0u) {
        return false;
    }
    return (app_rs485_raw_send(txb, tx_n, 80u) == HAL_OK) ? true : false;
}

static bool rs485_send_reply_pl(uint8_t seq, uint8_t rsp_cmd, const void *payload, uint16_t payload_len)
{
    uint8_t txb[RS485_HDR_FIXED + 32u + RS485_CRC_LEN];
    uint16_t tx_n;

    if(payload == NULL || payload_len == 0u) {
        return false;
    }
    tx_n = rs485_pack_frame_ex(txb, (uint16_t)sizeof(txb), APP_RS485_PEER_ADDR, APP_RS485_LOCAL_ADDR,
                               seq, rsp_cmd, (const uint8_t *)payload, payload_len);
    if(tx_n == 0u) {
        return false;
    }
    return (app_rs485_raw_send(txb, tx_n, 80u) == HAL_OK) ? true : false;
}

bool app_rs485_proto_ping_peer(uint32_t tout_ms)
{
    return rs485_do_cmd(0x01u, NULL, 0u, tout_ms);
}

bool app_rs485_proto_slave_user_add(const user_cred_t *u, uint32_t tout_ms)
{
    if(u == NULL) {
        return false;
    }
    return rs485_do_cmd(0x02u, (const uint8_t *)u, (uint16_t)sizeof(*u), tout_ms);
}

bool app_rs485_proto_slave_user_delete(const char *acc, uint32_t tout_ms)
{
    uint8_t acc13[13];

    if(acc == NULL) {
        return false;
    }
    memset(acc13, 0, sizeof(acc13));
    strncpy((char *)acc13, acc, sizeof(acc13) - 1u);
    return rs485_do_cmd(0x03u, acc13, (uint16_t)sizeof(acc13), tout_ms);
}

bool app_rs485_proto_slave_fp_template_chunk(const rs485_fp_tpl_chunk_t *chunk, uint32_t tout_ms)
{
    if(chunk == NULL) {
        return false;
    }
    /* 与 USER_ADD 相同：post_tx_gap + 重试；旧 rs485_do_cmd_payload 无这些导致 tpl 分片无应答 */
    HOST_RS485_LOG_VERBOSE("[RS485][FP] tx tpl chunk page=%u idx=%u/%u tout=%lums\r\n",
                           (unsigned)chunk->page_id, (unsigned)chunk->chunk_idx,
                           (unsigned)(chunk->chunk_count - 1u), (unsigned long)tout_ms);
    return rs485_do_cmd(RS485_CMD_FP_TEMPLATE, (const uint8_t *)chunk,
                        (uint16_t)sizeof(*chunk), tout_ms);
}

bool app_rs485_proto_slave_unlock_notify(const char *acc, uint8_t method_id, uint32_t tout_ms)
{
    rs485_unlock_notify_t un;

    memset(&un, 0, sizeof(un));
    un.method_id = method_id;
    un.device_id = RS485_UNLOCK_DEVICE_SLAVE;
    if(acc != NULL) {
        strncpy(un.acc, acc, sizeof(un.acc));
    }
    return rs485_do_cmd(RS485_CMD_SLAVE_UNLOCK_NOTIFY, (const uint8_t *)&un,
                        (uint16_t)sizeof(un), tout_ms);
}

#if APP_RS485_IS_MASTER
void app_rs485_master_poll_incoming(uint32_t read_tout_ms)
{
    uint8_t rxb[RS485_HDR_FIXED + 160u + RS485_CRC_LEN];
    uint16_t n;
    uint16_t plen;
    uint8_t cmd;
    uint8_t seq;
    const uint8_t *pl;

    if(app_rs485_link_ready() == 0u) {
        return;
    }
    if(s_rs485_master_cmd_active != 0u) {
        return;
    }

    rs485_proto_mutex_take();
    n = app_rs485_recv_frame(rxb, (uint16_t)sizeof(rxb), read_tout_ms);
    if(n == 0u) {
        rs485_proto_mutex_give();
        return;
    }
    HOST_RS485_LOG_VERBOSE("[RS485] rx frame n=%u: A5 5A dst=%02X src=%02X seq=%u cmd=0x%02X len=%u\r\n",
                           (unsigned)n, rxb[2], rxb[3], (unsigned)rxb[4], rxb[5],
                           (unsigned)((uint16_t)rxb[6] | ((uint16_t)rxb[7] << 8)));
    if(rs485_frame_crc_ok(rxb, n) == 0u) {
        HOST_RS485_LOG("[RS485] CRC fail (plen field may be garbage)\r\n");
        rs485_proto_mutex_give();
        return;
    }
    if(rxb[2] != APP_RS485_LOCAL_ADDR || rxb[3] != APP_RS485_PEER_ADDR) {
        HOST_RS485_LOG("[RS485] addr mismatch want dst=%02X src=%02X (host=0x01 peer=0x02)\r\n",
                       APP_RS485_LOCAL_ADDR, APP_RS485_PEER_ADDR);
        rs485_proto_mutex_give();
        return;
    }

    cmd = rxb[5];
    seq = rxb[4];
    plen = (uint16_t)rxb[6] | ((uint16_t)rxb[7] << 8);
    pl = rxb + RS485_HDR_FIXED;

    if(cmd == RS485_CMD_MIRROR_SYNC_REQ && plen == 0u) {
        s_mirror_sync_pending = 1u;
        HOST_RS485_LOG("[RS485][USER] mirror_req from slave (ACK+full sync)\r\n");
        if(!rs485_send_reply(seq, (uint8_t)(RS485_CMD_MIRROR_SYNC_REQ | 0x80u), 0u)) {
            HOST_RS485_LOG("[RS485] mirror_req ACK tx fail\r\n");
        }
    } else if(cmd == RS485_CMD_FP_COMMIT_NOTIFY && plen == (uint16_t)sizeof(rs485_fp_commit_t)) {
        const rs485_fp_commit_t *cm = (const rs485_fp_commit_t *)pl;
        s_fp_commit_page = cm->page_id;
        s_fp_commit_status = cm->status;
        s_fp_commit_pending = 1u;
        HOST_RS485_LOG("[RS485][FP] rx commit notify page=%u status=0x%02X seq=%u\r\n",
                       (unsigned)cm->page_id, (unsigned)cm->status, (unsigned)seq);
        if(!rs485_send_reply(seq, (uint8_t)(RS485_CMD_FP_COMMIT_NOTIFY | 0x80u), 0u)) {
            HOST_RS485_LOG("[RS485] fp_commit ACK tx fail\r\n");
        }
    } else if(cmd == RS485_CMD_SLAVE_UNLOCK_NOTIFY &&
              (plen == 14u || plen == (uint16_t)sizeof(rs485_unlock_notify_t))) {
        host_rs485_slave_unlock_forward_cloud(pl, plen);
        if(!rs485_send_reply(seq, (uint8_t)(RS485_CMD_SLAVE_UNLOCK_NOTIFY | 0x80u), 0u)) {
            HOST_RS485_LOG("[RS485] unlock ACK tx fail\r\n");
        }
    } else if(cmd == RS485_CMD_FP_MATCH_CHAR && plen == (uint16_t)sizeof(rs485_fp_char_chunk_t)) {
        const rs485_fp_char_chunk_t *ch = (const rs485_fp_char_chunk_t *)pl;
        rs485_fp_match_rsp_t mrsp;
        uint8_t done;

        memset(&mrsp, 0, sizeof(mrsp));
        done = app_fp_host_match_char_chunk(ch, &mrsp);
        if(done == 0u) {
            (void)rs485_send_reply(seq, (uint8_t)(RS485_CMD_FP_MATCH_CHAR | 0x80u), 0u);
        } else {
            (void)rs485_send_reply_pl(seq, (uint8_t)(RS485_CMD_FP_MATCH_CHAR | 0x80u),
                                      &mrsp, (uint16_t)sizeof(mrsp));
        }
    } else if((cmd & 0x80u) != 0u) {
        /* 从机应答帧(0x81/0x82/...)：留给 rs485_do_cmd，避免 poll 吃掉导致 PING FAIL */
        rs485_master_stash_rsp_frame(rxb, n);
    } else {
        HOST_RS485_LOG("[RS485] unhandled cmd=0x%02X plen=%u\r\n", cmd, (unsigned)plen);
    }

    rs485_proto_mutex_give();
}

void app_rs485_master_mirror_if_requested(void)
{
    if(s_mirror_sync_pending == 0u) {
        return;
    }
    s_mirror_sync_pending = 0u;
    users_mirror_to_slave_on_link();
}

void app_rs485_master_clear_fp_commit(void)
{
    s_fp_commit_pending = 0u;
    s_fp_commit_page = 0xFFFFu;
    s_fp_commit_status = 0xFFu;
}

uint8_t app_rs485_master_take_fp_commit(uint16_t *page_out, uint8_t *status_out)
{
    if(page_out == NULL || status_out == NULL || s_fp_commit_pending == 0u) {
        return 0u;
    }
    *page_out = s_fp_commit_page;
    *status_out = s_fp_commit_status;
    s_fp_commit_pending = 0u;
    return 1u;
}
#else
void app_rs485_master_poll_incoming(uint32_t read_tout_ms)
{
    (void)read_tout_ms;
}

uint8_t app_rs485_master_take_fp_commit(uint16_t *page_out, uint8_t *status_out)
{
    (void)page_out;
    (void)status_out;
    return 0u;
}
#endif

#if APP_RS485_IS_SLAVE
void app_rs485_slave_server_poll(uint32_t read_tout_ms)
{
    uint8_t rxb[RS485_HDR_FIXED + sizeof(user_cred_t) + RS485_CRC_LEN];
    uint16_t n;
    uint16_t plen;
    uint8_t cmd;
    uint8_t seq;
    uint8_t err;
    bool okb;

    if(app_rs485_link_ready() == 0u) {
        return;
    }

    rs485_proto_mutex_take();
    n = app_rs485_recv_frame(rxb, (uint16_t)sizeof(rxb), read_tout_ms);
    if(n == 0u) {
        rs485_proto_mutex_give();
        return;
    }
    if(rs485_frame_crc_ok(rxb, n) == 0u) {
        rs485_proto_mutex_give();
        return;
    }
    if(rxb[2] != APP_RS485_LOCAL_ADDR || rxb[3] != APP_RS485_PEER_ADDR) {
        rs485_proto_mutex_give();
        return;
    }

    cmd = rxb[5];
    seq = rxb[4];
    plen = (uint16_t)rxb[6] | ((uint16_t)rxb[7] << 8);

    err = 0u;
    okb = true;

    if(cmd == 0x01u && plen == 0u) {
        okb = rs485_send_reply(seq, 0x81u, 0u);
    } else if(cmd == 0x02u && plen == (uint16_t)sizeof(user_cred_t)) {
        user_cred_t u;
        memcpy(&u, rxb + RS485_HDR_FIXED, sizeof(u));
        okb = users_slave_mirror_apply_user(&u);
        err = okb ? 0u : 1u;
        okb = rs485_send_reply(seq, 0x82u, err);
    } else if(cmd == 0x03u && plen == 13u) {
        char acc[13];
        memcpy(acc, rxb + RS485_HDR_FIXED, sizeof(acc));
        acc[12] = '\0';
        okb = users_slave_mirror_delete_by_acc(acc);
        err = okb ? 0u : 1u;
        okb = rs485_send_reply(seq, 0x83u, err);
    } else {
        okb = rs485_send_reply(seq, (uint8_t)(cmd | 0x80u), 1u);
    }

    (void)okb;
    rs485_proto_mutex_give();
}
#else
void app_rs485_slave_server_poll(uint32_t read_tout_ms)
{
    (void)read_tout_ms;
}
#endif

#endif /* APP_RS485_ENABLE */
