#include "app_fp_mirror_tx.h"

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER

#include <string.h>
#include "app_rs485_fp_sync.h"
#include "app_rs485_link.h"
#include "app_rs485_proto.h"
#include "app_user_ops.h"
#include "app_state.h"

extern uint8_t g_default_admin_deleted;
extern uint8_t g_default_admin_has_fp;
extern uint16_t g_default_admin_fp_page_id_1;
extern uint16_t g_default_admin_fp_page_id_2;
extern char g_default_admin_account[13];
#include "../Drivers/BSP/AS608/as608.h"
#include "app_fp_mirror_diag.h"
#include "app_home_fp_poll.h"
#include "app_config.h"
#include "app_state.h"
#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

#define FP_MIRROR_Q_SZ 12u

typedef enum {
    FP_TX_IDLE = 0,
    FP_TX_READ,
    FP_TX_SEND_CHUNK,
    FP_TX_RS485_BACKOFF
} fp_tx_state_t;

static uint16_t s_fp_q[FP_MIRROR_Q_SZ];
static uint8_t s_fp_q_cnt;
static fp_tx_state_t s_fp_tx_state;
static uint8_t s_fp_mirror_ping_done;
static uint16_t s_fp_tx_page;
static uint8_t s_fp_tx_tpl[AS608_TEMPLATE_SIZE];
static uint8_t s_fp_tx_chunk;
static uint8_t s_fp_tpl_ready;
static uint32_t s_fp_rs485_backoff_until_ms;
static uint32_t s_fp_as608_backoff_until_ms;
static uint32_t s_fp_next_page_after_ms;
static uint32_t s_fp_page_done_ms[300];
static uint8_t s_fp_wait_commit;
static uint8_t s_fp_rs485_fail_streak;
static uint32_t s_fp_wait_commit_log_ms;
static uint32_t s_fp_wait_commit_since_ms;
static uint8_t s_mirror_hold_unlock = 0u;

#ifndef APP_FP_MIRROR_RS485_BACKOFF_MS
#define APP_FP_MIRROR_RS485_BACKOFF_MS 2000u
#endif

#define FP_PRIME_SLOTS 4u
static struct {
    uint16_t page;
    uint8_t valid;
    uint8_t tpl[AS608_TEMPLATE_SIZE];
} s_fp_prime[FP_PRIME_SLOTS];

extern uint8_t g_fp_hw_inited;

static uint8_t fp_mirror_take_prime(uint16_t page_id, uint8_t *tpl_out)
{
    uint8_t i;

    if(tpl_out == NULL) {
        return 0u;
    }
    for(i = 0u; i < FP_PRIME_SLOTS; i++) {
        if(s_fp_prime[i].valid != 0u && s_fp_prime[i].page == page_id) {
            memcpy(tpl_out, s_fp_prime[i].tpl, AS608_TEMPLATE_SIZE);
            return 1u;
        }
    }
    return 0u;
}

/* 该 page 是否可同步：仅用户调度到的 page；prime | Flash缓存 | AS608能读出（不按 chip 总数扫库） */
static uint8_t fp_mirror_page_has_tpl(uint16_t page)
{
    uint8_t i;
    uint8_t probe[AS608_TEMPLATE_SIZE];

    if(page >= 300u || page == 0xFFFFu) {
        return 0u;
    }
    for(i = 0u; i < FP_PRIME_SLOTS; i++) {
        if(s_fp_prime[i].valid != 0u && s_fp_prime[i].page == page) {
            return 1u;
        }
    }
    if(users_fp_template_cache_valid(page)) {
        return 1u;
    }
    if(!g_fp_hw_inited) {
        GZ_StaGPIO_Init();
        g_fp_hw_inited = 1u;
    }
    if(app_fp_hw_try_handshake_retries(&g_fp_hw_inited, 2u, 40u) == 0u) {
        return 0u;
    }
    as608_rx_buffer_clear();
    HAL_Delay(20);
    return (GZ_ReadTemplatePage(page, probe, AS608_TEMPLATE_SIZE) == 0x00u) ? 1u : 0u;
}

