#include "app_slave_fp_sync.h"

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE

#include <string.h>

#include "../Drivers/BSP/AS608/as608.h"
#include "app_home_fp_poll.h"
#include "app_home_unlock.h"
#include "app_fp_mirror_diag.h"
#include "app_rs485_proto.h"

#define AS608_TPL_STAGE_NONE          0u
#define AS608_TPL_STAGE_START_CMD     1u
#define AS608_TPL_STAGE_DATA_ACK      2u
#define AS608_TPL_STAGE_STORE_CHAR    3u

#define FP_SYNC_WRITE_Q_SZ 8u

typedef struct {
    uint16_t page;
    uint8_t tpl[AS608_TEMPLATE_SIZE];
    uint8_t valid;
} fp_sync_tpl_slot_t;

static uint16_t s_asm_page = 0xFFFFu;
static uint8_t s_asm_chunk_mask;
static uint8_t s_asm_buf[AS608_TEMPLATE_SIZE];

static fp_sync_tpl_slot_t s_write_q[FP_SYNC_WRITE_Q_SZ];
static uint8_t s_apply_tpl[AS608_TEMPLATE_SIZE];
static uint32_t s_tpl_write_quiet_until_ms;
static uint8_t s_tpl_write_ever_ok;
static uint8_t s_apply_busy;

/* 每页每类事件只打一条，避免串口刷屏 */
static uint16_t s_log_drop_page = 0xFFFFu;
static uint16_t s_log_defer_page = 0xFFFFu;
static uint16_t s_log_fail_page = 0xFFFFu;

/* 连续写入失败计数：同一页连续失败超过阈值时停止重试，避免无限重试占用 RS485 总线 */
#define FP_WRITE_FAIL_MAX 3u
static uint16_t s_apply_fail_page = 0xFFFFu;
static uint8_t  s_apply_fail_streak = 0u;
static uint8_t  s_mirror_session_emptied = 0u;
static uint8_t  s_write_defer_streak = 0u;
static uint8_t  s_mirror_as608_hold = 0u; /* RS485 收包后至写完：禁止指纹轮询抢 USART3 */

static const char *fp_tpl_stage_str(uint8_t stage)
{
    if(stage == AS608_TPL_STAGE_START_CMD) {
        return "start_cmd_ack";
    }
    if(stage == AS608_TPL_STAGE_DATA_ACK) {
        return "data_end_ack";
    }
    if(stage == AS608_TPL_STAGE_STORE_CHAR) {
        return "store_char";
    }
    return "none";
}

