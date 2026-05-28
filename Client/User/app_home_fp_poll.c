#include "app_home_fp_poll.h"

#include <string.h>
#include <stdio.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "app_config.h"
#include "app_screen.h"
/* APP_HOME_POLL_INTERVAL_MS / APP_FP_* 见 app_config.h */
#include "app_unlock_event.h"
#include "app_user_ops.h"
#include "app_home_unlock.h"
#include "app_state.h"
#include "app_fp_mirror_diag.h"
#include "../Drivers/BSP/AS608/as608.h"
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
#include "app_slave_fp_sync.h"
#include "app_rs485_proto.h"
#include "app_rs485_link.h"
#include "app_rs485_fp_sync.h"
#endif

#define FP_LOG(fmt, ...) ((void)0)

/* AS608 模块状态机：避免在模块缺失/掉线时反复 200ms 阻塞主循环。
 *  UNKNOWN: 还未确认；ALIVE: 握手过；DEAD: 连续失败到达阈值，长时间退避。 */
typedef enum {
    AS608_STATE_UNKNOWN = 0,
    AS608_STATE_ALIVE   = 1,
    AS608_STATE_DEAD    = 2
} as608_state_t;

static as608_state_t s_as608_state = AS608_STATE_UNKNOWN;
static uint8_t       s_as608_fail_count = 0u;

/* 单次握手（不再 3 连发），最多阻塞 ~200ms（模块在线时 ~3ms 即返回）。 */
uint8_t app_fp_hw_try_handshake(uint8_t *fp_hw_inited)
{
    uint32_t addr = 0u;
    SysPara sp;

    if(!(*fp_hw_inited)) {
        GZ_StaGPIO_Init();
        *fp_hw_inited = 1u;
    }
    as608_rx_buffer_clear();
    if(GZ_HandShake(&addr) == 0u) {
        AS608Addr = addr;
        return 1u;
    }
    /* 从机常见：简化握手包无应答，但 ReadSysPara(0x0F) 正常；否则 RS485 模板无法写入 AS608 */
    as608_rx_buffer_clear();
    if(GZ_ReadSysPara(&sp) == 0u) {
        if(sp.GZ_addr != 0u) {
            AS608Addr = sp.GZ_addr;
        }
        return 1u;
    }
    return 0u;
}

uint8_t app_fp_hw_try_handshake_retries(uint8_t *fp_hw_inited, uint8_t attempts, uint16_t gap_ms)
{
    uint8_t i;

    if(fp_hw_inited == NULL || attempts == 0u) {
        return 0u;
    }
    for(i = 0u; i < attempts; i++) {
        if(app_fp_hw_try_handshake(fp_hw_inited) != 0u) {
            return 1u;
        }
        if((i + 1u) < attempts && gap_ms > 0u) {
            HAL_Delay(gap_ms);
        }
    }
    return 0u;
}

uint8_t app_fp_chip_tpl_count(uint16_t *n_out, uint8_t *fp_hw_inited)
{
    SysPara sp;
    uint16_t n = 0u;

    if(n_out == NULL || fp_hw_inited == NULL) {
        return 0u;
    }
    *n_out = 0u;
    if(!(*fp_hw_inited)) {
        GZ_StaGPIO_Init();
        *fp_hw_inited = 1u;
    }
    if(app_fp_hw_try_handshake_retries(fp_hw_inited, 2u, 40u) == 0u) {
        return 0u;
    }
    if(GZ_ValidTempleteNum(&n) != 0x00u) {
        return 1u;
    }
    memset(&sp, 0, sizeof(sp));
    if(GZ_ReadSysPara(&sp) == 0x00u && sp.GZ_max > 0u && n > sp.GZ_max) {
        n = sp.GZ_max;
    }
    if(n > 300u) {
        n = 0u;
    }
    *n_out = n;
    return 1u;
}