/* 读取待同步模板（曾成功顺序）：prime > AS608读 > Flash缓存(仅 chip_n>0)。 */
static uint8_t fp_mirror_read_tpl(uint16_t page, uint8_t *tpl_out)
{
    uint8_t try_i;
    uint8_t en = 0xFFu;
    uint16_t chip_n = 0u;

    if(tpl_out == NULL || page >= 300u) {
        return 0xFFu;
    }

    if(fp_mirror_take_prime(page, tpl_out) != 0u) {
        FP_MIRROR_LOG("[HOST][FP] tpl prime page=%u", (unsigned)page);
        return 0u;
    }

    if(app_fp_hw_try_handshake_retries(&g_fp_hw_inited, 3u, 60u) != 0u) {
        for(try_i = 0u; try_i < 3u; try_i++) {
            as608_rx_buffer_clear();
            HAL_Delay(40);
            en = GZ_ReadTemplatePage(page, tpl_out, AS608_TEMPLATE_SIZE);
            if(en == 0x00u) {
                (void)users_fp_template_cache_save(page, tpl_out, AS608_TEMPLATE_SIZE);
                FP_MIRROR_LOG("[HOST][FP] tpl AS608 OK page=%u", (unsigned)page);
                return 0u;
            }
            HAL_Delay(30);
        }
        if(users_fp_template_cache_load(page, tpl_out, AS608_TEMPLATE_SIZE)) {
            (void)app_fp_chip_tpl_count(&chip_n, &g_fp_hw_inited);
            FP_MIRROR_LOG("[HOST][FP] tpl cache OK page=%u -> RS485 x4 (chip_tpl=%u)",
                          (unsigned)page, (unsigned)chip_n);
            return 0u;
        }
        FP_MIRROR_LOG("[HOST][FP] tpl AS608 read FAIL page=%u ret=0x%02X",
                      (unsigned)page, (unsigned)en);
    }

    return (en != 0xFFu) ? en : 0xFFu;
}

void app_fp_mirror_tx_prime_page(uint16_t page_id, const uint8_t *tpl)
{
    uint8_t i;

    if(page_id >= 300u || tpl == NULL) {
        return;
    }
    for(i = 0u; i < FP_PRIME_SLOTS; i++) {
        if(s_fp_prime[i].valid != 0u && s_fp_prime[i].page == page_id) {
            memcpy(s_fp_prime[i].tpl, tpl, AS608_TEMPLATE_SIZE);
            return;
        }
    }
    for(i = 0u; i < FP_PRIME_SLOTS; i++) {
        if(s_fp_prime[i].valid == 0u) {
            s_fp_prime[i].page = page_id;
            memcpy(s_fp_prime[i].tpl, tpl, AS608_TEMPLATE_SIZE);
            s_fp_prime[i].valid = 1u;
            if(page_id < 300u) {
                s_fp_page_done_ms[page_id] = 0u; /* 新录入：允许立即同步 */
            }
            FP_MIRROR_LOG("[HOST][FP] prime page=%u (skip flash read)", (unsigned)page_id);
            return;
        }
    }
}

void app_fp_mirror_tx_schedule_page_force(uint16_t page_id)
{
    if(page_id >= 300u || page_id == 0xFFFFu) {
        return;
    }
    if(page_id < 300u) {
        s_fp_page_done_ms[page_id] = 0u;
    }
    app_fp_mirror_tx_schedule_page(page_id);
}

static void fp_mirror_rs485_backoff(uint16_t page)
{
    FP_MIRROR_LOG("[HOST][FP] rs485 backoff page=%u streak=%u next=%lums",
                  (unsigned)page, (unsigned)(s_fp_rs485_fail_streak + 1u),
                  (unsigned long)APP_FP_MIRROR_RS485_BACKOFF_MS);
    s_fp_tx_page = page;
    s_fp_mirror_ping_done = 0u;
    s_fp_tx_chunk = 0u;
    s_fp_rs485_fail_streak++;
    s_fp_rs485_backoff_until_ms = HAL_GetTick() + APP_FP_MIRROR_RS485_BACKOFF_MS;
    s_fp_wait_commit = 0u;
    s_fp_tx_state = FP_TX_RS485_BACKOFF;
}

static void fp_mirror_enqueue_page(uint16_t page_id)
{
    uint8_t i;

    if(page_id >= 300u || page_id == 0xFFFFu) {
        return;
    }
    for(i = 0u; i < s_fp_q_cnt; i++) {
        if(s_fp_q[i] == page_id) {
            return;
        }
    }
    if(s_fp_q_cnt >= FP_MIRROR_Q_SZ) {
        return;
    }
    s_fp_q[s_fp_q_cnt++] = page_id;
    FP_MIRROR_LOG("[HOST][FP] schedule page=%u q=%u", (unsigned)page_id, (unsigned)s_fp_q_cnt);
}