/* 删除旧模板并写入；0x18 时尝试清空指纹库后重试一次。返回 AS608 确认码。 */
static uint8_t slave_fp_tpl_store(uint16_t page, const uint8_t *tpl, uint8_t *fp_hw_inited)
{
    uint8_t en = 0xFFu;
    uint8_t del_en;
    uint8_t try_i;
    uint8_t used_empty = 0u;
    SysPara sp;
    uint16_t tpl_n = 0u;

    if(tpl == NULL || page >= 300u) {
        return 0xFFu;
    }
    as608_rx_buffer_clear();
    HAL_Delay(120);
    if(app_fp_hw_try_handshake_retries(fp_hw_inited, 4u, 80u) == 0u) {
        FP_MIRROR_LOG("[SLV][FP] handshake FAIL before write page=%u rx=%u",
                      (unsigned)page, (unsigned)as608_rx_widx_get());
        return 0xFFu;
    }

    /* 本轮 RS485 镜像首次写入前清空从机指纹库，避免 Flash 碎片导致 StoreChar 0x18 */
    if(s_mirror_session_emptied == 0u) {
        uint8_t em;

        as608_rx_buffer_clear();
        em = GZ_Empty();
        s_mirror_session_emptied = 1u;
        FP_MIRROR_LOG("[SLV][FP] mirror session Empty ret=0x%02X", (unsigned)em);
        HAL_Delay(200);
    }

    memset(&sp, 0, sizeof(sp));
    if(GZ_ReadSysPara(&sp) == 0x00u) {
        /* GZ_max = 指纹库容量，合法页号为 0 .. GZ_max-1；仅在读参成功且容量>0 时校验 */
        FP_MIRROR_LOG("[SLV][FP] AS608 capacity=%u pkt=%u addr=0x%08lX",
                      (unsigned)sp.GZ_max, (unsigned)sp.GZ_size, (unsigned long)sp.GZ_addr);
        if(sp.GZ_max > 0u && page >= sp.GZ_max) {
            FP_MIRROR_LOG("[SLV][FP] page=%u >= capacity %u", (unsigned)page, (unsigned)sp.GZ_max);
            return 0x0Bu;
        }
    } else {
        FP_MIRROR_LOG("[SLV][FP] ReadSysPara FAIL, skip capacity check page=%u", (unsigned)page);
    }

    for(try_i = 0u; try_i < 2u; try_i++) {
        as608_rx_buffer_clear();
        del_en = GZ_DeletChar(page, 1u);
        FP_MIRROR_LOG("[SLV][FP] DeletChar page=%u ret=0x%02X", (unsigned)page, (unsigned)del_en);
        HAL_Delay(50);

        (void)GZ_ValidTempleteNum(&tpl_n);
        FP_MIRROR_LOG("[SLV][FP] tpl_in_chip=%u (before write page=%u)", (unsigned)tpl_n, (unsigned)page);

        as608_rx_buffer_clear();
        HAL_Delay(40);
        en = GZ_WriteTemplatePage(page, tpl, AS608_TEMPLATE_SIZE);
        FP_MIRROR_LOG("[SLV][FP] GZ_WriteTemplatePage page=%u ret=0x%02X stage=%s code=0x%02X rx=%u",
                      (unsigned)page, (unsigned)en,
                      fp_tpl_stage_str(as608_write_tpl_last_stage()),
                      (unsigned)as608_write_tpl_last_code(),
                      (unsigned)as608_write_tpl_last_rx());

        if(en == 0x00u) {
            uint8_t vrf[AS608_TEMPLATE_SIZE];

            as608_rx_buffer_clear();
            HAL_Delay(30);
            if(GZ_ReadTemplatePage(page, vrf, AS608_TEMPLATE_SIZE) == 0x00u) {
                FP_MIRROR_LOG("[SLV][FP] verify readback OK page=%u", (unsigned)page);
            } else {
                FP_MIRROR_LOG("[SLV][FP] verify readback FAIL page=%u", (unsigned)page);
                en = 0x18u;
            }
            break;
        }
        if(en == 0x18u && used_empty == 0u) {
            uint8_t em;

            used_empty = 1u;
            as608_rx_buffer_clear();
            em = GZ_Empty();
            FP_MIRROR_LOG("[SLV][FP] StoreChar 0x18 -> Empty ret=0x%02X, retry", (unsigned)em);
            HAL_Delay(300);
            continue;
        }
        HAL_Delay(30);
    }

    return en;
}

static uint8_t fp_sync_q_push(uint16_t page, const uint8_t *tpl)
{
    uint8_t i;

    if(tpl == NULL || page >= 300u) {
        return 0u;
    }
    for(i = 0u; i < FP_SYNC_WRITE_Q_SZ; i++) {
        if(s_write_q[i].valid != 0u && s_write_q[i].page == page) {
            return 1u;
        }
    }
    for(i = 0u; i < FP_SYNC_WRITE_Q_SZ; i++) {
        if(s_write_q[i].valid == 0u) {
            s_write_q[i].page = page;
            memcpy(s_write_q[i].tpl, tpl, AS608_TEMPLATE_SIZE);
            s_write_q[i].valid = 1u;
            FP_MIRROR_LOG("[SLV][FP] queue push page=%u slot=%u", (unsigned)page, (unsigned)i);
            return 1u;
        }
    }
    if(s_log_drop_page != page) {
        s_log_drop_page = page;
        FP_MIRROR_LOG("[SLV][FP] write queue full, drop page=%u (wait host retry)", (unsigned)page);
    }
    return 0u;
}

