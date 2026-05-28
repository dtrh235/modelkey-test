#include "app_fp_host_match.h"

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER

#include <string.h>

#include "../Drivers/BSP/AS608/as608.h"
#include "app_fp_mirror_diag.h"
#include "app_fp_mirror_tx.h"
#include "app_home_fp_poll.h"
#include "app_rs485_proto.h"
#include "app_state.h"
#include "app_screen.h"
#include "app_user_ops.h"

extern uint8_t g_fp_hw_inited;
extern uint8_t g_screen8_fp_enroll_state;

static uint8_t  s_feat_asm[AS608_TEMPLATE_SIZE];
static uint8_t  s_feat_mask;
static uint16_t s_feat_seq = 0xFFFFu;

static void fp_match_fill_rsp(rs485_fp_match_rsp_t *rsp, uint8_t result)
{
    if(rsp == NULL) {
        return;
    }
    memset(rsp, 0, sizeof(*rsp));
    rsp->result = result;
    rsp->page_id = 0xFFFFu;
    rsp->acc[0] = '\0';
}

static uint8_t fp_match_host_busy(void)
{
    if(app_fp_mirror_tx_busy() != 0u) {
        return 1u;
    }
    if(app_rs485_master_cmd_in_progress() != 0u) {
        return 1u;
    }
    if(g_screen8_fp_enroll_state == 1u || g_app_scr == APP_SCR_10) {
        return 1u;
    }
    return 0u;
}

static void fp_match_run_on_host(const uint8_t *feat512, rs485_fp_match_rsp_t *rsp)
{
    SearchResult search;
    uint8_t en;
    const char *acc;

    fp_match_fill_rsp(rsp, RS485_FP_MATCH_RESULT_ERR);

    if(feat512 == NULL) {
        return;
    }
    if(fp_match_host_busy() != 0u) {
        fp_match_fill_rsp(rsp, RS485_FP_MATCH_RESULT_BUSY);
        return;
    }

    if(!g_fp_hw_inited) {
        GZ_StaGPIO_Init();
        g_fp_hw_inited = 1u;
    }
    if(app_fp_hw_try_handshake_retries(&g_fp_hw_inited, 2u, 40u) == 0u) {
        return;
    }

    as608_rx_buffer_clear();
    HAL_Delay(10);
    en = GZ_DownCharBuffer(CharBuffer1, feat512, AS608_TEMPLATE_SIZE);
    if(en != 0x00u) {
        FP_MIRROR_LOG("[HOST][FP] match DownChar FAIL ret=0x%02X", (unsigned)en);
        return;
    }

    as608_rx_buffer_clear();
    HAL_Delay(10);
    en = GZ_SearchLibrary(CharBuffer1, &search);
    if(en != 0x00u) {
        fp_match_fill_rsp(rsp, RS485_FP_MATCH_RESULT_NO_MATCH);
        FP_MIRROR_LOG("[HOST][FP] match Search FAIL ret=0x%02X", (unsigned)en);
        return;
    }

    if(search.mathscore < (uint16_t)APP_FP_MATCH_SCORE_MIN) {
        fp_match_fill_rsp(rsp, RS485_FP_MATCH_RESULT_NO_MATCH);
        FP_MIRROR_LOG("[HOST][FP] match page=%u low score=%u",
                      (unsigned)search.pageID, (unsigned)search.mathscore);
        return;
    }
    if(users_match_fp_page(search.pageID) == 0u) {
        fp_match_fill_rsp(rsp, RS485_FP_MATCH_RESULT_NO_MATCH);
        FP_MIRROR_LOG("[HOST][FP] match page=%u no user bind", (unsigned)search.pageID);
        return;
    }

    acc = users_get_acc_by_fp_page(search.pageID);
    if(acc == NULL || acc[0] == '\0') {
        fp_match_fill_rsp(rsp, RS485_FP_MATCH_RESULT_NO_MATCH);
        return;
    }

    rsp->result = RS485_FP_MATCH_RESULT_OK;
    rsp->page_id = search.pageID;
    rsp->score = search.mathscore;
    strncpy(rsp->acc, acc, sizeof(rsp->acc) - 1u);
    rsp->acc[sizeof(rsp->acc) - 1u] = '\0';
    FP_MIRROR_LOG("[HOST][FP] match OK page=%u acc=%s score=%u",
                  (unsigned)search.pageID, rsp->acc, (unsigned)search.mathscore);
}

uint8_t app_fp_host_match_char_chunk(const rs485_fp_char_chunk_t *chunk, rs485_fp_match_rsp_t *rsp_out)
{
    uint32_t off;
    uint8_t need_all;

    if(chunk == NULL || rsp_out == NULL) {
        return 0u;
    }
    if(chunk->chunk_count != RS485_FP_CHUNK_COUNT ||
       chunk->chunk_idx >= RS485_FP_CHUNK_COUNT) {
        fp_match_fill_rsp(rsp_out, RS485_FP_MATCH_RESULT_ERR);
        return 1u;
    }

    if(s_feat_seq != chunk->page_id) {
        s_feat_seq = chunk->page_id;
        s_feat_mask = 0u;
        memset(s_feat_asm, 0, sizeof(s_feat_asm));
    }

    off = (uint32_t)chunk->chunk_idx * RS485_FP_CHUNK_BYTES;
    memcpy(s_feat_asm + off, chunk->data, RS485_FP_CHUNK_BYTES);
    s_feat_mask |= (uint8_t)(1u << chunk->chunk_idx);

    need_all = (uint8_t)((1u << RS485_FP_CHUNK_COUNT) - 1u);
    if(s_feat_mask != need_all) {
        return 0u;
    }

    s_feat_mask = 0u;
    fp_match_run_on_host(s_feat_asm, rsp_out);
    return 1u;
}

#endif /* APP_RS485_ENABLE && MASTER */
