#include "app_home_unlock.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "app_config.h"
#include "app_screen.h"
#include "app_unlock_event.h"
#include "app_user_ops.h"
#include "app_nfc_hw.h"
#include "app_state.h"
#include "user_auth_portable.h"
#include "../Drivers/BSP/mfrc522/door.h"
#include "../Drivers/BSP/mfrc522/MFRC522.h"
#include "app_slave_diag.h"

#if (APP_SLAVE_USART1_DEBUG != 0)
#define APP_UNLOCK_LOG(...) SLAVE_DBG_LOG("[SLV][NFC] " __VA_ARGS__)
#elif (APP_SLAVE_NFC_UNLOCK_TRACE != 0)
#define APP_UNLOCK_LOG(...) SLAVE_NFC_LOG(__VA_ARGS__)
#else
#define APP_UNLOCK_LOG(...) ((void)0)
#endif

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20);

extern lv_ui guider_ui;
extern app_scr_t g_app_scr;
extern user_cred_t g_users[APP_USER_MAX];
extern uint8_t g_user_count;
extern uint8_t g_default_admin_deleted;
extern uint8_t g_default_admin_has_nfc;
extern uint8_t g_default_admin_nfc_uid[4];
extern char g_default_admin_account[13];

#if APP_RS485_IS_SLAVE
static int app_nfc_match_user(const uint8_t uid[4], const char **acc_out)
{
    int idx;
    uint8_t uid_rev[4];

    if(acc_out != NULL) {
        *acc_out = "";
    }
    idx = users_find_index_by_nfc_uid(uid);
    if(idx >= 0) {
        if(acc_out != NULL) {
            *acc_out = g_users[idx].acc;
        }
        return idx;
    }
    uid_rev[0] = uid[3];
    uid_rev[1] = uid[2];
    uid_rev[2] = uid[1];
    uid_rev[3] = uid[0];
    idx = users_find_index_by_nfc_uid(uid_rev);
    if(idx >= 0) {
        if(acc_out != NULL) {
            *acc_out = g_users[idx].acc;
        }
        return idx;
    }
    if(!g_default_admin_deleted && g_default_admin_has_nfc) {
        if(memcmp(g_default_admin_nfc_uid, uid, 4) == 0 ||
           memcmp(g_default_admin_nfc_uid, uid_rev, 4) == 0) {
            if(acc_out != NULL) {
                *acc_out = g_default_admin_account;
            }
            return -2;
        }
    }
    return -1;
}
#endif

static uint32_t s_home_nfc_last_poll_ms = 0u;
#if APP_RS485_IS_SLAVE
static uint32_t s_nfc_match_cooldown_until = 0u;
#endif

#ifndef APP_NFC_POLL_INTERVAL_MS
#define APP_NFC_POLL_INTERVAL_MS  APP_HOME_POLL_INTERVAL_MS
#endif
#ifndef APP_NFC_DETECT_BURST_MS
#define APP_NFC_DETECT_BURST_MS   30u
#endif
#ifndef APP_NFC_MATCH_COOLDOWN_MS
#define APP_NFC_MATCH_COOLDOWN_MS 2600u
#endif
static lv_obj_t *s_home_unlock_popup = NULL;
static lv_timer_t *s_home_unlock_timer = NULL;
static uint32_t s_home_unlock_popup_ms = 0u;
static volatile uint8_t s_home_unlock_visible = 0u;

#define APP_HOME_UNLOCK_POPUP_MS 2500u
#define APP_HOME_UNLOCK_LETTER_SPACE 4

static void app_home_unlock_popup_close(void)
{
    if(s_home_unlock_timer != NULL) {
        lv_timer_del(s_home_unlock_timer);
        s_home_unlock_timer = NULL;
    }
    if(s_home_unlock_popup != NULL && lv_obj_is_valid(s_home_unlock_popup)) {
        lv_obj_del(s_home_unlock_popup);
    }
    s_home_unlock_popup = NULL;
    s_home_unlock_popup_ms = 0u;
    s_home_unlock_visible = 0u;
}

