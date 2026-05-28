#include "app_rs485_proto.h"

#if (APP_RS485_ENABLE == 1)

#include <string.h>

#include "app_rs485_link.h"

#if APP_RS485_IS_SLAVE
#include "app_state.h"
#include "app_user_ops.h"
#include "app_slave_diag.h"
#include "app_slave_fp_sync.h"
#include "app_fp_mirror_diag.h"
#endif

#if APP_RS485_IS_MASTER
#include "cloud_ota_service.h"
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

static uint8_t s_tx_seq;
#if APP_RS485_IS_SLAVE
static volatile uint8_t s_slave_mirror_sync_seen;
#endif

#if (APP_USE_FREERTOS == 1)
#if APP_RS485_IS_SLAVE
/* 本工程 FreeRTOS 无 semphr.h；从机 USART6 仅 Rs485Slave 任务访问，无需互斥。 */
#define rs485_proto_mutex_take() ((void)0)
#define rs485_proto_mutex_give() ((void)0)

static volatile uint8_t s_unlock_notify_pending;
static char s_unlock_notify_acc[13];
static uint8_t s_unlock_notify_mid;
static volatile uint8_t s_fp_commit_pending;
static uint16_t s_fp_commit_page;
static uint8_t s_fp_commit_status;

void app_rs485_slave_unlock_notify_async(const char *acc, uint8_t method_id)
{
    if(acc == NULL) {
        acc = "";
    }
    strncpy(s_unlock_notify_acc, acc, sizeof(s_unlock_notify_acc) - 1u);
    s_unlock_notify_acc[sizeof(s_unlock_notify_acc) - 1u] = '\0';
    s_unlock_notify_mid = method_id;
    s_unlock_notify_pending = 1u;
}

void app_rs485_slave_flush_pending_notify(void)
{
    uint8_t try_n;
    bool ok = false;

    if(s_unlock_notify_pending == 0u && s_fp_commit_pending == 0u) {
        return;
    }

    if(s_unlock_notify_pending != 0u) {
        ok = false;
        for(try_n = 0u; try_n < 3u; try_n++) {
            if(try_n > 0u) {
                vTaskDelay(pdMS_TO_TICKS(40u));
            }
            ok = app_rs485_proto_slave_unlock_notify(s_unlock_notify_acc, s_unlock_notify_mid, 400u);
            if(ok) {
                break;
            }
        }
        s_unlock_notify_pending = 0u;
#if (APP_SLAVE_USART1_DEBUG != 0)
        if(!ok) {
            SLAVE_DBG_LOG("[SLV] unlock_notify FAIL acc=%s mid=%u", s_unlock_notify_acc,
                          (unsigned)s_unlock_notify_mid);
        }
#endif
    }

    if(s_fp_commit_pending != 0u) {
        uint8_t max_try = (s_fp_commit_status == 0u) ? 3u : 1u;

        FP_MIRROR_LOG("[SLV][FP] commit tx page=%u status=0x%02X",
                      (unsigned)s_fp_commit_page, (unsigned)s_fp_commit_status);
        ok = false;
        for(try_n = 0u; try_n < max_try; try_n++) {
            if(try_n > 0u) {
                vTaskDelay(pdMS_TO_TICKS(40u));
            }
            if(app_rs485_proto_slave_fp_template_commit(s_fp_commit_page, s_fp_commit_status, 500u)) {
                ok = true;
                FP_MIRROR_LOG("[SLV][FP] commit tx OK page=%u status=0x%02X",
                              (unsigned)s_fp_commit_page, (unsigned)s_fp_commit_status);
                break;
            }
        }
        if(ok) {
            s_fp_commit_pending = 0u;
        } else {
            FP_MIRROR_LOG("[SLV][FP] commit tx FAIL page=%u status=0x%02X",
                          (unsigned)s_fp_commit_page, (unsigned)s_fp_commit_status);
        }
    }
}
#else
static void rs485_proto_mutex_take(void)
{
    vTaskSuspendAll();
}

static void rs485_proto_mutex_give(void)
{
    (void)xTaskResumeAll();
}
#endif
#else
#define rs485_proto_mutex_take() ((void)0)
#define rs485_proto_mutex_give() ((void)0)
#endif

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

