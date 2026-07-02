#include "app_ui_v3_diag.h"

#if (APP_UI_V3_ENABLE == 1) && (APP_UI_V3_DIAG != 0)

#include <stdio.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "app_ui_v3.h"
#include "../../Drivers/SYSTEM/usart/usart.h"

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

static const char *ui3_diag_scr_name(ui3_scr_id_t id)
{
    switch(id) {
    case UI3_SCR_HOME:       return "HOME";
    case UI3_SCR_UNLOCK:     return "UNLOCK";
    case UI3_SCR_ADMIN:      return "ADMIN";
    case UI3_SCR_MENU:       return "MENU";
    case UI3_SCR_ADD_USER:   return "ADD_USER";
    case UI3_SCR_DEL_USER:   return "DEL_USER";
    case UI3_SCR_SEARCH:     return "SEARCH";
    case UI3_SCR_EDIT_USER:  return "EDIT_USER";
    case UI3_SCR_USER_LIST:  return "USER_LIST";
    case UI3_SCR_WIFI:       return "WIFI";
    case UI3_SCR_PAIR:       return "PAIR";
    case UI3_SCR_NFC_MGMT:   return "NFC_MGMT";
    case UI3_SCR_FP_MGMT:    return "FP_MGMT";
    default:                 return "?";
    }
}

void ui3_diag_str(const char *s)
{
    if(s != NULL) {
        usart_debug_tx_str(s);
    }
}

void ui3_diag_fmt(const char *tag, const char *msg)
{
    char buf[96];

    if(tag == NULL) {
        tag = "UI3";
    }
    if(msg == NULL) {
        msg = "";
    }
    (void)snprintf(buf, sizeof(buf), "[UI3][%s] %s\r\n", tag, msg);
    usart_debug_tx_str(buf);
}

void ui3_diag_mem(const char *tag)
{
    lv_mem_monitor_t mon;
    char buf[96];

    lv_mem_monitor(&mon);
    (void)snprintf(buf, sizeof(buf),
                   "[UI3][%s] lv free=%u used=%u%% frag=%u%% max=%u cnt=%u/%u\r\n",
                   (tag != NULL) ? tag : "mem",
                   (unsigned)mon.free_size,
                   (unsigned)mon.used_pct,
                   (unsigned)mon.frag_pct,
                   (unsigned)mon.max_used,
                   (unsigned)mon.used_cnt,
                   (unsigned)mon.free_cnt);
    usart_debug_tx_str(buf);

#if (APP_USE_FREERTOS == 1)
    (void)snprintf(buf, sizeof(buf),
                   "[UI3][%s] rtos free=%u min=%u\r\n",
                   (tag != NULL) ? tag : "mem",
                   (unsigned)xPortGetFreeHeapSize(),
                   (unsigned)xPortGetMinimumEverFreeHeapSize());
    usart_debug_tx_str(buf);
#endif
}

void ui3_diag_scr(const char *tag, ui3_scr_id_t id, const void *scr)
{
    char buf[96];
    lv_opa_t opa = LV_OPA_COVER;
    const lv_obj_t *act = lv_scr_act();

    if(act != NULL) {
        opa = lv_obj_get_style_opa(act, 0);
    }
    (void)snprintf(buf, sizeof(buf),
                   "[UI3][%s] scr=%u(%s) obj=%p act=%p opa=%u\r\n",
                   (tag != NULL) ? tag : "scr",
                   (unsigned)id,
                   ui3_diag_scr_name(id),
                   scr,
                   (void *)act,
                   (unsigned)opa);
    usart_debug_tx_str(buf);
}

void ui3_diag_nav(const char *op, ui3_scr_id_t id, uint8_t push)
{
    char buf[64];

    (void)snprintf(buf, sizeof(buf),
                   "[UI3][nav] %s id=%u(%s) push=%u\r\n",
                   (op != NULL) ? op : "?",
                   (unsigned)id,
                   ui3_diag_scr_name(id),
                   (unsigned)push);
    usart_debug_tx_str(buf);
}

