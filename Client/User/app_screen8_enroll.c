#include "app_screen8_enroll.h"

#include <string.h>

#include "app_screen.h"
#include "app_state.h"
#include "app_user_ops.h"
#include "app_nfc_hw.h"
#include "app_screen8_popup.h"
#include "../Drivers/BSP/AS608/as608.h"
#include "../Drivers/BSP/mfrc522/door.h"

void screen8_show_fp_enroll_popup(void)
{
    uint32_t addr = 0u;
    uint8_t hs_try;
    uint8_t hs_ok = 0u;
    uint16_t valid_n = 0u;

    if(!g_fp_hw_inited) {
        GZ_StaGPIO_Init();
        g_fp_hw_inited = 1u;
    }
    for(hs_try = 0u; hs_try < 3u; hs_try++) {
        as608_rx_buffer_clear();
        if(GZ_HandShake(&addr) == 0u) {
            AS608Addr = addr;
            hs_ok = 1u;
            break;
        }
        HAL_Delay(40);
    }
    if(hs_ok) {
    }
    if(GZ_ValidTempleteNum(&valid_n) == 0x00u) {
        g_fp_enroll_page_id = valid_n;
        g_fp_enroll_page_id_2 = (uint16_t)(valid_n + 1u);
    } else {
        g_fp_enroll_page_id = 0u;
        g_fp_enroll_page_id_2 = 1u;
    }
    g_fp_enroll_step = 0u;
    g_screen8_fp_enroll_start_time = HAL_GetTick();
    g_screen8_fp_enroll_state = 1u;
    g_screen8_fp_enroll_result = 0u;
    screen8_show_enroll_popup("Fingerprint Enrollment...");
}

void screen8_handle_fp_enroll(void)
{
    uint32_t current_time;
    static uint32_t s_last_fp_log_ms = 0u;
    static uint32_t s_last_getimg_ms = 0u;
    static uint8_t s_noack_cnt = 0u;
    uint8_t en;

    if(g_screen8_fp_enroll_state != 1u) return;

    current_time = HAL_GetTick();
    if((current_time - s_last_fp_log_ms) >= 500u) s_last_fp_log_ms = current_time;
    if((current_time - g_screen8_fp_enroll_start_time) >= 10000u) {
        g_screen8_fp_enroll_state = 2u;
        g_screen8_fp_enroll_result = 0u;
        screen8_show_enroll_popup("Fingerprint FAIL");
        return;
    }

    if((current_time - s_last_getimg_ms) < 120u) return;
    s_last_getimg_ms = current_time;

    en = GZ_GetImage();

    if(en == 0x02u || en == 2u) {
        if(g_fp_enroll_step == 1u) g_fp_enroll_step = 2u;
        s_noack_cnt = 0u;
        return;
    }
    if(en == 0xFFu) {
        s_noack_cnt++;
        if(s_noack_cnt < 3u) return;
        g_screen8_fp_enroll_state = 2u;
        g_screen8_fp_enroll_result = 0u;
        screen8_show_enroll_popup("Fingerprint FAIL");
        return;
    }
    s_noack_cnt = 0u;
    if(en == 0x01u) return;
    if(en != 0x00u) {
        /* 0x06=图像太乱 / 0x07=特征点过少 / 其它瞬态错误：
         * 不立刻 FAIL，10s 总超时内允许用户重新按压。 */
        return;
    }

    if(g_fp_enroll_step == 0u) {
        en = GZ_GenChar(CharBuffer1);
        if(en == 0x00u) {
            g_fp_enroll_step = 1u;
            return;
        }
        /* 第一次特征提取失败：保持 step 0，让用户继续尝试。 */
        return;
    }
    if(g_fp_enroll_step == 1u) return;

    if(g_fp_enroll_step == 2u) {
        en = GZ_GenChar(CharBuffer2);
        if(en != 0x00u) {
            /* 第二次特征提取失败：保持 step 2，让用户重按第二次（10s 内）。 */
            return;
        }
        en = GZ_Match();
        if(en != 0x00u) {
            /* 两次按压不是同一指纹：直接 FAIL，让用户从头开始录入。
             * 之前我尝试过"回 step 0 自动重来"，但容易导致一直卡在 Match 上反复刷
             * 而无法成功，体验不如直接 FAIL 更明确。 */
            g_screen8_fp_enroll_state = 2u;
            g_screen8_fp_enroll_result = 0u;
            screen8_show_enroll_popup("Fingerprint FAIL");
            return;
        }
        en = GZ_RegModel();
        if(en != 0x00u) {
            g_screen8_fp_enroll_state = 2u;
            g_screen8_fp_enroll_result = 0u;
            screen8_show_enroll_popup("Fingerprint FAIL");
            return;
        }

        en = GZ_StoreChar(CharBuffer2, g_fp_enroll_page_id);
        if(en == 0x00u) {
            en = GZ_StoreChar(CharBuffer2, g_fp_enroll_page_id_2);
            if(en != 0x00u) (void)fp_delete_template_by_page(g_fp_enroll_page_id);
        }
        if(en == 0x00u) {
            if(g_app_scr == APP_SCR_10) {
                if(g_screen5_found_acc[0] != '\0' &&
                   users_bind_fp_by_acc(g_screen5_found_acc, g_fp_enroll_page_id) &&
                   users_bind_fp_by_acc(g_screen5_found_acc, g_fp_enroll_page_id_2)) {
                    g_screen8_fp_enroll_result = 1u;
                } else {
                    (void)fp_delete_template_by_page(g_fp_enroll_page_id);
                    (void)fp_delete_template_by_page(g_fp_enroll_page_id_2);
                    g_screen8_fp_enroll_result = 0u;
                }
            } else {
                if(g_fp_pending_page_valid) (void)fp_delete_template_by_page(g_fp_pending_page_id);
                if(g_fp_pending_page2_valid) (void)fp_delete_template_by_page(g_fp_pending_page_id_2);
                g_fp_pending_page_id = g_fp_enroll_page_id;
                g_fp_pending_page_valid = 1u;
                g_fp_pending_page_id_2 = g_fp_enroll_page_id_2;
                g_fp_pending_page2_valid = 1u;
                g_screen8_chip_read_ok = 1u;
                g_screen8_fp_enroll_result = 1u;
            }
            g_screen8_fp_enroll_state = 2u;
            screen8_show_enroll_popup((g_screen8_fp_enroll_result == 1u) ? "Fingerprint OK" : "Fingerprint FAIL");
            return;
        }
    }

    g_screen8_fp_enroll_state = 2u;
    g_screen8_fp_enroll_result = 0u;
    screen8_show_enroll_popup("Fingerprint FAIL");
}