static bool rs485_do_cmd(uint8_t cmd, const uint8_t *payload, uint16_t payload_len, uint32_t tout_ms);

#if APP_RS485_IS_SLAVE
static bool app_rs485_slave_send_mirror_request(void)
{
    uint8_t txb[RS485_HDR_FIXED + RS485_CRC_LEN];
    uint16_t tx_n;

    if(app_rs485_link_ready() == 0u) {
        return false;
    }
    tx_n = rs485_pack_frame(txb, (uint16_t)sizeof(txb), RS485_CMD_MIRROR_SYNC_REQ, NULL, 0u);
    if(tx_n == 0u) {
        return false;
    }
    app_rs485_flush_rx();
    return (app_rs485_raw_send(txb, tx_n, 80u) == HAL_OK) ? true : false;
}

void app_rs485_slave_upkeep_mirror_request(void)
{
    static uint8_t s_tries;
    static uint32_t s_next_ms;
    static uint32_t s_boot_ms;
    uint32_t now = HAL_GetTick();

    if(s_slave_mirror_sync_seen != 0u) {
        return;
    }
    if(s_boot_ms == 0u) {
        s_boot_ms = now;
    }
    if((now - s_boot_ms) < APP_RS485_MIRROR_BOOT_QUIET_MS) {
        return;
    }
    if(s_tries >= 5u) {
        return;
    }
    if((int32_t)(now - s_next_ms) < 0) {
        return;
    }
    s_next_ms = now + 2000u;
    if(app_rs485_slave_send_mirror_request()) {
        s_tries++;
#if (APP_SLAVE_USART1_DEBUG != 0)
        SLAVE_DBG_LOG("[SLV][USER] mirror_req tx try=%u", (unsigned)s_tries);
#endif
        /* 发完请求后多监听一会，便于收到主机 0x84 应答（非必须，但利于排障） */
        app_rs485_slave_server_poll(40u);
    }
}
#endif

#if APP_RS485_IS_SLAVE
bool app_rs485_proto_slave_fp_template_commit(uint16_t page_id, uint8_t status, uint32_t tout_ms)
{
    rs485_fp_commit_t cm;

    cm.page_id = page_id;
    cm.status = status;
    return rs485_do_cmd(RS485_CMD_FP_COMMIT_NOTIFY, (const uint8_t *)&cm, (uint16_t)sizeof(cm), tout_ms);
}

void app_rs485_slave_fp_commit_async(uint16_t page_id, uint8_t status)
{
    /* 相同 page+status 已在待发队列中则不再叠加，避免 RS485 上 commit 洪水 */
    if(s_fp_commit_pending != 0u &&
       s_fp_commit_page == page_id &&
       s_fp_commit_status == status) {
        return;
    }
    s_fp_commit_page = page_id;
    s_fp_commit_status = status;
    s_fp_commit_pending = 1u;
}

static uint8_t rs485_frame_crc_ok(const uint8_t *f, uint16_t n);

