#include "app_screen_pair_flow.h"

#include <stdio.h>
#include <string.h>

#include "gui_guider.h"
#include "app_pair_bind.h"
#include "app_screen3_menu.h"
#include "app_state.h"
#include "cloud_aliyun_at.h"

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_21);
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_25);
LV_FONT_DECLARE(lv_font_montserratMedium_51);

extern lv_ui guider_ui;

static lv_obj_t *s_pair_root = NULL;
static lv_obj_t *s_pair_title = NULL;
static lv_obj_t *s_pair_status = NULL;
static lv_obj_t *s_pair_code = NULL;
static lv_obj_t *s_pair_regen_btn = NULL;
static lv_obj_t *s_pair_regen_lbl = NULL;
static uint8_t s_pair_open = 0u;

static void pair_apply_btn_style(lv_obj_t *btn, lv_coord_t w, uint8_t enabled)
{
    if(!lv_obj_is_valid(btn)) {
        return;
    }
    lv_obj_set_size(btn, w, 30);
    lv_obj_set_style_bg_opa(btn, enabled ? 165 : 90, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(enabled ? 0x009bff : 0x9aa3ad),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(btn, &lv_font_SourceHanSerifSC_Regular_21, LV_PART_MAIN | LV_STATE_DEFAULT);
    if(enabled != 0u) {
        lv_obj_clear_state(btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
    }
}

static void pair_build_ui(void)
{
    lv_obj_t *parent;

    if(lv_obj_is_valid(s_pair_root)) {
        return;
    }
    if(!lv_obj_is_valid(guider_ui.screen_3_cont_1)) {
        return;
    }

    parent = guider_ui.screen_3_cont_1;
    s_pair_root = lv_obj_create(parent);
    lv_obj_set_pos(s_pair_root, 0, 0);
    lv_obj_set_size(s_pair_root, 240, 320);
    lv_obj_set_style_bg_opa(s_pair_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_pair_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(s_pair_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(s_pair_root, LV_OBJ_FLAG_SCROLLABLE);

    s_pair_title = lv_label_create(s_pair_root);
    lv_label_set_text(s_pair_title, "App配对");
    lv_obj_set_pos(s_pair_title, 6, 13);
    lv_obj_set_style_text_font(s_pair_title, &lv_font_SourceHanSerifSC_Regular_25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_pair_title, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);

    s_pair_status = lv_label_create(s_pair_root);
    lv_label_set_text(s_pair_status, "");
    lv_obj_set_width(s_pair_status, 228);
    lv_obj_set_style_text_font(s_pair_status, &lv_font_SourceHanSerifSC_Regular_21, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_pair_status, lv_color_hex(0x9aa3ad), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(s_pair_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_pair_code = lv_label_create(s_pair_root);
    lv_label_set_text(s_pair_code, "000000");
    lv_obj_set_size(s_pair_code, 240, 60);
    lv_obj_align(s_pair_code, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_text_align(s_pair_code, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_pair_code, &lv_font_montserratMedium_51, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_pair_code, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(s_pair_code, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_pair_regen_btn = lv_btn_create(s_pair_root);
    s_pair_regen_lbl = lv_label_create(s_pair_regen_btn);
    lv_label_set_text(s_pair_regen_lbl, "重新生成配对码");
    lv_label_set_long_mode(s_pair_regen_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_pair_regen_lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(s_pair_regen_btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_pair_regen_lbl, &lv_font_SourceHanSerifSC_Regular_21, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_flag(s_pair_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(guider_ui.screen_3_btn_1);
}

void screen_pair_refresh(void)
{
    const char *code;
    lv_coord_t tw;
    lv_coord_t bw;
    lv_coord_t btn_x;
    uint8_t wifi_ok;
    uint8_t cloud_ok;
    uint8_t regen_ok;

    if(!lv_obj_is_valid(s_pair_code)) {
        return;
    }

    wifi_ok = cloud_aliyun_at_wifi_link_ready();
    cloud_ok = app_pair_cloud_ready();
    if(app_pair_is_bound() != 0u) {
        /* 已绑定时允许本地解除并重新出码，不依赖云端下行 */
        regen_ok = 1u;
    } else {
        regen_ok = (wifi_ok != 0u) ? 1u : 0u;
    }

    if(wifi_ok != 0u && cloud_ok == 0u) {
        if(cloud_aliyun_at_wifi_bringup_active() == 0u) {
            cloud_aliyun_at_request_mqtt_connect();
        }
    }

    if(app_pair_is_bound() != 0u) {
        const char *bound_acc = app_pair_get_bound_account();
        if(cloud_ok != 0u) {
            lv_label_set_text(s_pair_status, "已绑定");
            lv_obj_set_style_text_color(s_pair_status, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if(wifi_ok != 0u) {
            lv_label_set_text(s_pair_status, "云端重连中");
            lv_obj_set_style_text_color(s_pair_status, lv_color_hex(0xc45c00), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_label_set_text(s_pair_status, "已绑定·WiFi离线");
            lv_obj_set_style_text_color(s_pair_status, lv_color_hex(0xc45c00), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if(bound_acc != NULL && bound_acc[0] != '\0' && cloud_ok != 0u) {
            char acc_line[24];
            (void)snprintf(acc_line, sizeof(acc_line), "账号 %s", bound_acc);
            lv_label_set_text(s_pair_code, acc_line);
            lv_obj_set_style_text_font(s_pair_code, &lv_font_SourceHanSerifSC_Regular_21,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(s_pair_code, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(s_pair_code, LV_ALIGN_CENTER, 0, -6);
            lv_obj_align(s_pair_status, LV_ALIGN_CENTER, 0, 36);
        } else {
            lv_obj_add_flag(s_pair_code, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(s_pair_status, LV_ALIGN_CENTER, 0, 40);
        }
        lv_obj_clear_flag(s_pair_status, LV_OBJ_FLAG_HIDDEN);
    } else if(wifi_ok == 0u) {
        lv_label_set_text(s_pair_status, "请先完成WiFi设置");
        lv_obj_set_style_text_color(s_pair_status, lv_color_hex(0xc45c00), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(s_pair_status, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_pair_code, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(s_pair_status, LV_ALIGN_CENTER, 0, 40);
    } else {
        app_pair_ensure_code();
        code = app_pair_get_code();
        lv_obj_clear_flag(s_pair_code, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_pair_code, code);
        lv_obj_align(s_pair_code, LV_ALIGN_CENTER, 0, -10);
        lv_obj_clear_flag(s_pair_status, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align_to(s_pair_status, s_pair_code, LV_ALIGN_OUT_BOTTOM_MID, 0, 14);
        if(cloud_ok == 0u) {
            lv_label_set_text(s_pair_status, "不可配对");
            lv_obj_set_style_text_color(s_pair_status, lv_color_hex(0x9aa3ad), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(s_pair_code, lv_color_hex(0x9aa3ad), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_label_set_text(s_pair_status, "可配对");
            lv_obj_set_style_text_color(s_pair_status, lv_color_hex(0x1a8f4a), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(s_pair_code, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    if(lv_obj_is_valid(s_pair_regen_btn) && lv_obj_is_valid(s_pair_regen_lbl)) {
        const char *btn_txt = (app_pair_is_bound() != 0u) ? "重新配对" : "重新生成配对码";

        lv_label_set_text(s_pair_regen_lbl, btn_txt);
        tw = lv_txt_get_width(btn_txt, (uint32_t)strlen(btn_txt),
                              &lv_font_SourceHanSerifSC_Regular_21, 0, LV_TEXT_FLAG_NONE);
        bw = (lv_coord_t)(tw + 16);
        if(bw < 110) {
            bw = 110;
        }
        if(bw > 168) {
            bw = 168;
        }
        btn_x = (lv_coord_t)(240 - bw - 6);
        pair_apply_btn_style(s_pair_regen_btn, bw, regen_ok);
        lv_obj_set_pos(s_pair_regen_btn, btn_x, 44);
        lv_obj_clear_flag(s_pair_regen_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

void screen_pair_on_regen(void)
{
    if(app_pair_is_bound() != 0u) {
        app_pair_regenerate_code();
        if(cloud_aliyun_at_wifi_link_ready() != 0u && app_pair_cloud_ready() == 0u) {
            cloud_aliyun_at_request_mqtt_connect();
        }
        screen_pair_refresh();
        return;
    }
    if(cloud_aliyun_at_wifi_link_ready() == 0u) {
        screen_pair_refresh();
        return;
    }
    app_pair_regenerate_code();
    if(app_pair_cloud_ready() == 0u) {
        cloud_aliyun_at_request_mqtt_connect();
    }
    screen_pair_refresh();
}

uint8_t screen_pair_hit_regen(lv_coord_t x, lv_coord_t y)
{
    lv_area_t a;
    lv_point_t p;

    if(!s_pair_open || !lv_obj_is_valid(s_pair_regen_btn)) {
        return 0u;
    }
    if(lv_obj_has_flag(s_pair_regen_btn, LV_OBJ_FLAG_HIDDEN)) {
        return 0u;
    }
    if(lv_obj_has_state(s_pair_regen_btn, LV_STATE_DISABLED)) {
        return 0u;
    }
    lv_obj_get_coords(s_pair_regen_btn, &a);
    p.x = x;
    p.y = y;
    return _lv_area_is_point_on(&a, &p, 0) ? 1u : 0u;
}

uint8_t screen_pair_is_open(void)
{
    if(s_pair_open != 0u && !lv_obj_is_valid(s_pair_root)) {
        s_pair_open = 0u;
    }
    return s_pair_open;
}

void enter_screen_pair(void)
{
    screen3_init_scroll_layout();
    pair_build_ui();
    screen3_hide_menu_panel();
    if(lv_obj_is_valid(guider_ui.screen_3_label_1)) {
        lv_obj_add_flag(guider_ui.screen_3_label_1, LV_OBJ_FLAG_HIDDEN);
    }
    if(cloud_aliyun_at_wifi_link_ready() != 0u) {
        cloud_aliyun_at_request_mqtt_connect();
    }
    screen_pair_refresh();
    if(lv_obj_is_valid(s_pair_root)) {
        lv_obj_clear_flag(s_pair_root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_background(s_pair_root);
        lv_obj_move_foreground(guider_ui.screen_3_btn_1);
    }
    s_pair_open = 1u;
}

void screen_pair_exit_to_menu(void)
{
    if(s_pair_open == 0u) {
        return;
    }
    if(lv_obj_is_valid(s_pair_root)) {
        lv_obj_add_flag(s_pair_root, LV_OBJ_FLAG_HIDDEN);
    }
    if(lv_obj_is_valid(guider_ui.screen_3_label_1)) {
        lv_obj_clear_flag(guider_ui.screen_3_label_1, LV_OBJ_FLAG_HIDDEN);
    }
    screen3_show_menu_panel();
    screen3_init_scroll_layout();
    g_screen3_need_init = 1u;
    s_pair_open = 0u;
}