void ui3_diag_boot_done(void)
{
    ui3_diag_str("[UI3] boot trace ON -> USART1 PA9 115200 (APP_DEBUG_ON_USART6=0)\r\n");
    ui3_diag_mem("boot_done");
    ui3_diag_scr("boot_done", UI3_SCR_HOME, lv_scr_act());
    ui3_diag_home_tree();
}

void ui3_diag_disp_snap(const char *tag)
{
    lv_port_disp_diag_t d;
    char buf[120];
    static uint32_t s_prev_flush;

    lv_port_disp_diag_get(&d);
    (void)snprintf(buf, sizeof(buf),
                   "[UI3][%s] flush=%lu(+%lu) px=%lu en=%u area=%u,%u-%u,%u samp=0x%04X\r\n",
                   (tag != NULL) ? tag : "disp",
                   (unsigned long)d.flush_total,
                   (unsigned long)(d.flush_total - s_prev_flush),
                   (unsigned long)d.flush_bytes,
                   (unsigned)d.flush_enabled,
                   (unsigned)d.last_x1, (unsigned)d.last_y1,
                   (unsigned)d.last_x2, (unsigned)d.last_y2,
                   (unsigned)d.sample_px);
    usart_debug_tx_str(buf);
    s_prev_flush = d.flush_total;
}

void ui3_diag_home_tree(void)
{
    ui3_state_t *st = ui3_state();
    lv_obj_t *scr = lv_scr_act();
    char buf[128];
    const char *clock_txt = "?";
    const char *date_txt = "?";
    lv_color_t bg;
    lv_opa_t bg_opa;

    if(scr == NULL || !lv_obj_is_valid(scr)) {
        ui3_diag_fmt("home", "scr invalid");
        return;
    }
    bg = lv_obj_get_style_bg_color(scr, 0);
    bg_opa = lv_obj_get_style_bg_opa(scr, 0);
    if(st != NULL && st->lbl_clock_big != NULL && lv_obj_is_valid(st->lbl_clock_big)) {
        clock_txt = lv_label_get_text(st->lbl_clock_big);
    }
    if(st != NULL && st->lbl_date != NULL && lv_obj_is_valid(st->lbl_date)) {
        date_txt = lv_label_get_text(st->lbl_date);
    }
    (void)snprintf(buf, sizeof(buf),
                   "[UI3][home] child=%u bg=0x%04X opa=%u clk=%s date=%s menu=%p unlock=%p\r\n",
                   (unsigned)lv_obj_get_child_cnt(scr),
                   (unsigned)bg.full,
                   (unsigned)bg_opa,
                   (clock_txt != NULL) ? clock_txt : "",
                   (date_txt != NULL) ? date_txt : "",
                   (st != NULL) ? (void *)st->btn_home_menu : NULL,
                   (st != NULL) ? (void *)st->btn_home_unlock : NULL);
    usart_debug_tx_str(buf);
}

void ui3_diag_gui_first_poll(void)
{
    static uint8_t s_done;

    if(s_done != 0u) {
        return;
    }
    s_done = 1u;
    ui3_diag_str("[UI3] gui_first_poll (scheduler running)\r\n");
    ui3_diag_disp_snap("gui0_before");
    ui3_diag_home_tree();
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(lv_disp_get_default());
    ui3_diag_disp_snap("gui0_refr");
}

void ui3_diag_gui_heartbeat(ui3_scr_id_t scr)
{
    static uint32_t s_last_ms;
    uint32_t now = lv_tick_get();
    uint32_t refr_ms;

    if((now - s_last_ms) < 5000u) {
        return;
    }
    s_last_ms = now;
    ui3_diag_scr("hb", scr, lv_scr_act());
    ui3_diag_mem("hb");
    ui3_diag_home_tree();
    ui3_diag_disp_snap("hb_before");
    lv_obj_invalidate(lv_scr_act());
    refr_ms = lv_timer_handler();
    lv_refr_now(lv_disp_get_default());
    {
        char buf[64];
        (void)snprintf(buf, sizeof(buf), "[UI3][hb] lv_timer_handler=%ums\r\n", (unsigned)refr_ms);
        usart_debug_tx_str(buf);
    }
    ui3_diag_disp_snap("hb_after");
}

#endif