void screen8_show_nfc_enroll_popup(void)
{
    if(!app_nfc_hw_ready()) {
        app_nfc_hw_init_once();
    }
    app_nfc_hw_wakeup_no_reset();
    MFRC522_AntennaOn();
    Flash_ReadID();
    if(g_nfc_op != NFC_OP_REPLACE_SCREEN9) {
        g_screen8_chip_read_ok = 0u;
        g_nfc_pending_uid_valid = 0u;
    }

    screen8_show_enroll_popup("NFC Working...");
    g_nfc_enroll_start_time = HAL_GetTick();
    g_nfc_enroll_state = 1u;
    g_nfc_enroll_result = 0u;
}

void screen8_handle_nfc_enroll(void)
{
    uint32_t current_time;
    static uint32_t s_last_poll_log = 0u;
    static uint32_t s_last_recover_ms = 0u;
    uint8_t card_id[4] = {0};
    uint8_t detect_result;

    if(g_nfc_enroll_state != 1u) return;

    current_time = HAL_GetTick();
    if((current_time - s_last_poll_log) >= 500u) s_last_poll_log = current_time;
    if((current_time - g_nfc_enroll_start_time) >= 10000u) {
        g_nfc_enroll_state = 2u;
        g_nfc_enroll_result = 0u;
        screen8_show_enroll_popup("NFC FAIL");
        return;
    }

    if((current_time - s_last_recover_ms) >= 1500u) {
        s_last_recover_ms = current_time;
        (void)MFRC522_Reset();
        MFRC522_AntennaOn();
    }

    /* Split long detect wait into shorter slices to avoid blocking UI loop. */
    detect_result = NFC_Enroll_Detect(card_id, 20);
    g_nfc_last_detect_result = detect_result;
    if(detect_result != 1u) return;

    g_nfc_last_uid[0] = card_id[0];
    g_nfc_last_uid[1] = card_id[1];
    g_nfc_last_uid[2] = card_id[2];
    g_nfc_last_uid[3] = card_id[3];

    g_nfc_enroll_state = 2u;
    if(card_id[0] == 0u && card_id[1] == 0u && card_id[2] == 0u && card_id[3] == 0u) {
        g_nfc_enroll_result = 0u;
        screen8_show_enroll_popup("NFC FAIL");
    } else {
        if(g_nfc_op == NFC_OP_REPLACE_SCREEN9) {
            g_nfc_enroll_result = (g_screen5_found_acc[0] != '\0' && users_bind_nfc_by_acc(g_screen5_found_acc, card_id)) ? 1u : 0u;
        } else {
            memcpy(g_nfc_pending_uid, card_id, 4u);
            g_nfc_pending_uid_valid = 1u;
            g_screen8_chip_read_ok = 1u;
            g_nfc_enroll_result = 1u;
        }
        screen8_show_enroll_popup((g_nfc_enroll_result == 1u) ? "NFC SUCCESS" : "NFC FAIL");
    }
}