static bool rs485_do_cmd_fp_match_chunk(const rs485_fp_char_chunk_t *ch, rs485_fp_match_rsp_t *rsp_out,
                                        uint8_t is_last, uint32_t tout_ms)
{
    uint8_t txb[RS485_HDR_FIXED + 160u + RS485_CRC_LEN];
    uint8_t rxb[RS485_HDR_FIXED + 40u + RS485_CRC_LEN];
    uint16_t tx_n;
    uint8_t seq_save;
    uint16_t plen;
    uint16_t total;
    HAL_StatusTypeDef st;
    bool ok_ret = false;

    if(ch == NULL || app_rs485_link_ready() == 0u) {
        return false;
    }

    rs485_proto_mutex_take();
    tx_n = rs485_pack_frame(txb, (uint16_t)sizeof(txb), RS485_CMD_FP_MATCH_CHAR,
                             (const uint8_t *)ch, (uint16_t)sizeof(*ch));
    if(tx_n == 0u) {
        goto out;
    }
    seq_save = txb[4];
    app_rs485_flush_rx();
    st = app_rs485_raw_send(txb, tx_n, 80u);
    if(st != HAL_OK) {
        goto out;
    }
#if (APP_USE_FREERTOS == 1)
    vTaskDelay(pdMS_TO_TICKS(APP_RS485_POST_TX_GAP_MS));
#else
    HAL_Delay(APP_RS485_POST_TX_GAP_MS);
#endif
    memset(rxb, 0, sizeof(rxb));
    {
        uint16_t nfr = app_rs485_recv_frame(rxb, (uint16_t)sizeof(rxb), tout_ms);

        if(nfr < (uint16_t)(RS485_HDR_FIXED + RS485_CRC_LEN)) {
            goto out;
        }
        plen = (uint16_t)rxb[6] | ((uint16_t)rxb[7] << 8);
        total = (uint16_t)(RS485_HDR_FIXED + plen + RS485_CRC_LEN);
        if(total > (uint16_t)sizeof(rxb) || nfr < total) {
            goto out;
        }
        if(rs485_frame_crc_ok(rxb, total) == 0u) {
            goto out;
        }
        if(rxb[2] != APP_RS485_LOCAL_ADDR || rxb[3] != APP_RS485_PEER_ADDR) {
            goto out;
        }
        if(rxb[4] != seq_save || rxb[5] != (uint8_t)(RS485_CMD_FP_MATCH_CHAR | 0x80u)) {
            goto out;
        }
        if(is_last != 0u && rsp_out != NULL &&
           plen == (uint16_t)sizeof(rs485_fp_match_rsp_t)) {
            memcpy(rsp_out, rxb + RS485_HDR_FIXED, sizeof(rs485_fp_match_rsp_t));
            ok_ret = true;
            goto out;
        }
        if(plen == 0u || (plen == 1u && rxb[RS485_HDR_FIXED] == 0u)) {
            ok_ret = true;
        }
    }
out:
    rs485_proto_mutex_give();
    return ok_ret;
}

bool app_rs485_slave_fp_match_request(const uint8_t *feat512, rs485_fp_match_rsp_t *rsp, uint32_t tout_ms)
{
    rs485_fp_char_chunk_t ch;
    uint8_t i;
    uint16_t match_seq;
    uint32_t per_ms;
    static uint16_t s_match_seq = 0u;

    if(feat512 == NULL || rsp == NULL || app_rs485_link_ready() == 0u) {
        return false;
    }
    per_ms = tout_ms / 4u;
    if(per_ms < 250u) {
        per_ms = 250u;
    }
    memset(rsp, 0, sizeof(*rsp));
    rsp->result = RS485_FP_MATCH_RESULT_ERR;

    s_match_seq++;
    match_seq = s_match_seq;

    for(i = 0u; i < RS485_FP_CHUNK_COUNT; i++) {
        memset(&ch, 0, sizeof(ch));
        ch.page_id = match_seq;
        ch.chunk_idx = i;
        ch.chunk_count = RS485_FP_CHUNK_COUNT;
        memcpy(ch.data, feat512 + ((uint32_t)i * RS485_FP_CHUNK_BYTES), RS485_FP_CHUNK_BYTES);
        if(rs485_do_cmd_fp_match_chunk(&ch, rsp, (i + 1u >= RS485_FP_CHUNK_COUNT) ? 1u : 0u, per_ms) == false) {
            return false;
        }
    }
    return true;
}
#endif

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

    if(app_rs485_link_ready() == 0u) {
        return false;
    }

    rs485_proto_mutex_take();

    tx_n = rs485_pack_frame(txb, (uint16_t)sizeof(txb), cmd, payload, payload_len);
    if(tx_n == 0u) {
        goto out;
    }
    seq_save = txb[4];

    app_rs485_flush_rx();
    st = app_rs485_raw_send(txb, tx_n, 80u);
    if(st != HAL_OK) {
        goto out;
    }

    /* RX: recv_frame syncs A5 5A then full frame; do not blind-read 8 bytes. */

    memset(rxb, 0, sizeof(rxb));
    {
        uint16_t nfr = app_rs485_recv_frame(rxb, (uint16_t)sizeof(rxb), tout_ms);

        if(nfr < (uint16_t)(RS485_HDR_FIXED + RS485_CRC_LEN)) {
            goto out;
        }
        plen = (uint16_t)rxb[6] | ((uint16_t)rxb[7] << 8);
        total = (uint16_t)(RS485_HDR_FIXED + plen + RS485_CRC_LEN);
        if(total > (uint16_t)sizeof(rxb) || nfr < total) {
            goto out;
        }
    }
    ok_ret = (rs485_parse_rsp(rxb, total, (uint8_t)(cmd | 0x80u), seq_save) != 0u) ? true : false;