void app_home_fp_poll_handle(uint32_t *home_fp_last_poll_ms, uint8_t *fp_hw_inited)
{
    uint32_t now = HAL_GetTick();
    static uint32_t s_last_hs_try_ms = 0u;
    static uint32_t s_retry_after_ms = 0u;
    static uint32_t s_last_alive_log = 0u;
    static uint8_t s_finger_seen_last = 0u;
    static uint8_t s_finger_sta_cnt = 0u;
    static uint8_t s_grace_set = 0u;
    static uint32_t s_last_sta_probe_ms = 0u;
    uint8_t finger_now;
    uint8_t try_i;
    uint8_t en;
    SearchResult search = {0u, 0u};

#if APP_RS485_IS_SLAVE
    if(g_app_scr != APP_SCR_1) return;
    /* 只在 AS608 正在写入模板时屏蔽（UART 占用）；
       quiet 期间 AS608 已空闲，GetImage/Search 可以正常运行，
       不必等所有页全部传完才能开锁 */
    if(app_slave_fp_write_busy() != 0u) return;
    if(app_slave_fp_mirror_hold() != 0u) return;
    if(app_slave_fp_quiet_active() != 0u) return;
    if(app_slave_fp_queue_pending() != 0u) return;
    if(app_home_unlock_in_match_cooldown() != 0u) return;
    if(g_screen1_unlock_popup != NULL && lv_obj_is_valid(g_screen1_unlock_popup)) return;
#else
    if(g_app_scr != APP_SCR_HOME) return;
#endif
    if(app_home_unlock_popup_visible()) return;

    /* 短 grace（与 NFC 上电即可轮询接近）；避免长时间不能刷指纹。 */
    if(!s_grace_set) {
        s_grace_set = 1u;
        s_last_hs_try_ms = now;
        s_retry_after_ms = now + APP_FP_BOOT_GRACE_MS;
    }

    if((int32_t)(now - s_retry_after_ms) < 0) return;
    if((now - *home_fp_last_poll_ms) < APP_HOME_POLL_INTERVAL_MS) return;
    *home_fp_last_poll_ms = now;

    /* 每 5 秒一次心跳，证明指纹轮询在跑 */
    if((now - s_last_alive_log) >= 5000u) {
        s_last_alive_log = now;
        FP_LOG("poll alive sta=%d hs_inited=%u state=%u", (int)GZ_Sta, *fp_hw_inited, (unsigned)s_as608_state);
    }

    finger_now = (GZ_Sta == GPIO_PIN_SET) ? 1u : 0u;
    /* 手指落下/抬起的边沿都打日志，确认 STA 引脚真的响应了 */
    if(finger_now != s_finger_seen_last) {
        FP_LOG("finger %s", finger_now ? "DOWN" : "UP");
        s_finger_seen_last = finger_now;
    }
    if(!finger_now) {
        s_finger_sta_cnt = 0u;
        if((now - s_last_sta_probe_ms) < APP_FP_STA_PROBE_MS) {
            return;
        }
        s_last_sta_probe_ms = now;
        if((now - s_last_hs_try_ms) >= 2000u) {
            (void)app_fp_hw_try_handshake(fp_hw_inited);
            s_last_hs_try_ms = now;
        }
        return;
    }
    if(s_finger_sta_cnt < 255u) {
        s_finger_sta_cnt++;
    }
    if(s_finger_sta_cnt < (uint8_t)APP_FP_FINGER_STA_CONFIRM_CNT) {
        return;
    }

    /* 模块判定为 DEAD 时，长时间不再 try（s_retry_after_ms 已被压到很大）。
     * 但每 30s 还是给一次重试机会，避免永远不响应——这里直接由 retry_after 控制。 */

    if((now - s_last_hs_try_ms) >= 2000u) {
        if(!app_fp_hw_try_handshake(fp_hw_inited)) {
            s_as608_fail_count++;
            if(s_as608_fail_count >= 3u) {
                /* 连续 3 次失败基本可以判定模块缺失/严重掉线；30s 后再重试，
                 * 避免每 2 秒 200ms 阻塞主循环。 */
                s_as608_state = AS608_STATE_DEAD;
                s_retry_after_ms = now + 30000u;
                FP_LOG("handshake FAIL x%u -> backoff 30s (AS608 missing?)", s_as608_fail_count);
            } else {
                s_retry_after_ms = now + 2000u;
                FP_LOG("handshake FAIL x%u", s_as608_fail_count);
            }
            s_last_hs_try_ms = now;
            return;
        }
        if(s_as608_state != AS608_STATE_ALIVE) {
            FP_LOG("AS608 alive (addr=0x%08X)", (unsigned)AS608Addr);
        }
        s_as608_state = AS608_STATE_ALIVE;
        s_as608_fail_count = 0u;
        s_last_hs_try_ms = now;
    }

    en = GZ_GetImage();
    if(en == 0x02u) return;  /* 没拿到图（手指没贴稳）：静默返回，不算错 */
    if(en == 0xFFu) {
        FP_LOG("GetImage NO ACK (AS608 link fault)");
        s_retry_after_ms = now + 200u;
        return;
    }
    if(en != 0x00u) {
        FP_LOG("GetImage ret=0x%02X", en);
        return;
    }

    en = 0xFFu;
    for(try_i = 0u; try_i < 2u; try_i++) {
        en = GZ_GenChar(CharBuffer1);
        if(en == 0x00u) break;
        HAL_Delay(20);
    }
    if(en != 0x00u) {
        FP_LOG("GenChar1 ret=0x%02X", en);
        if(en == 0xFFu) s_retry_after_ms = now + 200u;
        return;
    }

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE && (APP_FP_SLAVE_MATCH_VIA_HOST != 0)
    {
        uint8_t feat[AS608_TEMPLATE_SIZE];
        rs485_fp_match_rsp_t mrsp;

        as608_rx_buffer_clear();
        HAL_Delay(15);
        en = GZ_UpCharBuffer(CharBuffer1, feat, AS608_TEMPLATE_SIZE);
        if(en != 0x00u) {
            FP_MIRROR_LOG("[SLV][FP] UpChar for host match FAIL ret=0x%02X", (unsigned)en);
            return;
        }
        if(app_rs485_link_ready() == 0u) {
            FP_MIRROR_LOG("[SLV][FP] host match skip (RS485 not ready)");
#if (APP_FP_SLAVE_MATCH_FALLBACK != 0)
            goto slave_fp_local_search;
#else
            return;
#endif
        }
        if(app_rs485_slave_fp_match_request(feat, &mrsp, APP_FP_SLAVE_MATCH_RS485_MS) == false) {
            FP_MIRROR_LOG("[SLV][FP] host match RS485 FAIL");
#if (APP_FP_SLAVE_MATCH_FALLBACK != 0)
            goto slave_fp_local_search;
#else
            return;
#endif
        }
        if(mrsp.result == RS485_FP_MATCH_RESULT_OK && mrsp.acc[0] != '\0' &&
           mrsp.score >= (uint16_t)APP_FP_MATCH_SCORE_MIN) {
            FP_MIRROR_LOG("[SLV][FP] host match OK page=%u acc=%s score=%u",
                          (unsigned)mrsp.page_id, mrsp.acc, (unsigned)mrsp.score);
            app_home_unlock_arm_match_cooldown();
            app_unlock_event_handle_success(APP_UNLOCK_POPUP_SCREEN1, mrsp.acc, "fingerprint");
            return;
        }
        if(mrsp.result == RS485_FP_MATCH_RESULT_BUSY) {
            FP_MIRROR_LOG("[SLV][FP] host match BUSY (mirror/enroll)");
        } else if(mrsp.result == RS485_FP_MATCH_RESULT_NO_MATCH) {
            FP_MIRROR_LOG("[SLV][FP] host match NO_MATCH");
        } else {
            FP_MIRROR_LOG("[SLV][FP] host match FAIL result=0x%02X", (unsigned)mrsp.result);
        }
#if (APP_FP_SLAVE_MATCH_FALLBACK == 0)
        return;
#endif
    }
#if (APP_FP_SLAVE_MATCH_FALLBACK != 0)
slave_fp_local_search:
#endif
#endif /* slave match via host */

    en = 0xFFu;
    for(try_i = 0u; try_i < 2u; try_i++) {
        en = GZ_SearchLibrary(CharBuffer1, &search);
        if(en == 0x00u) break;
        HAL_Delay(20);
    }
    if(en != 0x00u) {
        static uint32_t s_last_srch_fail_log_ms;
        static uint8_t s_last_srch_fail_code;
        uint16_t tpl_n = 0u;

        (void)app_fp_chip_tpl_count(&tpl_n, fp_hw_inited);
        if(en != s_last_srch_fail_code ||
           (now - s_last_srch_fail_log_ms) >= 1000u) {
            s_last_srch_fail_code = en;
            s_last_srch_fail_log_ms = now;
            FP_MIRROR_LOG("[SLV][FP] local Search FAIL ret=0x%02X tpl=%u", en, (unsigned)tpl_n);
        }
        if(en == 0xFFu) s_retry_after_ms = now + 200u;
        return;
    }

    if(search.mathscore < (uint16_t)APP_FP_MATCH_SCORE_MIN) {
        FP_MIRROR_LOG("[SLV][FP] local Search page=%u low score=%u",
                      (unsigned)search.pageID, (unsigned)search.mathscore);
        return;
    }
    if(users_match_fp_page(search.pageID) != 0u) {
        const char *acc = users_get_acc_by_fp_page(search.pageID);
        if(app_slave_fp_write_ever_ok() == 0u) {
            FP_MIRROR_LOG("[SLV][FP] local match page=%u ignored (no RS485 tpl yet)",
                          (unsigned)search.pageID);
            return;
        }
        app_home_unlock_arm_match_cooldown();
        app_unlock_event_handle_success(APP_UNLOCK_POPUP_SCREEN1, acc, "fingerprint");
    }
}