void app_fp_mirror_tx_schedule_page(uint16_t page_id)
{
    if(page_id >= 300u || page_id == 0xFFFFu) {
        return;
    }
    if(fp_mirror_page_has_tpl(page_id) == 0u) {
        uint16_t chip_n = 0u;
        (void)app_fp_chip_tpl_count(&chip_n, &g_fp_hw_inited);
        FP_MIRROR_LOG("[HOST][FP] skip page=%u (chip_tpl=%u cache=%u, enroll on HOST)",
                      (unsigned)page_id, (unsigned)chip_n,
                      users_fp_template_cache_valid(page_id) ? 1u : 0u);
        return;
    }
    if(s_fp_page_done_ms[page_id] != 0u &&
       (HAL_GetTick() - s_fp_page_done_ms[page_id]) < 300000u) {
        return;
    }
    if(s_fp_tx_state == FP_TX_IDLE) {
        fp_mirror_enqueue_page(page_id);
    } else if(s_fp_tx_page != page_id) {
        fp_mirror_enqueue_page(page_id);
    }
}

void app_fp_mirror_tx_hold_for_unlock(uint8_t hold)
{
    s_mirror_hold_unlock = (hold != 0u) ? 1u : 0u;
}

uint8_t app_fp_mirror_tx_busy(void)
{
    if(s_mirror_hold_unlock != 0u) {
        return 0u;
    }
    if(s_fp_tx_state != FP_TX_IDLE || s_fp_q_cnt != 0u) {
        return 1u;
    }
    if(s_fp_next_page_after_ms != 0u &&
       (int32_t)(HAL_GetTick() - s_fp_next_page_after_ms) < 0) {
        return 1u;
    }
    return 0u;
}

void app_fp_mirror_tx_schedule_acc(const char *acc)
{
    int idx;

    if(acc == NULL || acc[0] == '\0') {
        return;
    }

    idx = users_find_index_by_acc(acc);
    if(idx >= 0) {
        if(!g_users[idx].active) {
            return;
        }
        if(g_users[idx].has_fp == 0u) {
            return;
        }
        app_fp_mirror_tx_schedule_page(g_users[idx].fp_page_id_1);
        if(g_users[idx].fp_page_id_2 != g_users[idx].fp_page_id_1) {
            app_fp_mirror_tx_schedule_page(g_users[idx].fp_page_id_2);
        }
        return;
    }

    if(!g_default_admin_deleted &&
       strcmp(acc, g_default_admin_account) == 0 &&
       g_default_admin_has_fp != 0u) {
        app_fp_mirror_tx_schedule_page(g_default_admin_fp_page_id_1);
        if(g_default_admin_fp_page_id_2 != g_default_admin_fp_page_id_1) {
            app_fp_mirror_tx_schedule_page(g_default_admin_fp_page_id_2);
        }
    }
}