static uint8_t fp_sync_q_pop(uint16_t *page_out, uint8_t *tpl_out)
{
    uint8_t i;

    if(page_out == NULL || tpl_out == NULL) {
        return 0u;
    }
    for(i = 0u; i < FP_SYNC_WRITE_Q_SZ; i++) {
        if(s_write_q[i].valid != 0u) {
            *page_out = s_write_q[i].page;
            memcpy(tpl_out, s_write_q[i].tpl, AS608_TEMPLATE_SIZE);
            s_write_q[i].valid = 0u;
            FP_MIRROR_LOG("[SLV][FP] queue pop page=%u slot=%u", (unsigned)(*page_out), (unsigned)i);
            return 1u;
        }
    }
    return 0u;
}

static uint8_t fp_sync_q_has_pending(void)
{
    uint8_t i;

    for(i = 0u; i < FP_SYNC_WRITE_Q_SZ; i++) {
        if(s_write_q[i].valid != 0u) {
            return 1u;
        }
    }
    return 0u;
}

uint8_t app_slave_fp_quiet_active(void)
{
    return ((int32_t)(HAL_GetTick() - s_tpl_write_quiet_until_ms) < 0) ? 1u : 0u;
}

uint8_t app_slave_fp_template_chunk_rx(const rs485_fp_tpl_chunk_t *chunk)
{
    uint32_t off;
    uint8_t was_empty;

    if(chunk == NULL) {
        return 0u;
    }
    if(chunk->chunk_count != RS485_FP_CHUNK_COUNT ||
       chunk->chunk_idx >= RS485_FP_CHUNK_COUNT ||
       chunk->page_id >= 300u) {
        return 0u;
    }

    if(s_asm_page != chunk->page_id) {
        if(s_asm_page != 0xFFFFu && s_asm_chunk_mask != 0u) {
            FP_MIRROR_LOG("[SLV][FP] asm reset old_page=%u mask=0x%02X new_page=%u",
                          (unsigned)s_asm_page, (unsigned)s_asm_chunk_mask, (unsigned)chunk->page_id);
        }
        s_asm_page = chunk->page_id;
        s_asm_chunk_mask = 0u;
        memset(s_asm_buf, 0, sizeof(s_asm_buf));
    }

    was_empty = (s_asm_chunk_mask == 0u) ? 1u : 0u;
    off = (uint32_t)chunk->chunk_idx * RS485_FP_CHUNK_BYTES;
    memcpy(s_asm_buf + off, chunk->data, RS485_FP_CHUNK_BYTES);
    s_asm_chunk_mask |= (uint8_t)(1u << chunk->chunk_idx);

    if(chunk->chunk_idx == 0u && was_empty != 0u) {
        FP_MIRROR_LOG("[SLV][FP] tpl rx page=%u start", (unsigned)chunk->page_id);
    }

    if(s_asm_chunk_mask == (uint8_t)((1u << RS485_FP_CHUNK_COUNT) - 1u)) {
        s_asm_chunk_mask = 0u;
        FP_MIRROR_LOG("[SLV][FP] tpl rx complete page=%u (queued for FpTask)", (unsigned)s_asm_page);
        s_mirror_as608_hold = 1u;
        /* RS485 刚收完，给总线/中断一点时间再碰 AS608 */
        s_tpl_write_quiet_until_ms = HAL_GetTick() + 150u;
        /* 主机重传该页时重置失败计数，允许再次尝试写入 */
        if(s_apply_fail_page == s_asm_page) {
            s_apply_fail_streak = 0u;
            s_write_defer_streak = 0u;
        }
        if(fp_sync_q_push(s_asm_page, s_asm_buf) == 0u) {
            return 0u;
        }
        s_asm_page = 0xFFFFu;
    }
    return 1u;
}