static void app_home_unlock_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_home_unlock_timer = NULL;
    app_home_unlock_popup_close();
}

void app_home_show_unlock_popup(void)
{
    lv_obj_t *mask;
    lv_obj_t *panel;
    lv_obj_t *lbl;

    if(!lv_obj_is_valid(guider_ui.screen)) return;
    app_home_unlock_popup_close();

    mask = lv_obj_create(guider_ui.screen);
    s_home_unlock_popup = mask;
    s_home_unlock_visible = 1u;
    lv_obj_set_size(mask, 240, 320);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_set_scrollbar_mode(mask, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(mask, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(mask, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(mask, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    panel = lv_obj_create(mask);
    lv_obj_set_size(panel, 180, 100);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl = lv_label_create(panel);
    lv_label_set_text(lbl, "\xE5\xBC\x80\xE9\x94\x81\xE6\x88\x90\xE5\x8A\x9F");
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl, 160);
    lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x0D3055), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(lbl, APP_HOME_UNLOCK_LETTER_SPACE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(lbl);
    lv_obj_move_foreground(mask);
    s_home_unlock_popup_ms = HAL_GetTick();

    s_home_unlock_timer = lv_timer_create(app_home_unlock_timer_cb, APP_HOME_UNLOCK_POPUP_MS, NULL);
    if(s_home_unlock_timer != NULL) {
        lv_timer_set_repeat_count(s_home_unlock_timer, 1);
    }
}

void app_home_unlock_housekeeping(void)
{
    if(s_home_unlock_popup == NULL || !lv_obj_is_valid(s_home_unlock_popup)) return;

    /* Failsafe: if timer creation/execution is delayed, force close popup. */
#if APP_RS485_IS_SLAVE
    if(g_app_scr != APP_SCR_1 || lv_scr_act() != guider_ui.screen_1) {
#else
    if(g_app_scr != APP_SCR_HOME || lv_scr_act() != guider_ui.screen) {
#endif
        app_home_unlock_popup_close();
        return;
    }
    if(s_home_unlock_popup_ms != 0u && (HAL_GetTick() - s_home_unlock_popup_ms) > (APP_HOME_UNLOCK_POPUP_MS + 1000u)) {
        app_home_unlock_popup_close();
    }
}

uint8_t app_home_unlock_popup_visible(void)
{
    return s_home_unlock_visible;
}

void app_home_unlock_set_poll_base(uint32_t now_ms)
{
    s_home_nfc_last_poll_ms = now_ms;
}

void app_home_nfc_poll_arm_immediate(void)
{
    uint32_t now = HAL_GetTick();

    if(now >= APP_NFC_POLL_INTERVAL_MS) {
        s_home_nfc_last_poll_ms = now - APP_NFC_POLL_INTERVAL_MS;
    } else {
        s_home_nfc_last_poll_ms = 0u;
    }
}

#if APP_RS485_IS_SLAVE
extern uint32_t g_home_fp_last_poll_ms;

void app_home_fp_poll_arm_immediate(void)
{
    uint32_t now = HAL_GetTick();

    if(now >= APP_HOME_POLL_INTERVAL_MS) {
        g_home_fp_last_poll_ms = now - APP_HOME_POLL_INTERVAL_MS;
    } else {
        g_home_fp_last_poll_ms = 0u;
    }
}

uint8_t app_home_unlock_in_match_cooldown(void)
{
    return (HAL_GetTick() < s_nfc_match_cooldown_until) ? 1u : 0u;
}

void app_home_unlock_arm_match_cooldown(void)
{
    s_nfc_match_cooldown_until = HAL_GetTick() + APP_NFC_MATCH_COOLDOWN_MS;
}
#endif

void app_home_nfc_poll_handle(void)
{
    uint8_t uid[4];
    uint32_t now = HAL_GetTick();
    uint8_t matched = 0u;
    const char *unlock_acc = "";
    int idx;
    uint8_t got_card;
#if (APP_SLAVE_USART1_DEBUG != 0) || (APP_SLAVE_NFC_UNLOCK_TRACE != 0)
    static uint32_t s_last_alive_log = 0u;
#endif
    static uint32_t s_last_health_check = 0u;

#if APP_RS485_IS_SLAVE
    if(g_app_scr != APP_SCR_1) {
        static uint8_t s_log_scr;
        if(s_log_scr != (uint8_t)g_app_scr) {
            s_log_scr = (uint8_t)g_app_scr;
            APP_UNLOCK_LOG("skip scr=%u (need scr1)", (unsigned)g_app_scr);
        }
        return;
    }
    if(now < s_nfc_match_cooldown_until) {
        static uint32_t s_log_cd_ms;
        if((now - s_log_cd_ms) >= 3000u) {
            s_log_cd_ms = now;
            APP_UNLOCK_LOG("skip cooldown left_ms=%lu",
                           (unsigned long)(s_nfc_match_cooldown_until - now));
        }
        return;
    }
    if(g_screen1_unlock_popup != NULL && lv_obj_is_valid(g_screen1_unlock_popup)) {
        static uint32_t s_log_pop_ms;
        if((now - s_log_pop_ms) >= 3000u) {
            s_log_pop_ms = now;
            APP_UNLOCK_LOG("skip unlock popup visible");
        }
        return;
    }
#else
    if(g_app_scr != APP_SCR_HOME) return;
#endif
    if(app_home_unlock_popup_visible()) return;
    if((now - s_home_nfc_last_poll_ms) < APP_NFC_POLL_INTERVAL_MS) return;
    s_home_nfc_last_poll_ms = now;

    /* 冷启动如果 init_once 没立起 ready，再补一次完整初始化。 */
    if(!app_nfc_hw_ready()) {
        app_nfc_hw_init_once();
        if(!app_nfc_hw_ready()) {
            static uint32_t s_log_hw_ms;
            if((now - s_log_hw_ms) >= 5000u) {
                s_log_hw_ms = now;
                APP_UNLOCK_LOG("hw not ready reset_ret=%u", (unsigned)app_nfc_last_reset_ret());
            }
            return;
        }
    }

    /* 健康检查：每 10s 读一次 TxControlReg。只有它真的掉到 0（天线断了）才 deep_recover。
     * 注意：MI_ERR 是 MFRC522 在两次 Request 之间 ErrorReg 自清的常见现象，不代表掉线，
     * 不要因为偶发 MI_ERR 就触发深恢复——deep_recover 阻塞 ~155ms，会把 LVGL 卡到看不下去。 */
    if((now - s_last_health_check) >= 10000u) {
        s_last_health_check = now;
        {
            extern unsigned char Read_MFRC522(unsigned char Address);
            uint8_t tx_now = Read_MFRC522(0x14u);
            if((tx_now & 0x03u) != 0x03u) {
#if (APP_SLAVE_LOG_VERBOSE != 0)
                APP_UNLOCK_LOG("health: tx=0x%02X (lost antenna), deep_recover", tx_now);
#endif
                app_nfc_hw_deep_recover();
            }
        }
    }

#if (APP_SLAVE_USART1_DEBUG != 0) || (APP_SLAVE_NFC_UNLOCK_TRACE != 0)
    if((now - s_last_alive_log) >= 10000u) {
        s_last_alive_log = now;
        APP_UNLOCK_LOG("poll alive ready=%u users=%u admin_nfc=%u",
                       (unsigned)app_nfc_hw_ready(),
                       (unsigned)g_user_count,
                       (unsigned)g_default_admin_has_nfc);
    }
#endif

    /* detect 单次阻塞上限 APP_NFC_DETECT_BURST_MS；从机 60ms/80ms 周期，主机 30ms/200ms。 */
    got_card = 0u;
    {
        extern unsigned char card_buf[10];
        extern unsigned char Read_MFRC522(unsigned char Address);
        unsigned char req_status = 0xFFu;
        unsigned char anti_status = 0xFFu;
#if (APP_SLAVE_LOG_VERBOSE != 0)
        unsigned char err_reg = 0u;
        unsigned char irq_reg = 0u;
        unsigned char tx_ctl = 0u;
#endif
        uint32_t det_start = HAL_GetTick();
#if (APP_SLAVE_LOG_VERBOSE != 0)
        static uint32_t s_last_detlog = 0u;
#endif

        while((HAL_GetTick() - det_start) < APP_NFC_DETECT_BURST_MS) {
            req_status = (unsigned char)MFRC522_Request(PICC_REQALL, card_buf);
            anti_status = 0xFFu;
            if(req_status == MI_OK) {
                anti_status = (unsigned char)MFRC522_Anticoll(card_buf);
                if(anti_status == MI_OK) {
                    memcpy(uid, card_buf, 4);
                    (void)MFRC522_Halt();
                    got_card = 1u;
                    break;
                }
            }
        }

#if (APP_SLAVE_LOG_VERBOSE != 0)
        if((now - s_last_detlog) >= 3000u) {
            s_last_detlog = now;
            err_reg = Read_MFRC522(0x06u);
            irq_reg = Read_MFRC522(0x04u);
            tx_ctl  = Read_MFRC522(0x14u);
            APP_UNLOCK_LOG("detect req=0x%02X anti=0x%02X err=0x%02X irq=0x%02X tx=0x%02X",
                           req_status, anti_status, err_reg, irq_reg, tx_ctl);
        }
#endif
    }
    if(got_card == 1u) {
        static uint8_t s_nfc_uid_last[4];
        static uint8_t s_nfc_hit = 0u;

        if(memcmp(uid, s_nfc_uid_last, 4) == 0) {
            if(s_nfc_hit < 255u) {
                s_nfc_hit++;
            }
        } else {
            memcpy(s_nfc_uid_last, uid, 4);
            s_nfc_hit = 1u;
        }
        if(s_nfc_hit < (uint8_t)APP_NFC_UNLOCK_CONFIRM_CNT) {
            APP_UNLOCK_LOG("uid confirm %u/%u",
                           (unsigned)s_nfc_hit, (unsigned)APP_NFC_UNLOCK_CONFIRM_CNT);
            return;
        }
        s_nfc_hit = 0u;

#if APP_RS485_IS_SLAVE
        idx = app_nfc_match_user(uid, &unlock_acc);
        matched = (idx != -1) ? 1u : 0u;
#else
        idx = users_find_index_by_nfc_uid(uid);
        if(idx >= 0) {
            matched = 1u;
            unlock_acc = g_users[idx].acc;
        } else if(!g_default_admin_deleted && g_default_admin_has_nfc &&
                  memcmp(g_default_admin_nfc_uid, uid, 4) == 0) {
            matched = 1u;
            unlock_acc = g_default_admin_account;
        }
#endif
        APP_UNLOCK_LOG("uid=%02X%02X%02X%02X match=%u acc=%.12s",
                       uid[0], uid[1], uid[2], uid[3],
                       (unsigned)matched, unlock_acc);
#if APP_RS485_IS_SLAVE
        if(matched == 0u) {
            APP_UNLOCK_LOG("no_match stored_admin=%02X%02X%02X%02X (sync users via RS485 host)",
                           g_default_admin_nfc_uid[0], g_default_admin_nfc_uid[1],
                           g_default_admin_nfc_uid[2], g_default_admin_nfc_uid[3]);
        }
#endif
        if(matched) {
#if APP_RS485_IS_SLAVE
            APP_UNLOCK_LOG("unlock OK method=nfc acc=%s", unlock_acc);
            app_home_unlock_arm_match_cooldown();
            app_unlock_event_handle_success(APP_UNLOCK_POPUP_SCREEN1, unlock_acc, "nfc");
#else
            app_unlock_event_handle_success(APP_UNLOCK_POPUP_HOME, unlock_acc, "nfc");
#endif
        }
    }
}
