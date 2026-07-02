#include "app_home_fp_poll.h"

#include <string.h>
#include <stdio.h>

#include "lvgl.h"
#if (APP_LEGACY_UI_ENABLE != 0)
#include "gui_guider.h"
#endif
#include "app_screen.h"
#include "app_unlock_event.h"
#include "app_user_ops.h"
#include "app_home_unlock.h"
#include "app_config.h"
#include "app_state.h"
#include "app_fp_unlock_log.h"
#include "../Drivers/BSP/AS608/as608.h"
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
#include "app_fp_mirror_tx.h"
#endif
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
#include "app_slave_fp_sync.h"
#include "app_rs485_link.h"
#include "app_rs485_proto.h"
#include "app_rs485_fp_sync.h"
#endif

#if (APP_LEGACY_UI_ENABLE != 0)
extern lv_ui guider_ui;
#endif
extern volatile app_scr_t g_app_scr;
extern uint8_t g_screen8_fp_enroll_state;

typedef enum {
    AS608_STATE_UNKNOWN = 0,
    AS608_STATE_ALIVE   = 1,
    AS608_STATE_DEAD    = 2
} as608_state_t;

static as608_state_t s_as608_state = AS608_STATE_UNKNOWN;
static uint8_t       s_as608_fail_count = 0u;

static const char *fp_search_ret_text(uint8_t en)
{
    switch(en) {
    case 0x09u: return "no match in library";
    case 0xFFu: return "no ACK from module";
    default:    return "search error";
    }
}

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

static uint8_t fp_poll_try_unlock(uint16_t page_id, uint16_t score, uint8_t *fp_hw_inited)
{
    const char *acc;

    (void)fp_hw_inited;
    if(users_match_fp_page(page_id) == 0u) {
        FP_ULOG("[FP] match page=%u score=%u but NOT bound to user -> deny",
                (unsigned)page_id, (unsigned)score);
        return 0u;
    }
#if APP_RS485_IS_SLAVE
    if(app_slave_fp_write_ever_ok() == 0u) {
        FP_ULOG("[FP] match page=%u but slave has no template yet -> deny", (unsigned)page_id);
        return 0u;
    }
    acc = users_get_acc_by_fp_page(page_id);
    FP_ULOG("[FP] UNLOCK OK account=%s page=%u score=%u", acc, (unsigned)page_id, (unsigned)score);
    app_home_unlock_arm_match_cooldown();
    app_unlock_event_handle_success(APP_UNLOCK_POPUP_SCREEN1, acc, "fingerprint");
#else
    acc = users_get_acc_by_fp_page(page_id);
    FP_ULOG("[FP] UNLOCK OK account=%s page=%u score=%u", acc, (unsigned)page_id, (unsigned)score);
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
    app_fp_mirror_tx_hold_for_unlock(0u);
#endif
    app_home_unlock_arm_match_cooldown();
    app_unlock_event_handle_success(APP_UNLOCK_POPUP_HOME, acc, "fingerprint");
#endif
    return 1u;
}