void app_slave_fp_template_apply_pending(uint8_t *fp_hw_inited)
{
    uint16_t page;
    uint8_t en;

    if(s_apply_busy != 0u) {
        return;
    }
    if(fp_sync_q_has_pending() == 0u) {
        return;
    }
    if(app_slave_fp_quiet_active() != 0u) {
        return;
    }

    if(!fp_sync_q_pop(&page, s_apply_tpl)) {
        return;
    }

    FP_MIRROR_LOG("[SLV][FP] tpl write start page=%u", (unsigned)page);

    /* 重置连续失败计数（换页时重置） */
    if(s_apply_fail_page != page) {
        s_apply_fail_page = page;
        s_apply_fail_streak = 0u;
        s_write_defer_streak = 0u;
    }

    /* 写 AS608 期间再置 busy；握手阶段保持 0，避免 RS485 分片被拒 */
    s_apply_busy = 1u;
    FP_MIRROR_LOG("[SLV][FP] tpl write try page=%u", (unsigned)page);
    en = slave_fp_tpl_store(page, s_apply_tpl, fp_hw_inited);
    if(en == 0xFFu) {
        s_apply_busy = 0u;
        s_write_defer_streak++;
        if(s_log_defer_page != page) {
            s_log_defer_page = page;
            FP_MIRROR_LOG("[SLV][FP] tpl write defer page=%u (AS608 no ack) streak=%u",
                          (unsigned)page, (unsigned)s_write_defer_streak);
        }
        if(s_write_defer_streak >= 5u) {
            FP_MIRROR_LOG("[SLV][FP] tpl write GIVE UP page=%u (handshake fail)", (unsigned)page);
            app_rs485_slave_fp_commit_async(page, 0xFFu);
            s_write_defer_streak = 0u;
            s_mirror_as608_hold = 0u;
            return;
        }
        s_tpl_write_quiet_until_ms = HAL_GetTick() + APP_FP_TPL_WRITE_RETRY_QUIET_MS;
        s_mirror_as608_hold = 1u;
        (void)fp_sync_q_push(page, s_apply_tpl);
        return;
    }
    s_write_defer_streak = 0u;

    s_tpl_write_quiet_until_ms = HAL_GetTick() + APP_FP_TPL_WRITE_QUIET_MS;

    if(en == 0x00u) {
        s_apply_fail_streak = 0u;
        s_apply_fail_page = 0xFFFFu;
        s_tpl_write_ever_ok = 1u;
        s_log_fail_page = 0xFFFFu;
        s_log_defer_page = 0xFFFFu;
        FP_MIRROR_LOG("[SLV][FP] tpl write OK page=%u (slave unlock ready)", (unsigned)page);
        app_rs485_slave_fp_commit_async(page, 0u);
        if(fp_sync_q_has_pending() == 0u) {
            s_mirror_as608_hold = 0u;
            app_home_fp_poll_arm_immediate();
        }
    } else {
        s_apply_fail_streak++;
        if(s_log_fail_page != page) {
            s_log_fail_page = page;
            FP_MIRROR_LOG("[SLV][FP] tpl write FAIL page=%u ret=0x%02X (host will retransmit)",
                          (unsigned)page, (unsigned)en);
        }
        /* 仅通知主机一次失败，不再本地重试入队（避免 RS485 commit 洪水） */
        app_rs485_slave_fp_commit_async(page, en);
        s_apply_fail_page = 0xFFFFu;
        if(en == 0x18u) {
            s_tpl_write_quiet_until_ms = HAL_GetTick() + 1500u;
            s_mirror_session_emptied = 0u; /* 下次写入前再 Empty */
        }
        if(fp_sync_q_has_pending() == 0u) {
            s_mirror_as608_hold = 0u;
        }
    }
    s_apply_busy = 0u;
}

uint8_t app_slave_fp_mirror_hold(void)
{
    return s_mirror_as608_hold;
}

uint8_t app_slave_fp_write_ever_ok(void)
{
    return s_tpl_write_ever_ok;
}

uint8_t app_slave_fp_write_busy(void)
{
    return s_apply_busy;
}

uint8_t app_slave_fp_template_apply_now(uint8_t *fp_hw_inited)
{
    /* 仅 FpTask 写 AS608，避免 RS485 任务与指纹轮询抢 USART3 */
    (void)fp_hw_inited;
    return fp_sync_q_has_pending() ? 0u : 1u;
}

uint8_t app_slave_fp_queue_pending(void)
{
    return fp_sync_q_has_pending();
}

#endif /* APP_RS485_ENABLE && SLAVE */