out:
    rs485_proto_mutex_give();
    return ok_ret;
}

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
    /* Give host enough time to switch from TX to RX before slave ACK. */
#if (APP_USE_FREERTOS == 1)
    vTaskDelay(pdMS_TO_TICKS(15u));
#else
    HAL_Delay(15u);
#endif
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
    uint8_t rxb[RS485_HDR_FIXED + 32u + RS485_CRC_LEN];
    uint16_t n;
    uint16_t plen;
    uint8_t cmd;
    uint8_t seq;
    const uint8_t *pl;

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
    pl = rxb + RS485_HDR_FIXED;

    if(cmd == RS485_CMD_SLAVE_UNLOCK_NOTIFY && plen == 14u) {
        char acc[13];
        uint8_t mid = pl[0];
        const char *mtd = "password";

        memcpy(acc, pl + 1u, 12u);
        acc[12] = '\0';
        if(mid == 2u) {
            mtd = "nfc";
        } else if(mid == 3u) {
            mtd = "fingerprint";
        } else if(mid == 0u) {
            mtd = "password";
        }

        cloud_ota_service_report_event(CLOUD_EVT_UNLOCK_OK, acc);
        cloud_ota_service_report_unlock_record(acc, mtd, HAL_GetTick());
        (void)rs485_send_reply(seq, (uint8_t)(RS485_CMD_SLAVE_UNLOCK_NOTIFY | 0x80u), 0u);
    }

    rs485_proto_mutex_give();
}
#else
void app_rs485_master_poll_incoming(uint32_t read_tout_ms)
{
    (void)read_tout_ms;
}
#endif