void app_fp_mirror_tx_tick(void)
{
    rs485_fp_tpl_chunk_t pl;
    uint16_t page;

    if(s_mirror_hold_unlock != 0u) {
        return;
    }

    /* 录入界面/过程中禁止 RS485 镜像：与 FpTask 抢主机 AS608，且会打断从机写模板 */
    if(g_screen8_fp_enroll_state == 1u || g_app_scr == APP_SCR_10) {
        if(s_fp_tx_state != FP_TX_IDLE || s_fp_wait_commit != 0u) {
            FP_MIRROR_LOG("[HOST][FP] pause mirror (enroll on screen10)");
            s_fp_wait_commit = 0u;
            s_fp_wait_commit_log_ms = 0u;
            s_fp_tx_state = FP_TX_IDLE;
        }
        return;
    }

    if(app_rs485_link_ready() == 0u) {
        return;
    }

    if(s_fp_tx_state == FP_TX_IDLE &&
       s_fp_next_page_after_ms != 0u &&
       (int32_t)(HAL_GetTick() - s_fp_next_page_after_ms) < 0) {
        return;
    }

    if(s_fp_as608_backoff_until_ms != 0u &&
       (int32_t)(HAL_GetTick() - s_fp_as608_backoff_until_ms) < 0) {
        return;
    }

    if(s_fp_tx_state == FP_TX_RS485_BACKOFF) {
        if((int32_t)(HAL_GetTick() - s_fp_rs485_backoff_until_ms) < 0) {
            return;
        }
        s_fp_tx_state = FP_TX_SEND_CHUNK;
    }

    if(s_fp_tx_state == FP_TX_IDLE) {
        if(s_fp_q_cnt == 0u) {
            return;
        }
        s_fp_tx_page = s_fp_q[0u];
        memmove(s_fp_q, &s_fp_q[1u], (size_t)(s_fp_q_cnt - 1u) * sizeof(s_fp_q[0u]));
        s_fp_q_cnt--;
        s_fp_mirror_ping_done = 0u;
        s_fp_tpl_ready = 0u;
        s_fp_wait_commit = 0u;
        s_fp_wait_commit_log_ms = 0u;
        s_fp_wait_commit_since_ms = 0u;
        app_rs485_master_clear_fp_commit();
        s_fp_rs485_fail_streak = 0u;
        FP_MIRROR_LOG("[HOST][FP] start page=%u q_left=%u", (unsigned)s_fp_tx_page, (unsigned)s_fp_q_cnt);
        s_fp_tx_state = FP_TX_READ;
    }

    if(s_fp_tx_state == FP_TX_READ) {
        uint8_t rd_en;
        uint16_t page_now = s_fp_tx_page;

        if(!g_fp_hw_inited) {
            GZ_StaGPIO_Init();
            g_fp_hw_inited = 1u;
        }

        rd_en = fp_mirror_read_tpl(s_fp_tx_page, s_fp_tx_tpl);
        if(rd_en != 0x00u) {
            if(rd_en == 0xFFu) {
                static uint32_t s_last_as608_off_log_ms;

                s_fp_as608_backoff_until_ms = HAL_GetTick() + 5000u;
                if((HAL_GetTick() - s_last_as608_off_log_ms) >= 30000u) {
                    s_last_as608_off_log_ms = HAL_GetTick();
                    FP_MIRROR_LOG("[HOST][FP] no tpl source page=%u (AS608 offline rx=%u)",
                                  (unsigned)page_now, (unsigned)as608_rx_widx_get());
                }
            } else {
                if(page_now < 300u) {
                    s_fp_page_done_ms[page_now] = HAL_GetTick();
                }
                FP_MIRROR_LOG("[HOST][FP] page=%u no data to send (host enroll first)", (unsigned)page_now);
            }
            s_fp_tx_state = FP_TX_IDLE;
            return;
        }
        FP_MIRROR_LOG("[HOST][FP] read tpl OK page=%u -> RS485 x4", (unsigned)s_fp_tx_page);
        s_fp_tpl_ready = 1u;
        s_fp_tx_chunk = 0u;
        s_fp_tx_state = FP_TX_SEND_CHUNK;
    }

    if(s_fp_tx_state == FP_TX_SEND_CHUNK) {
        page = s_fp_tx_page;
        if(s_fp_wait_commit != 0u) {
            uint16_t cm_page = 0xFFFFu;
            uint8_t cm_status = 0xFFu;
            uint32_t now_ms = HAL_GetTick();

            if((int32_t)(now_ms - s_fp_rs485_backoff_until_ms) >= 0) {
                FP_MIRROR_LOG("[HOST][FP] commit TIMEOUT page=%u (retry)", (unsigned)page);
                fp_mirror_rs485_backoff(page);
                return;
            }
            if(s_fp_wait_commit_log_ms == 0u || (now_ms - s_fp_wait_commit_log_ms) >= 1000u) {
                s_fp_wait_commit_log_ms = now_ms;
                FP_MIRROR_LOG("[HOST][FP] wait commit page=%u left=%lums",
                              (unsigned)page,
                              (unsigned long)(s_fp_rs485_backoff_until_ms - now_ms));
            }
            app_rs485_master_poll_incoming(20u);
            if(app_rs485_master_take_fp_commit(&cm_page, &cm_status) != 0u) {
                if(cm_page == page && cm_status == 0u) {
                    if(s_fp_wait_commit_since_ms != 0u &&
                       (now_ms - s_fp_wait_commit_since_ms) < APP_FP_MIRROR_COMMIT_MIN_OK_MS) {
                        FP_MIRROR_LOG("[HOST][FP] ignore fast commit page=%u (wait slave write)",
                                      (unsigned)page);
                        return;
                    }
                    FP_MIRROR_LOG("[HOST][FP] tx tpl COMMIT page=%u", (unsigned)page);
                    if(page < 300u) {
                        s_fp_page_done_ms[page] = HAL_GetTick();
                    }
                    s_fp_tpl_ready = 0u;
                    s_fp_wait_commit = 0u;
                    s_fp_wait_commit_log_ms = 0u;
                    s_fp_tx_state = FP_TX_IDLE;
                    s_fp_next_page_after_ms = HAL_GetTick() + APP_FP_MIRROR_PAGE_GAP_MS;
                    return;
                }
                if(cm_page == page && cm_status == 2u) {
                    FP_MIRROR_LOG("[HOST][FP] commit BUSY page=%u (slave AS608 busy, retry)",
                                  (unsigned)page);
                    fp_mirror_rs485_backoff(page);
                } else if(cm_page == page && cm_status != 0u) {
                    /* 从机 AS608 写入失败：重新读模板后再发；0x18 时多等一会让从机 Flash 恢复 */
                    FP_MIRROR_LOG("[HOST][FP] slave write FAIL page=%u status=0x%02X, re-read & retry",
                                  (unsigned)page, (unsigned)cm_status);
                    s_fp_tpl_ready = 0u;
                    s_fp_wait_commit = 0u;
                    s_fp_wait_commit_log_ms = 0u;
                    s_fp_tx_chunk = 0u;
                    s_fp_rs485_fail_streak = 0u;
                    if(cm_status == 0x18u) {
                        s_fp_rs485_backoff_until_ms = HAL_GetTick() + 3500u;
                    } else {
                        s_fp_rs485_backoff_until_ms = HAL_GetTick() + APP_FP_MIRROR_RS485_BACKOFF_MS;
                    }
                    s_fp_tx_state = FP_TX_READ;
                    return;
                } else if(cm_page != page) {
                    /* 其它页的失败/残留 commit（如上一页 0x18 重试），勿触发 backoff */
                    FP_MIRROR_LOG("[HOST][FP] stale commit page=%u status=0x%02X, keep waiting page=%u",
                                  (unsigned)cm_page, (unsigned)cm_status, (unsigned)page);
                    return;
                } else {
                    FP_MIRROR_LOG("[HOST][FP] commit unexpected page=%u status=0x%02X (retry)",
                                  (unsigned)page, (unsigned)cm_status);
                    fp_mirror_rs485_backoff(page);
                }
                return;
            }
            return;
        }
        /* 不再依赖 PING：从机已 ACK 但主机曾漏收；直接发 0x05 模板分片 */
        s_fp_mirror_ping_done = 1u;
        memset(&pl, 0, sizeof(pl));
        pl.page_id = page;
        pl.chunk_idx = s_fp_tx_chunk;
        pl.chunk_count = RS485_FP_CHUNK_COUNT;
        memcpy(pl.data, s_fp_tx_tpl + ((uint32_t)s_fp_tx_chunk * RS485_FP_CHUNK_BYTES),
               RS485_FP_CHUNK_BYTES);
        FP_MIRROR_LOG("[HOST][FP] tx chunk page=%u idx=%u/%u",
                      (unsigned)page, (unsigned)s_fp_tx_chunk, (unsigned)(RS485_FP_CHUNK_COUNT - 1u));

        if(!app_rs485_proto_slave_fp_template_chunk(&pl, 1200u)) {
            FP_MIRROR_LOG("[HOST][FP] tx chunk FAIL page=%u idx=%u (RS485 no rsp?)",
                          (unsigned)page, (unsigned)s_fp_tx_chunk);
            fp_mirror_rs485_backoff(page);
            return;
        }

        s_fp_tx_chunk++;
        if(s_fp_tx_chunk >= RS485_FP_CHUNK_COUNT) {
            FP_MIRROR_LOG("[HOST][FP] tx tpl SENT page=%u wait commit", (unsigned)page);
            app_rs485_master_clear_fp_commit();
            s_fp_wait_commit = 1u;
            s_fp_wait_commit_since_ms = HAL_GetTick();
            s_fp_wait_commit_log_ms = s_fp_wait_commit_since_ms;
            /* Slave AS608 write can take several seconds (handshake + data + StoreChar).
             * Keep commit timeout generous to avoid duplicate re-send while slave is still writing. */
            s_fp_rs485_backoff_until_ms = HAL_GetTick() + APP_FP_MIRROR_COMMIT_TOUT_MS;
        } else {
#if (APP_USE_FREERTOS == 1)
            vTaskDelay(pdMS_TO_TICKS(APP_RS485_MIRROR_INTER_CMD_MS));
#endif
            /* 发分片间隙顺便轮询一次 incoming，
               让从机的 commit 通知能及时被 ACK，避免从机反复重试 */
            app_rs485_master_poll_incoming(5u);
        }
    }
}

#endif /* APP_RS485_ENABLE && MASTER */