void app_home_fp_poll_handle(uint32_t *home_fp_last_poll_ms, uint8_t *fp_hw_inited)
{
    uint32_t now = HAL_GetTick();
    static uint32_t s_last_hs_try_ms = 0u;
    static uint32_t s_retry_after_ms = 0u;
    static uint8_t s_finger_seen_last = 0u;
    static uint8_t s_finger_sta_cnt = 0u;
    static uint8_t s_grace_set = 0u;
    static uint32_t s_last_sta_probe_ms = 0u;
    static uint8_t s_verify_logged = 0u;
    uint8_t finger_now;
    uint8_t try_i;
    uint8_t en;
    SearchResult search = {0u, 0u};

    if(g_app_scr != APP_SCR_HOME) return;
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
    if(g_screen8_fp_enroll_state == 1u || g_app_scr == APP_SCR_6 ||
       g_app_scr == APP_SCR_8 || g_app_scr == APP_SCR_10) {
        return;
    }
#endif
    if(app_home_unlock_popup_visible()) return;
    if(app_home_unlock_in_match_cooldown() != 0u) {
        if((GZ_Sta == GPIO_PIN_SET) && s_verify_logged == 0u) {
            FP_ULOG("[FP] skip: unlock cooldown");
        }
        return;
    }

    if(!s_grace_set) {
        s_grace_set = 1u;
        s_last_hs_try_ms = 0u;
        s_retry_after_ms = now + APP_FP_BOOT_GRACE_MS;
    }

    if((int32_t)(now - s_retry_after_ms) < 0) return;
    if((now - *home_fp_last_poll_ms) < APP_HOME_POLL_INTERVAL_MS) return;
    *home_fp_last_poll_ms = now;

    finger_now = (GZ_Sta == GPIO_PIN_SET) ? 1u : 0u;
    if(finger_now != s_finger_seen_last) {
        FP_ULOG("[FP] finger %s", finger_now ? "ON (detect pin)" : "OFF");
        s_finger_seen_last = finger_now;
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
        app_fp_mirror_tx_hold_for_unlock(finger_now);
#endif
        if(finger_now == 0u) {
            if(s_verify_logged != 0u) {
                FP_ULOG("[FP] tip: hold finger until you see capture/search logs");
            }
            s_verify_logged = 0u;
        }
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

    if(s_verify_logged == 0u) {
        FP_ULOG("[FP] start verify (finger stable)");
        s_verify_logged = 1u;
    }

    if(s_as608_state != AS608_STATE_ALIVE ||
       (now - s_last_hs_try_ms) >= 2000u) {
        if(!app_fp_hw_try_handshake(fp_hw_inited)) {
            s_as608_fail_count++;
            if(s_as608_fail_count >= 3u) {
                s_as608_state = AS608_STATE_DEAD;
                s_retry_after_ms = now + 30000u;
                FP_ULOG("[FP] module no reply x%u -> wait 30s", s_as608_fail_count);
            } else {
                s_retry_after_ms = now + 2000u;
                FP_ULOG("[FP] module no reply x%u (USART3 PB10/PB11 57600?)", s_as608_fail_count);
            }
            s_last_hs_try_ms = now;
            return;
        }
        if(s_as608_state != AS608_STATE_ALIVE) {
            FP_ULOG("[FP] module online addr=0x%08X", (unsigned)AS608Addr);
        }
        s_as608_state = AS608_STATE_ALIVE;
        s_as608_fail_count = 0u;
        s_last_hs_try_ms = now;
    }

    en = GZ_GetImage();
    if(en == 0x02u) {
        return;
    }
    if(en == 0xFFu) {
        FP_ULOG("[FP] capture image FAIL: no ACK");
        s_retry_after_ms = now + 200u;
        return;
    }
    if(en != 0x00u) {
        FP_ULOG("[FP] capture image FAIL: code=0x%02X", en);
        return;
    }
    FP_ULOG("[FP] capture image OK");

    en = 0xFFu;
    for(try_i = 0u; try_i < 2u; try_i++) {
        en = GZ_GenChar(CharBuffer1);
        if(en == 0x00u) break;
        HAL_Delay(20);
    }
    if(en != 0x00u) {
        FP_ULOG("[FP] extract feature FAIL: code=0x%02X%s",
                en, (en == 0xFFu) ? " (no ACK)" : "");
        if(en == 0xFFu) s_retry_after_ms = now + 200u;
        return;
    }
    FP_ULOG("[FP] extract feature OK");

    en = 0xFFu;
    for(try_i = 0u; try_i < 2u; try_i++) {
        en = GZ_SearchLibrary(CharBuffer1, &search);
        if(en == 0x00u) break;
        HAL_Delay(20);
    }
    if(en != 0x00u) {
        FP_ULOG("[FP] search library FAIL: code=0x%02X (%s)",
                en, fp_search_ret_text(en));
        if(en == 0xFFu) s_retry_after_ms = now + 200u;
        return;
    }
    FP_ULOG("[FP] search library OK: page=%u score=%u",
            (unsigned)search.pageID, (unsigned)search.mathscore);

    if(fp_poll_try_unlock(search.pageID, search.mathscore, fp_hw_inited) != 0u) {
        return;
    }

    en = GZ_GenChar(CharBuffer2);
    if(en != 0x00u) {
        FP_ULOG("[FP] buffer2 feature FAIL: code=0x%02X", en);
        return;
    }
    en = GZ_SearchLibrary(CharBuffer2, &search);
    if(en != 0x00u) {
        FP_ULOG("[FP] buffer2 search FAIL: code=0x%02X (%s)",
                en, fp_search_ret_text(en));
        return;
    }
    FP_ULOG("[FP] buffer2 search OK: page=%u score=%u",
            (unsigned)search.pageID, (unsigned)search.mathscore);
    (void)fp_poll_try_unlock(search.pageID, search.mathscore, fp_hw_inited);
}