#if APP_RS485_IS_SLAVE
void app_rs485_slave_server_poll(uint32_t read_tout_ms)
{
    uint8_t rxb[RS485_HDR_FIXED + sizeof(rs485_fp_tpl_chunk_t) + RS485_CRC_LEN];
    uint16_t n;
    uint16_t plen;
    uint8_t cmd;
    uint8_t seq;
    uint8_t err;
    bool okb;
    uint32_t listen_ms = read_tout_ms;
    uint8_t fp_chunk_replied = 0u;

    if(app_rs485_link_ready() == 0u) {
        return;
    }
    if(listen_ms == 0u || listen_ms > APP_RS485_SLAVE_LISTEN_MS) {
        listen_ms = APP_RS485_SLAVE_LISTEN_MS;
    }

    n = app_rs485_recv_frame(rxb, (uint16_t)sizeof(rxb), listen_ms);
    if(n == 0u) {
#if (APP_SLAVE_USART1_DEBUG != 0) && (APP_SLAVE_LOG_VERBOSE != 0)
        {
            static uint32_t s_slv_rs485_idle_log_ms;
            uint32_t tnow = HAL_GetTick();
            if((tnow - s_slv_rs485_idle_log_ms) >= 20000u) {
                s_slv_rs485_idle_log_ms = tnow;
                SLAVE_DBG_LOG("[SLV][RS485] listen idle link_ready=%u (wait host frame)",
                              (unsigned)app_rs485_link_ready());
            }
        }
#endif
        return;
    }
#if (APP_SLAVE_USART1_DEBUG != 0) && (APP_SLAVE_LOG_VERBOSE != 0)
    SLAVE_DBG_LOG("[SLV][RS485] rx n=%u dst=%02X src=%02X cmd=0x%02X",
                  (unsigned)n, rxb[2], rxb[3], (unsigned)rxb[5]);
#endif
    if(rs485_frame_crc_ok(rxb, n) == 0u) {
        SLAVE_DBG_LOG("[SLV][RS485] crc fail n=%u", (unsigned)n);
        return;
    }
    if(rxb[2] != APP_RS485_LOCAL_ADDR || rxb[3] != APP_RS485_PEER_ADDR) {
        FP_MIRROR_LOG("[SLV][RS485] addr mismatch want dst=%02X src=%02X got dst=%02X src=%02X",
                      (unsigned)APP_RS485_LOCAL_ADDR, (unsigned)APP_RS485_PEER_ADDR,
                      (unsigned)rxb[2], (unsigned)rxb[3]);
        return;
    }

    cmd = rxb[5];
    seq = rxb[4];
    plen = (uint16_t)rxb[6] | ((uint16_t)rxb[7] << 8);

    err = 0u;
    okb = true;

    if((cmd & 0x80u) != 0u) {
#if APP_RS485_IS_SLAVE
        /* Responses to slave-initiated requests (e.g. mirror_req ACK 0x84). */
        if(cmd == (uint8_t)(RS485_CMD_MIRROR_SYNC_REQ | 0x80u) ||
           cmd == 0x82u || cmd == 0x85u ||
           cmd == (uint8_t)(RS485_CMD_FP_COMMIT_NOTIFY | 0x80u)) {
            s_slave_mirror_sync_seen = 1u;
        }
#endif
        return;
    } else if(cmd == 0x01u && plen == 0u) {
        static uint32_t s_last_ping_log_ms;
        uint32_t now = HAL_GetTick();
        if((now - s_last_ping_log_ms) >= 60000u) {
            s_last_ping_log_ms = now;
            FP_MIRROR_LOG("[SLV][RS485] PING -> ACK (throttled 60s)");
        }
        okb = rs485_send_reply(seq, 0x81u, 0u);
    } else if(cmd == 0x02u && plen == (uint16_t)sizeof(user_cred_t)) {
        user_cred_t u;
        memcpy(&u, rxb + RS485_HDR_FIXED, sizeof(u));
        okb = users_slave_mirror_apply_user(&u);
#if APP_RS485_IS_SLAVE
        s_slave_mirror_sync_seen = 1u;
#endif
        err = okb ? 0u : 1u;
        okb = rs485_send_reply(seq, 0x82u, err);
    } else if(cmd == 0x03u && plen == 13u) {
        char acc[13];
        memcpy(acc, rxb + RS485_HDR_FIXED, sizeof(acc));
        acc[12] = '\0';
        okb = users_slave_mirror_delete_by_acc(acc);
        err = okb ? 0u : 1u;
        okb = rs485_send_reply(seq, 0x83u, err);
    } else if(cmd == RS485_CMD_FP_TEMPLATE && plen == (uint16_t)sizeof(rs485_fp_tpl_chunk_t)) {
        const rs485_fp_tpl_chunk_t *chunk = (const rs485_fp_tpl_chunk_t *)(rxb + RS485_HDR_FIXED);
        /* 只在 AS608 正在写入时拒绝（防止同一页被覆写），
           quiet_active 期间仍可接收并排队，FpTask 会等 quiet 过期后再写 */
        if(app_slave_fp_write_busy() != 0u || app_slave_fp_mirror_hold() != 0u) {
            FP_MIRROR_LOG("[SLV][FP] reject chunk page=%u idx=%u (AS608 busy)",
                          (unsigned)chunk->page_id, (unsigned)chunk->chunk_idx);
            err = 2u; /* busy: ask host retry later */
            okb = rs485_send_reply(seq, 0x85u, err);
            fp_chunk_replied = 1u;
        }
        if(fp_chunk_replied == 0u) {
            okb = app_slave_fp_template_chunk_rx(chunk) ? true : false;
#if APP_RS485_IS_SLAVE
            s_slave_mirror_sync_seen = 1u;
#endif
            err = okb ? 0u : 1u;
            okb = rs485_send_reply(seq, 0x85u, err);
        }
    } else {
        SLAVE_DBG_LOG("[SLV][RS485] unknown cmd=0x%02X plen=%u", (unsigned)cmd, (unsigned)plen);
        okb = rs485_send_reply(seq, (uint8_t)(cmd | 0x80u), 1u);
    }

#if (APP_SLAVE_USART1_DEBUG != 0)
    if((okb == false) || (err != 0u) ||
       ((cmd == 0x01u) && (APP_SLAVE_LOG_VERBOSE != 0))) {
        SLAVE_DBG_LOG("[SLV][RS485] reply err_byte=%u send_ok=%u", (unsigned)err, (unsigned)(okb ? 1u : 0u));
    }
#endif
}
#else
void app_rs485_slave_server_poll(uint32_t read_tout_ms)
{
    (void)read_tout_ms;
}
#endif

#endif /* APP_RS485_ENABLE */
