#include "app_screen_wifi_flow.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "app_wifi_diag.h"
#include "app_ui_nav.h"
#include "cloud_aliyun_at.h"
#include "ui.h"
#include "ui_nav_flow.h"
#include "app_screen.h"
#include "app_state.h"
#include "ui_common_utils.h"
#include "ui_auth_input.h"
#include "app_wifi_cfg.h"
#include "app_wifi_remember.h"
#include "app_wifi_scan.h"
#include "app_link_guard.h"
#include "app_touch_ui.h"
#include "app_config.h"
#if (APP_WIFI_UART_DEBUG == 0)
#undef printf
#define printf(...) ((void)0)
#endif

/* 滑动列表时降低 merge 频率，避免与 CWLAP 刷新抢 GuiTask */
#define WIFI_LIST_MERGE_SCROLL_MS  400u
#define WIFI_REFRESH_BTN_X          174
#define WIFI_REFRESH_BTN_Y          78
#define WIFI_REFRESH_BTN_W          62
#define WIFI_REFRESH_BTN_H          30

LV_FONT_DECLARE(lv_font_cn_wifi_16);
LV_FONT_DECLARE(lv_font_cn_wifi_25);
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_21);
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_25);
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20);
LV_FONT_DECLARE(lv_font_montserratMedium_16);
LV_FONT_DECLARE(lv_font_montserrat_14);

extern lv_ui guider_ui;
extern volatile app_scr_t g_app_scr;
extern uint8_t g_screen3_need_init;
extern uint8_t g_screen3_pending_index;

#define WIFI_ROW_MAX       APP_WIFI_SCAN_MAX
#define WIFI_ROW_H         34
#define WIFI_LIST_NAME_X   26
#define WIFI_LIST_NAME_W   206
#define WIFI_LIST_PANEL_Y  108
#define WIFI_LIST_PANEL_H  168
#define WIFI_TOUCH_HIT_PAD 6

typedef enum {
    WIFI_ROW_ST_NONE = 0,
    WIFI_ROW_ST_CONNECTING,
    WIFI_ROW_ST_OK,
    WIFI_ROW_ST_FAIL
} wifi_row_st_t;

typedef struct {
    lv_obj_t *row;
    lv_obj_t *lbl_icon;
    lv_obj_t *lbl_name;
    lv_obj_t *lbl_status;
    wifi_row_st_t status;
} wifi_row_ui_t;

static wifi_row_ui_t s_rows[WIFI_ROW_MAX];
/* 行最近一次在扫描结果中“出现”的时刻（lv_tick_get ms）。
 * 扫描过程中只增量追加不删除；仅在 scan_done 后按 last_seen 逐步删除，避免闪屏。 */
static uint32_t s_row_last_seen_ms[WIFI_ROW_MAX];
static uint8_t s_sel_index = 0u;
static uint8_t s_list_built_count = 0u;
static char s_sel_ssid[33];

static lv_obj_t *s_popup = NULL;
static lv_obj_t *s_popup_ta = NULL;
static lv_obj_t *s_popup_btn_conn = NULL;
static lv_obj_t *s_popup_btn_cancel = NULL;
static lv_obj_t *s_btn_refresh = NULL;
static lv_obj_t *s_btn_refresh_label = NULL;
static lv_obj_t *s_wifi_cursor = NULL;
static uint32_t s_wifi_cursor_last_ms = 0u;
static uint8_t s_wifi_cursor_visible = 1u;
static char s_pending_ssid[33];

static uint8_t s_connecting_row = 0xFFu;

#define WIFI_SCAN_LABEL_X        6
#define WIFI_SCAN_LABEL_Y        78
#define WIFI_SCAN_DOTS_GAP_PX    2
#define WIFI_SCAN_DOTS_PERIOD_MS 400u

static uint8_t s_scan_dots_anim_on = 0u;
static uint8_t s_scan_dots_count = 0u;
static uint32_t s_scan_dots_last_ms = 0u;
static lv_obj_t *s_wifi_empty_hint = NULL;
static uint32_t s_last_list_merge_ms = 0u;
#define WIFI_LIST_MERGE_MIN_MS  200u
#define WIFI_LIST_MERGE_HOLD_MS 600u
static uint32_t s_merge_hold_until_ms = 0u;
static volatile uint8_t s_force_list_merge = 0u;
static uint8_t s_gui_wake = 0u;

static void screen_wifi_layout_scan_row(void);
static void screen_wifi_scan_dots_apply_text(uint8_t n);
static void screen_wifi_scan_dots_anim_reset(void);
static void screen_wifi_scan_dots_anim_tick(void);
static void screen_wifi_show_scanning(void);
static void screen_wifi_show_connecting(void);
static void screen_wifi_show_connect_result(uint8_t ok);
static void screen_wifi_update_connected_bar(void);

static ui_nav_ctx_t build_nav_ctx(void)
{
    ui_nav_ctx_t ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.ui = &guider_ui;
    ctx.app_scr = &g_app_scr;
    ctx.screen3_need_init = &g_screen3_need_init;
    ctx.screen3_pending_index = &g_screen3_pending_index;
    return ctx;
}

uint8_t screen_wifi_popup_is_active(void)
{
    return (s_popup != NULL && lv_obj_is_valid(s_popup)) ? 1u : 0u;
}

static void screen_wifi_popup_restore_background(void)
{
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan)) {
        lv_obj_clear_flag(guider_ui.screen_11_label_scan, LV_OBJ_FLAG_HIDDEN);
    }
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan_dots)) {
        lv_obj_clear_flag(guider_ui.screen_11_label_scan_dots, LV_OBJ_FLAG_HIDDEN);
    }
    screen_wifi_update_connected_bar();
}

static void screen_wifi_popup_hide_background(void)
{
    if(lv_obj_is_valid(guider_ui.screen_11_row_connected)) {
        lv_obj_add_flag(guider_ui.screen_11_row_connected, LV_OBJ_FLAG_HIDDEN);
    }
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan)) {
        lv_obj_add_flag(guider_ui.screen_11_label_scan, LV_OBJ_FLAG_HIDDEN);
    }
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan_dots)) {
        lv_obj_add_flag(guider_ui.screen_11_label_scan_dots, LV_OBJ_FLAG_HIDDEN);
    }
}

static void screen_wifi_popup_close(void)
{
    uint8_t had_popup = screen_wifi_popup_is_active();

    if(s_popup != NULL && lv_obj_is_valid(s_popup)) {
        lv_obj_del(s_popup);
    }
    s_popup = NULL;
    s_popup_ta = NULL;
    s_popup_btn_conn = NULL;
    s_popup_btn_cancel = NULL;
    s_wifi_cursor = NULL;
    if(had_popup != 0u) {
        screen_wifi_popup_restore_background();
    }
}

static int8_t screen_wifi_find_row_by_ssid(const char *ssid);
static void screen_wifi_set_selected(uint8_t idx);
static void screen_wifi_refresh_btn_ensure(void);
static void screen_wifi_refresh_btn_raise(void);
static void screen_wifi_rebuild_list_from_scan(const char *conn_ssid);
static void screen_wifi_list_panel_setup(void);
static uint8_t screen_wifi_scan_in_progress(void);
static void screen_wifi_update_scan_hint(void);

static void screen_wifi_update_cursor_pos(void)
{
    if(s_popup_ta != NULL && lv_obj_is_valid(s_popup_ta)) {
        ui_auth_update_cursor_pos(s_wifi_cursor, s_popup_ta);
    }
}

static lv_obj_t *screen_wifi_popup_make_btn(lv_obj_t *parent, const char *txt, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *btn;
    lv_obj_t *lbl;

    btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, 88, 28);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lbl = lv_label_create(btn);
    lv_label_set_text(lbl, txt);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_25, LV_PART_MAIN | LV_STATE_DEFAULT);
    return btn;
}

static void screen_wifi_refresh_btn_ensure(void)
{
    lv_obj_t *parent;
    lv_font_glyph_dsc_t gd;
    uint8_t have_refresh_chars = 0u;
    uint8_t have_scan_chars = 0u;

    parent = guider_ui.screen_11_cont_1;
    if(parent == NULL || !lv_obj_is_valid(parent)) {
        return;
    }
    if(s_btn_refresh != NULL) {
        if(lv_obj_is_valid(s_btn_refresh) && lv_obj_get_parent(s_btn_refresh) == parent) {
            screen_wifi_refresh_btn_raise();
            return;
        }
        s_btn_refresh = NULL;
        s_btn_refresh_label = NULL;
    }

    s_btn_refresh = lv_btn_create(parent);
    s_btn_refresh_label = lv_label_create(s_btn_refresh);
    lv_obj_align(s_btn_refresh_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(s_btn_refresh, 0, LV_STATE_DEFAULT);
    lv_obj_set_pos(s_btn_refresh, WIFI_REFRESH_BTN_X, WIFI_REFRESH_BTN_Y);
    lv_obj_set_size(s_btn_refresh, WIFI_REFRESH_BTN_W, WIFI_REFRESH_BTN_H);
    lv_obj_set_style_bg_opa(s_btn_refresh, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_btn_refresh, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(s_btn_refresh, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_btn_refresh, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s_btn_refresh, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(s_btn_refresh, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_btn_refresh_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_btn_refresh_label, &lv_font_SourceHanSerifSC_Regular_25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(s_btn_refresh_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(s_btn_refresh_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(s_btn_refresh_label, LV_PCT(100));

    /* 检查字库是否包含“刷新”（U+5237/U+65B0）；缺字时退化为“扫描”。 */
    if(lv_font_get_glyph_dsc(&lv_font_SourceHanSerifSC_Regular_25, &gd, 0x5237u, 0u) &&
       lv_font_get_glyph_dsc(&lv_font_SourceHanSerifSC_Regular_25, &gd, 0x65B0u, 0u)) {
        have_refresh_chars = 1u;
    }
    if(lv_font_get_glyph_dsc(&lv_font_SourceHanSerifSC_Regular_25, &gd, 0x626Bu, 0u) &&
       lv_font_get_glyph_dsc(&lv_font_SourceHanSerifSC_Regular_25, &gd, 0x63CFu, 0u)) {
        have_scan_chars = 1u;
    }
    lv_label_set_text(s_btn_refresh_label,
                      have_refresh_chars ? "\xe5\x88\xb7\xe6\x96\xb0" :
                      (have_scan_chars ? "\xe6\x89\xab\xe6\x8f\x8f" : "SCAN"));
    screen_wifi_refresh_btn_raise();
}

static void screen_wifi_refresh_btn_raise(void)
{
    if(s_btn_refresh != NULL && lv_obj_is_valid(s_btn_refresh)) {
        lv_obj_move_foreground(s_btn_refresh);
    }
}

static lv_coord_t screen_wifi_list_scroll_max_y(void)
{
    lv_obj_t *panel = guider_ui.screen_11_list_panel;
    lv_coord_t view_h;
    lv_coord_t content_h;

    if(panel == NULL || !lv_obj_is_valid(panel) || s_list_built_count == 0u) {
        return 0;
    }
    view_h = lv_obj_get_height(panel);
    content_h = (lv_coord_t)s_list_built_count * WIFI_ROW_H;
    if(content_h <= view_h) {
        return 0;
    }
    return content_h - view_h;
}

static void screen_wifi_list_panel_setup(void)
{
    lv_obj_t *panel = guider_ui.screen_11_list_panel;

    if(panel == NULL || !lv_obj_is_valid(panel)) {
        return;
    }
    lv_obj_set_pos(panel, 0, WIFI_LIST_PANEL_Y);
    lv_obj_set_size(panel, 240, WIFI_LIST_PANEL_H);
    lv_obj_set_layout(panel, 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_scroll_to_y(panel, 0, LV_ANIM_OFF);
}

uint8_t screen_wifi_list_lvgl_scrolling(void)
{
    return 0u;
}

void screen_wifi_list_touch_begin(void)
{
    (void)0;
}

static void screen_wifi_popup_open(const char *ssid)
{
    lv_obj_t *parent;
    lv_obj_t *lbl;

    if(!lv_obj_is_valid(guider_ui.screen_11_cont_1)) {
        return;
    }
    screen_wifi_popup_close();
    app_wifi_remember_popup_open();
    strncpy(s_pending_ssid, (ssid != NULL) ? ssid : "", sizeof(s_pending_ssid) - 1u);
    s_pending_ssid[sizeof(s_pending_ssid) - 1u] = '\0';

    parent = guider_ui.screen_11_cont_1;
    s_popup = lv_obj_create(parent);
    lv_obj_set_size(s_popup, 220, 156);
    lv_obj_align(s_popup, LV_ALIGN_CENTER, 0, -10);
    lv_obj_clear_flag(s_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_popup, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_popup, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_popup, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_popup, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(s_popup, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s_popup, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl = lv_label_create(s_popup);
    lv_label_set_text(lbl, "WIFI\xe5\xaf\x86\xe7\xa0\x81\xef\xbc\x9a");
    lv_obj_set_pos(lbl, 10, 8);
    lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_25, LV_PART_MAIN | LV_STATE_DEFAULT);

    s_popup_ta = lv_textarea_create(s_popup);
    lv_textarea_set_text(s_popup_ta, "");
    lv_textarea_set_one_line(s_popup_ta, true);
    lv_textarea_set_password_mode(s_popup_ta, true);
    lv_textarea_set_password_bullet(s_popup_ta, "*");
    lv_textarea_set_max_length(s_popup_ta, 64);
    lv_obj_set_pos(s_popup_ta, 10, 40);
    lv_obj_set_size(s_popup_ta, 200, 36);
    lv_obj_set_style_bg_color(s_popup_ta, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_popup_ta, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_popup_ta, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_popup_ta, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_textarea_apply_input_style(s_popup_ta, 1u);
    ui_cursor_attach_bar(&s_wifi_cursor, s_popup_ta);
    ui_cursor_activate(s_wifi_cursor, &s_wifi_cursor_visible, &s_wifi_cursor_last_ms);

    screen_wifi_popup_hide_background();

    s_popup_btn_conn = screen_wifi_popup_make_btn(s_popup,
                                                  "\xe8\xbf\x9e\xe6\x8e\xa5", 10, 112);
    s_popup_btn_cancel = screen_wifi_popup_make_btn(s_popup,
                                                    "\xe5\x8f\x96\xe6\xb6\x88", 118, 112);
    lv_obj_move_foreground(s_popup);
}

static uint8_t screen_wifi_obj_hit(lv_obj_t *obj, lv_coord_t x, lv_coord_t y)
{
    lv_area_t a;
    lv_point_t p;

    if(obj == NULL || !lv_obj_is_valid(obj)) {
        return 0u;
    }
    lv_obj_get_coords(obj, &a);
    p.x = x;
    p.y = y;
    return _lv_area_is_point_on(&a, &p, WIFI_TOUCH_HIT_PAD) ? 1u : 0u;
}

uint8_t screen_wifi_popup_hit_btn(lv_coord_t x, lv_coord_t y)
{
    if(!screen_wifi_popup_is_active()) {
        return 0u;
    }
    if(screen_wifi_obj_hit(s_popup_btn_conn, x, y) != 0u) {
        return 1u;
    }
    if(screen_wifi_obj_hit(s_popup_btn_cancel, x, y) != 0u) {
        return 2u;
    }
    return 0u;
}

uint8_t screen_wifi_popup_tap_outside_ta(lv_coord_t x, lv_coord_t y)
{
    lv_area_t a;
    lv_point_t p;

    if(!screen_wifi_popup_is_active()) {
        return 0u;
    }
    if(s_popup_ta != NULL && lv_obj_is_valid(s_popup_ta)) {
        lv_obj_get_coords(s_popup_ta, &a);
        p.x = x;
        p.y = y;
        if(_lv_area_is_point_on(&a, &p, 4)) {
            return 0u;
        }
    }
    if(s_popup != NULL && lv_obj_is_valid(s_popup)) {
        lv_obj_get_coords(s_popup, &a);
        p.x = x;
        p.y = y;
        if(_lv_area_is_point_on(&a, &p, 4)) {
            return 1u;
        }
    }
    return 0u;
}

void screen_wifi_exit_cleanup(void)
{
    WIFI_DBG("exit_cleanup rows=%u popup=%u conn_row=%u",
             (unsigned)s_list_built_count,
             (unsigned)screen_wifi_popup_is_active(),
             (unsigned)s_connecting_row);
    screen_wifi_popup_close();
    app_touch_wifi_reset();
    s_scan_dots_anim_on = 0u;
    s_connecting_row = 0xFFu;
    s_pending_ssid[0] = '\0';
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(app_wifi_connect_busy() != 0u) {
        if(cloud_aliyun_at_wifi_link_ready() != 0u) {
            app_wifi_connect_reset();
        } else if(cloud_aliyun_at_wifi_bringup_active() == 0u) {
            app_wifi_connect_reset();
        }
    }
    app_wifi_scan_scr11_leave();
    cloud_aliyun_at_scr11_leave();
#endif
}

void screen_wifi_exit_to_menu(void)
{
    extern uint8_t g_screen3_need_init;
    extern uint8_t g_screen3_pending_index;

    WIFI_DBG("exit_to_menu scr=%u nav_busy=%u", (unsigned)g_app_scr, (unsigned)app_ui_nav_is_busy());
    screen_wifi_exit_cleanup();
    if(g_app_scr != APP_SCR_11) {
        return;
    }
    app_ui_nav_begin((uint8_t)APP_SCR_11);
    ui_load_scr_animation(&guider_ui, &guider_ui.screen_3, guider_ui.screen_3_del, &guider_ui.screen_11_del,
                          setup_scr_screen_3, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_3;
    g_screen3_pending_index = 4u;
    g_screen3_need_init = 1u;
    app_ui_nav_end((uint8_t)APP_SCR_3);
    WIFI_DBG("exit_to_menu done scr=%u", (unsigned)g_app_scr);
}

void screen_wifi_popup_cancel(void)
{
    screen_wifi_popup_close();
}

void screen_wifi_popup_confirm(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    const char *pwd_raw;
    char pwd_buf[65];
    int8_t row_idx;

    if(s_pending_ssid[0] == '\0' || !lv_obj_is_valid(s_popup_ta)) {
        screen_wifi_popup_close();
        return;
    }
    /* 必须先拷贝密码再关弹窗：popup_close 会 lv_obj_del 掉 textarea */
    pwd_raw = lv_textarea_get_text(s_popup_ta);
    pwd_buf[0] = '\0';
    if(pwd_raw != NULL) {
        strncpy(pwd_buf, pwd_raw, sizeof(pwd_buf) - 1u);
        pwd_buf[sizeof(pwd_buf) - 1u] = '\0';
    }
    if(screen_wifi_scan_in_progress() != 0u) {
        WIFI_DBG("popup confirm blocked: scan not finished");
        return;
    }
    screen_wifi_popup_close();

    row_idx = screen_wifi_find_row_by_ssid(s_pending_ssid);
    if(row_idx >= 0) {
        s_sel_index = (uint8_t)row_idx;
        screen_wifi_set_selected(s_sel_index);
    }
    app_wifi_remember_manual_begin(s_pending_ssid);
    app_wifi_cfg_set(s_pending_ssid, pwd_buf);
    WIFI_DBG("popup confirm ssid=%s pwd_len=%u", s_pending_ssid, (unsigned)strlen(pwd_buf));
    app_wifi_connect_start(s_pending_ssid, pwd_buf);
    screen_wifi_show_connecting();
    screen_wifi_scan_dots_anim_reset();
    screen_wifi_gui_wake();
#else
    screen_wifi_popup_close();
#endif
}

void screen_wifi_handle_popup_input(KeyValue_t key)
{
    if(!lv_obj_is_valid(s_popup_ta)) return;
    ui_auth_handle_input_key(s_popup_ta, key, 64u, NULL, screen_wifi_update_cursor_pos);
}

static void screen_wifi_clear_rows(void)
{
    uint8_t i;

    for(i = 0u; i < WIFI_ROW_MAX; i++) {
        if(s_rows[i].row != NULL && lv_obj_is_valid(s_rows[i].row)) {
            lv_obj_del(s_rows[i].row);
        }
        memset(&s_rows[i], 0, sizeof(s_rows[i]));
        s_row_last_seen_ms[i] = 0u;
    }
    s_list_built_count = 0u;
    s_wifi_empty_hint = NULL;
    if(lv_obj_is_valid(guider_ui.screen_11_list_panel)) {
        lv_obj_clean(guider_ui.screen_11_list_panel);
    }
}

static void screen_wifi_hide_empty_hint(void)
{
    if(s_wifi_empty_hint != NULL && lv_obj_is_valid(s_wifi_empty_hint)) {
        lv_obj_add_flag(s_wifi_empty_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

/* 扫描完成前隐藏列表区，避免半成品行闪一下「口」 */
static void screen_wifi_list_conceal_for_scan(void)
{
    screen_wifi_clear_rows();
    screen_wifi_hide_empty_hint();
    if(lv_obj_is_valid(guider_ui.screen_11_list_panel)) {
        lv_obj_add_flag(guider_ui.screen_11_list_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void screen_wifi_list_reveal(void)
{
    if(lv_obj_is_valid(guider_ui.screen_11_list_panel)) {
        lv_obj_clear_flag(guider_ui.screen_11_list_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void screen_wifi_show_empty_hint(void)
{
    lv_obj_t *panel;

    if(!lv_obj_is_valid(guider_ui.screen_11_list_panel)) {
        return;
    }
    panel = guider_ui.screen_11_list_panel;
    if(s_wifi_empty_hint == NULL || !lv_obj_is_valid(s_wifi_empty_hint)) {
        s_wifi_empty_hint = lv_label_create(panel);
        lv_label_set_text(s_wifi_empty_hint, "\xe6\x9a\x82\xe6\x97\xa0\xe5\x8f\xaf\xe9\x80\x89\xe7\x83\xad\xe7\x82\xb9");
        lv_obj_set_style_text_font(s_wifi_empty_hint, &lv_font_SourceHanSerifSC_Regular_21,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(s_wifi_empty_hint, lv_color_hex(0x666666),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_clear_flag(s_wifi_empty_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_wifi_empty_hint);
}

static void screen_wifi_style_row(uint8_t idx, uint8_t selected)
{
    if(idx >= WIFI_ROW_MAX || s_rows[idx].row == NULL) {
        return;
    }
    if(selected) {
        lv_obj_set_style_bg_opa(s_rows[idx].row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(s_rows[idx].row, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(s_rows[idx].lbl_name, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        if(s_rows[idx].lbl_icon != NULL) {
            lv_obj_set_style_text_color(s_rows[idx].lbl_icon, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    } else {
        lv_obj_set_style_bg_opa(s_rows[idx].row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(s_rows[idx].row, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(s_rows[idx].lbl_name, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        if(s_rows[idx].lbl_icon != NULL) {
            lv_obj_set_style_text_color(s_rows[idx].lbl_icon, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

static void screen_wifi_show_enter_idle_hint(void)
{
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan)) {
        lv_obj_clear_flag(guider_ui.screen_11_label_scan, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(guider_ui.screen_11_label_scan, &lv_font_SourceHanSerifSC_Regular_25,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(guider_ui.screen_11_label_scan, "\xe8\xaf\xb7\xe6\x8c\x89\xe6\x89\xab\xe6\x8f\x8f\xe5\x88\xb7\xe6\x96\xb0");
    }
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan_dots)) {
        lv_label_set_text(guider_ui.screen_11_label_scan_dots, "");
        lv_obj_clear_flag(guider_ui.screen_11_label_scan_dots, LV_OBJ_FLAG_HIDDEN);
    }
    s_scan_dots_anim_on = 0u;
}

static void screen_wifi_update_connected_bar(void)
{
    char ssid_buf[33];

    if(!lv_obj_is_valid(guider_ui.screen_11_row_connected)) {
        return;
    }
    /* 仅 STA 已拿到 IP 时显示；wifi_joined() 在 scr11 上常为真会误显示 */
    if(cloud_aliyun_at_wifi_link_ready() == 0u) {
        lv_obj_add_flag(guider_ui.screen_11_row_connected, LV_OBJ_FLAG_HIDDEN);
        WIFI_DBG("conn bar hide: link_ready=0");
        return;
    }
    ssid_buf[0] = '\0';
    /* 禁止在 GuiTask 里发 AT+CWJAP?：会与 CloudTask 抢 UART2 导致整屏卡死 */
    (void)cloud_aliyun_at_get_connected_ssid(ssid_buf, sizeof(ssid_buf));
    if(ssid_buf[0] == '\0') {
        lv_obj_add_flag(guider_ui.screen_11_row_connected, LV_OBJ_FLAG_HIDDEN);
        WIFI_DBG("conn bar hide: link_ready=1 no ssid");
        return;
    }
    lv_obj_clear_flag(guider_ui.screen_11_row_connected, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(guider_ui.screen_11_lbl_conn_ssid, ssid_buf);
    WIFI_DBG("conn bar show ssid=%s", ssid_buf);
    if(lv_obj_is_valid(guider_ui.screen_11_lbl_conn_sta)) {
        lv_obj_set_style_text_font(guider_ui.screen_11_lbl_conn_sta, &lv_font_cn_wifi_16,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(guider_ui.screen_11_lbl_conn_sta, "\xe5\xb7\xb2\xe8\xbf\x9e\xe6\x8e\xa5");
        lv_obj_clear_flag(guider_ui.screen_11_lbl_conn_sta, LV_OBJ_FLAG_HIDDEN);
    }
}

static const lv_font_t *screen_wifi_ssid_font(const char *ssid)
{
    const uint8_t *p;

    if(ssid == NULL) {
        return &lv_font_montserratMedium_16;
    }
    p = (const uint8_t *)ssid;
    while(*p != 0u) {
        if(*p >= 0x80u) {
            return &lv_font_SourceHanSerifSC_Regular_21;
        }
        p++;
    }
    return &lv_font_montserratMedium_16;
}

static void screen_wifi_save_sel_ssid(void)
{
    const char *t;

    if(s_sel_index < s_list_built_count && s_rows[s_sel_index].lbl_name != NULL) {
        t = lv_label_get_text(s_rows[s_sel_index].lbl_name);
        if(t != NULL) {
            strncpy(s_sel_ssid, t, sizeof(s_sel_ssid) - 1u);
            s_sel_ssid[sizeof(s_sel_ssid) - 1u] = '\0';
            return;
        }
    }
    s_sel_ssid[0] = '\0';
}

static int8_t screen_wifi_find_row_by_ssid(const char *ssid)
{
    uint8_t i;

    if(ssid == NULL || ssid[0] == '\0') {
        return -1;
    }
    for(i = 0u; i < s_list_built_count; i++) {
        const char *t;
        if(s_rows[i].lbl_name == NULL) {
            continue;
        }
        t = lv_label_get_text(s_rows[i].lbl_name);
        if(t != NULL && strcmp(t, ssid) == 0) {
            return (int8_t)i;
        }
    }
    return -1;
}

static uint8_t screen_wifi_scan_has_ssid(const char *ssid)
{
    uint8_t i;
    uint8_t n = app_wifi_scan_count();

    for(i = 0u; i < n; i++) {
        const app_wifi_ap_t *ap = app_wifi_scan_get(i);
        if(ap != NULL && strcmp(ap->ssid, ssid) == 0) {
            return 1u;
        }
    }
    return 0u;
}

static void screen_wifi_remove_row(uint8_t idx)
{
    uint8_t i;

    if(idx >= s_list_built_count) {
        return;
    }
    if(s_rows[idx].row != NULL && lv_obj_is_valid(s_rows[idx].row)) {
        lv_obj_del(s_rows[idx].row);
    }
    for(i = idx; i < (uint8_t)(s_list_built_count - 1u); i++) {
        s_rows[i] = s_rows[i + 1u];
        s_row_last_seen_ms[i] = s_row_last_seen_ms[i + 1u];
    }
    memset(&s_rows[s_list_built_count - 1u], 0, sizeof(s_rows[0]));
    s_row_last_seen_ms[s_list_built_count - 1u] = 0u;
    s_list_built_count--;
    if(s_list_built_count == 0u) {
        s_sel_index = 0u;
        s_sel_ssid[0] = '\0';
    } else if(s_sel_index >= s_list_built_count) {
        s_sel_index = (uint8_t)(s_list_built_count - 1u);
    }
}

static uint8_t screen_wifi_append_row(const char *ssid)
{
    lv_obj_t *panel;
    lv_obj_t *row_obj;
    lv_obj_t *icon;
    lv_obj_t *name;
    char line[48];
    uint8_t idx;

    if(ssid == NULL || ssid[0] == '\0' || s_list_built_count >= WIFI_ROW_MAX) {
        WIFI_DBG("UI append fail ssid=%p cnt=%u", (const void *)ssid, (unsigned)s_list_built_count);
        return 0u;
    }
    if(!lv_obj_is_valid(guider_ui.screen_11_list_panel)) {
        WIFI_DBG("UI append fail: list_panel invalid");
        return 0u;
    }
    panel = guider_ui.screen_11_list_panel;
    idx = s_list_built_count;
    row_obj = lv_obj_create(panel);
    lv_obj_set_pos(row_obj, 0, (lv_coord_t)(idx * WIFI_ROW_H));
    lv_obj_set_size(row_obj, 240, WIFI_ROW_H);
    lv_obj_set_style_pad_all(row_obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(row_obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(row_obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(row_obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(row_obj, LV_OBJ_FLAG_SCROLLABLE);
    /* 行点击：app_touch 抬起命中；列表滚动：LVGL 原生（lv_port_indev） */

    icon = lv_label_create(row_obj);
    lv_label_set_text(icon, LV_SYMBOL_WIFI);
    lv_obj_set_pos(icon, 4, 6);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);

    name = lv_label_create(row_obj);
    snprintf(line, sizeof(line), "%s", ssid);
    lv_label_set_text(name, line);
    lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(name, WIFI_LIST_NAME_W);
    lv_obj_set_pos(name, WIFI_LIST_NAME_X, 6);
    lv_obj_set_style_text_font(name, screen_wifi_ssid_font(ssid),
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(name, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);

    s_rows[idx].row = row_obj;
    s_rows[idx].lbl_icon = icon;
    s_rows[idx].lbl_name = name;
    s_rows[idx].lbl_status = NULL;
    s_rows[idx].status = WIFI_ROW_ST_NONE;
    s_row_last_seen_ms[idx] = lv_tick_get();
    s_list_built_count++;
    return 1u;
}

/* 滚动边界：仅用行数×行高计算，避免 flex/scroll_bottom 为 0 导致滑到底回弹 */
static void screen_wifi_clamp_list_scroll(void)
{
    lv_obj_t *panel;
    lv_coord_t y;
    lv_coord_t max_y;

    panel = guider_ui.screen_11_list_panel;
    if(panel == NULL || !lv_obj_is_valid(panel)) {
        return;
    }
    max_y = screen_wifi_list_scroll_max_y();
    y = lv_obj_get_scroll_y(panel);
    if(y < 0) {
        lv_obj_scroll_to_y(panel, 0, LV_ANIM_OFF);
    } else if(max_y > 0 && y > max_y) {
        lv_obj_scroll_to_y(panel, max_y, LV_ANIM_OFF);
    } else if(max_y <= 0 && y != 0) {
        lv_obj_scroll_to_y(panel, 0, LV_ANIM_OFF);
    }
}

static void screen_wifi_ensure_sel_visible(void)
{
    lv_obj_t *row;
    lv_obj_t *panel;
    lv_area_t pa;
    lv_area_t ra;

    if(s_sel_index >= s_list_built_count) {
        return;
    }
    row = s_rows[s_sel_index].row;
    panel = guider_ui.screen_11_list_panel;
    if(row == NULL || !lv_obj_is_valid(row) || !lv_obj_is_valid(panel)) {
        return;
    }
    lv_obj_get_coords(panel, &pa);
    lv_obj_get_coords(row, &ra);
    if(ra.y1 < pa.y1 || ra.y2 > pa.y2) {
        lv_obj_scroll_to_view(row, LV_ANIM_OFF);
        screen_wifi_clamp_list_scroll();
    }
}

static void screen_wifi_set_selected_ex(uint8_t idx, uint8_t scroll_into_view)
{
    uint8_t i;

    if(s_list_built_count == 0u) {
        s_sel_index = 0u;
        s_sel_ssid[0] = '\0';
        return;
    }
    if(idx >= s_list_built_count) {
        idx = (uint8_t)(s_list_built_count - 1u);
    }
    s_sel_index = idx;
    screen_wifi_save_sel_ssid();
    for(i = 0u; i < s_list_built_count; i++) {
        screen_wifi_style_row(i, (i == s_sel_index) ? 1u : 0u);
    }
    if(scroll_into_view != 0u) {
        screen_wifi_ensure_sel_visible();
    }
}

static void screen_wifi_set_selected(uint8_t idx)
{
    /* 触摸点选不自动 scroll_to_view，避免滑到底后被拉回顶部 */
    screen_wifi_set_selected_ex(idx, 0u);
}

static uint8_t screen_wifi_scan_pass_done(void)
{
    return (app_wifi_scan_pass_completed() != 0u && !app_wifi_scan_busy() &&
            !app_wifi_scan_has_pending()) ? 1u : 0u;
}

static uint8_t screen_wifi_scan_in_progress(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    return (app_wifi_scan_busy() != 0u || app_wifi_scan_has_pending() != 0u) ? 1u : 0u;
#else
    return 0u;
#endif
}

static void screen_wifi_rebuild_list_from_scan(const char *conn_ssid)
{
    uint8_t i;
    uint8_t n = app_wifi_scan_count();
    int8_t restore_idx;

    screen_wifi_clear_rows();
    for(i = 0u; i < n; i++) {
        const app_wifi_ap_t *ap = app_wifi_scan_get(i);
        if(ap == NULL || ap->ssid[0] == '\0') {
            continue;
        }
        if(conn_ssid != NULL && conn_ssid[0] != '\0' && strcmp(ap->ssid, conn_ssid) == 0) {
            continue;
        }
        (void)screen_wifi_append_row(ap->ssid);
    }

    if(s_list_built_count == 0u) {
        s_sel_index = 0u;
        s_sel_ssid[0] = '\0';
        screen_wifi_show_empty_hint();
        return;
    }

    screen_wifi_hide_empty_hint();
    restore_idx = screen_wifi_find_row_by_ssid(s_sel_ssid);
    if(restore_idx < 0) {
        restore_idx = (s_sel_index < s_list_built_count) ? (int8_t)s_sel_index : 0;
    }
    screen_wifi_set_selected_ex((uint8_t)restore_idx, 0u);
    screen_wifi_clamp_list_scroll();
    if(lv_obj_is_valid(guider_ui.screen_11_list_panel)) {
        lv_obj_update_layout(guider_ui.screen_11_list_panel);
    }
}

static void screen_wifi_sync_list_scroll_range(void)
{
    lv_obj_t *panel = guider_ui.screen_11_list_panel;

    if(panel == NULL || !lv_obj_is_valid(panel) || s_list_built_count == 0u) {
        return;
    }
    lv_obj_update_layout(panel);
    screen_wifi_clamp_list_scroll();
}

static uint8_t screen_wifi_should_merge_list_now(void)
{
    uint32_t min_gap = WIFI_LIST_MERGE_MIN_MS;
    uint32_t now_ms = lv_tick_get();

    if(app_wifi_scan_peek_list_dirty() == 0u) {
        return 0u;
    }
    /* 仅扫描完整结束后再合并，避免扫描中途增量刷出乱码行 */
    if(screen_wifi_scan_pass_done() == 0u) {
        return 0u;
    }
    if((int32_t)(now_ms - s_merge_hold_until_ms) < 0) {
        return 0u;
    }
    if(screen_wifi_scan_pass_done() != 0u) {
        return 1u;
    }
    if(app_wifi_scan_busy() != 0u) {
        min_gap = WIFI_LIST_MERGE_SCROLL_MS;
    }
    if(s_last_list_merge_ms == 0u ||
       lv_tick_elaps(s_last_list_merge_ms) >= min_gap) {
        return 1u;
    }
    return 0u;
}

static lv_coord_t screen_wifi_list_scroll_capture(void)
{
    lv_obj_t *panel = guider_ui.screen_11_list_panel;

    if(panel == NULL || !lv_obj_is_valid(panel)) {
        return 0;
    }
    return lv_obj_get_scroll_y(panel);
}

static void screen_wifi_list_scroll_restore(lv_coord_t y)
{
    lv_obj_t *panel = guider_ui.screen_11_list_panel;
    lv_coord_t max_y;

    if(panel == NULL || !lv_obj_is_valid(panel)) {
        return;
    }
    max_y = screen_wifi_list_scroll_max_y();
    if(y < 0) {
        y = 0;
    } else if(y > max_y) {
        y = max_y;
    }
    lv_obj_scroll_to_y(panel, y, LV_ANIM_OFF);
}

static void screen_wifi_merge_list(void)
{
    uint8_t n;
    char conn_ssid[33];
    uint8_t scan_done;
    lv_coord_t scroll_keep = 0;

    if(!lv_obj_is_valid(guider_ui.screen_11_list_panel)) {
        WIFI_DBG("UI merge abort: list_panel invalid");
        return;
    }

    /* 输入密码弹窗期间，不允许列表刷新/重排，避免卡顿和“WiFi 消失”。 */
    if(screen_wifi_popup_is_active()) {
        return;
    }

    scroll_keep = screen_wifi_list_scroll_capture();
    screen_wifi_save_sel_ssid();
    scan_done = screen_wifi_scan_pass_done();
    n = app_wifi_scan_count();
    printf("[WiFi] UI merge start ap=%u built=%u scan_done=%u\r\n",
           (unsigned)n, (unsigned)s_list_built_count, (unsigned)scan_done);
    WIFI_DBG("UI merge start scan_ap=%u", (unsigned)n);
    screen_wifi_hide_empty_hint();

    /* 扫描未完成时不刷新列表，避免先闪几行「口」再整表重建 */
    if(scan_done == 0u) {
        return;
    }

    if(app_wifi_scan_last_failed() != 0u) {
        screen_wifi_clear_rows();
        screen_wifi_hide_empty_hint();
        screen_wifi_update_scan_hint();
        screen_wifi_refresh_btn_raise();
        s_force_list_merge = 0u;
        return;
    }
    if(n == 0u) {
        screen_wifi_clear_rows();
        screen_wifi_list_reveal();
        screen_wifi_update_scan_hint();
        screen_wifi_refresh_btn_raise();
        s_force_list_merge = 0u;
        return;
    }

    conn_ssid[0] = '\0';
    if(cloud_aliyun_at_wifi_link_ready() != 0u) {
        (void)cloud_aliyun_at_get_connected_ssid(conn_ssid, sizeof(conn_ssid));
    }

    screen_wifi_rebuild_list_from_scan(conn_ssid);
    screen_wifi_list_reveal();
    screen_wifi_list_scroll_restore(scroll_keep);
    screen_wifi_sync_list_scroll_range();
    screen_wifi_refresh_btn_raise();
    WIFI_DBG("UI merge done rows=%u sel=%u (rebuild only)",
             (unsigned)s_list_built_count, (unsigned)s_sel_index);
    s_force_list_merge = 0u;
}

static void screen_wifi_scan_dots_apply_text(uint8_t n)
{
    static const char *const k_dots[] = { "", ".", "..", "..." };

    if(!lv_obj_is_valid(guider_ui.screen_11_label_scan_dots)) {
        return;
    }
    if(n > 3u) {
        n = 3u;
    }
    lv_obj_clear_flag(guider_ui.screen_11_label_scan_dots, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(guider_ui.screen_11_label_scan_dots,
                               &lv_font_SourceHanSerifSC_Regular_25,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(guider_ui.screen_11_label_scan_dots, k_dots[n]);
}

static void screen_wifi_layout_scan_row(void)
{
    if(!lv_obj_is_valid(guider_ui.screen_11_label_scan) ||
       !lv_obj_is_valid(guider_ui.screen_11_label_scan_dots)) {
        return;
    }
    /* 与 Guider 一致：主文案锚点在 (6,78)，省略号紧跟文字右侧 */
    lv_obj_set_pos(guider_ui.screen_11_label_scan, WIFI_SCAN_LABEL_X, WIFI_SCAN_LABEL_Y);
    lv_obj_set_width(guider_ui.screen_11_label_scan, LV_SIZE_CONTENT);
    lv_obj_set_height(guider_ui.screen_11_label_scan, LV_SIZE_CONTENT);
    lv_obj_set_width(guider_ui.screen_11_label_scan_dots, LV_SIZE_CONTENT);
    lv_obj_set_height(guider_ui.screen_11_label_scan_dots, LV_SIZE_CONTENT);
    lv_obj_update_layout(guider_ui.screen_11_label_scan);
    lv_obj_align_to(guider_ui.screen_11_label_scan_dots, guider_ui.screen_11_label_scan,
                    LV_ALIGN_OUT_RIGHT_MID, WIFI_SCAN_DOTS_GAP_PX, 0);
}

static void screen_wifi_scan_dots_anim_reset(void)
{
    s_scan_dots_count = 0u;
    s_scan_dots_last_ms = lv_tick_get();
    s_scan_dots_anim_on = 1u;
    screen_wifi_scan_dots_apply_text(0u);
    screen_wifi_layout_scan_row();
}

static void screen_wifi_scan_dots_idle(void)
{
    s_scan_dots_anim_on = 0u;
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan_dots)) {
        lv_obj_clear_flag(guider_ui.screen_11_label_scan_dots, LV_OBJ_FLAG_HIDDEN);
        screen_wifi_scan_dots_apply_text(3u);
        screen_wifi_layout_scan_row();
    }
}

static void screen_wifi_scan_dots_anim_tick(void)
{
    if(!s_scan_dots_anim_on || g_app_scr != APP_SCR_11) {
        return;
    }
    if(!lv_obj_is_valid(guider_ui.screen_11_label_scan_dots)) {
        return;
    }
    if(lv_tick_elaps(s_scan_dots_last_ms) < WIFI_SCAN_DOTS_PERIOD_MS) {
        return;
    }
    s_scan_dots_last_ms = lv_tick_get();

    if(s_scan_dots_count < 3u) {
        s_scan_dots_count++;
        screen_wifi_scan_dots_apply_text(s_scan_dots_count);
    } else {
        s_scan_dots_count = 0u;
        screen_wifi_scan_dots_apply_text(0u);
    }
    screen_wifi_layout_scan_row();
}

static uint8_t screen_wifi_hint_is_scanning(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(g_app_scr != APP_SCR_11 || screen_wifi_popup_is_active()) {
        return 0u;
    }
    return (app_wifi_scan_busy() != 0u) ? 1u : 0u;
#else
    return 0u;
#endif
}

static const lv_font_t *screen_wifi_hint_font_for_text(uint32_t cp_a, uint32_t cp_b, uint32_t cp_c)
{
    lv_font_glyph_dsc_t gd;

    if(lv_font_get_glyph_dsc(&lv_font_cn_wifi_25, &gd, cp_a, 0u) &&
       lv_font_get_glyph_dsc(&lv_font_cn_wifi_25, &gd, cp_b, 0u) &&
       lv_font_get_glyph_dsc(&lv_font_cn_wifi_25, &gd, cp_c, 0u)) {
        return &lv_font_cn_wifi_25;
    }
    return &lv_font_SourceHanSerifSC_Regular_25;
}

static void screen_wifi_hint_prepare_row(const lv_font_t *hint_font)
{
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan)) {
        lv_obj_clear_flag(guider_ui.screen_11_label_scan, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(guider_ui.screen_11_label_scan, hint_font,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_move_foreground(guider_ui.screen_11_label_scan);
    }
    /* 省略号仅用 Guider 原字库（含 '.'），勿与主文案共用 cn_wifi_25 */
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan_dots)) {
        lv_obj_set_style_text_font(guider_ui.screen_11_label_scan_dots,
                                   &lv_font_SourceHanSerifSC_Regular_25,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(guider_ui.screen_11_label_scan_dots, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(guider_ui.screen_11_label_scan_dots);
    }
    screen_wifi_refresh_btn_raise();
}

static void screen_wifi_show_scanning(void)
{
    screen_wifi_hint_prepare_row(&lv_font_SourceHanSerifSC_Regular_25);
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan)) {
        lv_label_set_text(guider_ui.screen_11_label_scan, "\xe6\x89\xab\xe6\x8f\x8f\xe4\xb8\xad");
    }
    screen_wifi_layout_scan_row();
}

static void screen_wifi_show_connecting(void)
{
    const lv_font_t *font = screen_wifi_hint_font_for_text(0x6B63u, 0x5728u, 0x8FDEu);

    screen_wifi_hint_prepare_row(font);
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan)) {
        lv_label_set_text(guider_ui.screen_11_label_scan, "\xe6\xad\xa3\xe5\x9c\xa8\xe8\xbf\x9e\xe6\x8e\xa5");
    }
    screen_wifi_layout_scan_row();
}

static void screen_wifi_show_connect_result(uint8_t ok)
{
    const lv_font_t *font = screen_wifi_hint_font_for_text(0x8FDEu, 0x63A5u,
                                                           ok ? 0x6210u : 0x5931u);

    s_scan_dots_anim_on = 0u;
    screen_wifi_hint_prepare_row(font);
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan)) {
        lv_label_set_text(guider_ui.screen_11_label_scan,
                          ok ? "\xe8\xbf\x9e\xe6\x8e\xa5\xe6\x88\x90\xe5\x8a\x9f" :
                               "\xe8\xbf\x9e\xe6\x8e\xa5\xe5\xa4\xb1\xe8\xb4\xa5");
    }
    if(lv_obj_is_valid(guider_ui.screen_11_label_scan_dots)) {
        lv_label_set_text(guider_ui.screen_11_label_scan_dots, "");
    }
}

static void screen_wifi_update_scan_hint(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    uint8_t scan_busy;
    lv_font_glyph_dsc_t gd;
    uint8_t have_done_text = 0u;

    if(!lv_obj_is_valid(guider_ui.screen_11_label_scan)) {
        return;
    }
    if(g_app_scr != APP_SCR_11 || screen_wifi_popup_is_active()) {
        return;
    }
    if(app_wifi_connect_busy() != 0u) {
        return;
    }
    scan_busy = app_wifi_scan_busy();
    if(scan_busy != 0u) {
        screen_wifi_show_scanning();
        if(!s_scan_dots_anim_on) {
            screen_wifi_scan_dots_anim_reset();
        }
    } else if(app_wifi_scan_last_failed() != 0u) {
        lv_obj_set_style_text_font(guider_ui.screen_11_label_scan, &lv_font_cn_wifi_25,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(guider_ui.screen_11_label_scan, "\xe6\x89\xab\xe6\x8f\x8f\xe5\xa4\xb1\xe8\xb4\xa5");
        if(lv_obj_is_valid(guider_ui.screen_11_label_scan_dots)) {
            lv_label_set_text(guider_ui.screen_11_label_scan_dots, "");
            lv_obj_clear_flag(guider_ui.screen_11_label_scan_dots, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(guider_ui.screen_11_label_scan_dots);
        }
        screen_wifi_hide_empty_hint();
        lv_obj_move_foreground(guider_ui.screen_11_label_scan);
        s_scan_dots_anim_on = 0u;
        screen_wifi_refresh_btn_raise();
    } else {
        if(lv_font_get_glyph_dsc(&lv_font_SourceHanSerifSC_Regular_25, &gd, 0x626Bu, 0u) &&
           lv_font_get_glyph_dsc(&lv_font_SourceHanSerifSC_Regular_25, &gd, 0x63CFu, 0u) &&
           lv_font_get_glyph_dsc(&lv_font_SourceHanSerifSC_Regular_25, &gd, 0x5B8Cu, 0u) &&
           lv_font_get_glyph_dsc(&lv_font_SourceHanSerifSC_Regular_25, &gd, 0x6210u, 0u)) {
            have_done_text = 1u;
        }
        lv_obj_set_style_text_font(guider_ui.screen_11_label_scan, &lv_font_SourceHanSerifSC_Regular_25,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(guider_ui.screen_11_label_scan,
                          have_done_text ? "\xe6\x89\xab\xe6\x8f\x8f\xe5\xae\x8c\xe6\x88\x90" :
                                           "\xe6\x89\xab\xe6\x8f\x8fOK");
        if(lv_obj_is_valid(guider_ui.screen_11_label_scan_dots)) {
            lv_label_set_text(guider_ui.screen_11_label_scan_dots, "");
            lv_obj_clear_flag(guider_ui.screen_11_label_scan_dots, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(guider_ui.screen_11_label_scan_dots);
        }
        lv_obj_move_foreground(guider_ui.screen_11_label_scan);
        s_scan_dots_anim_on = 0u;
        screen_wifi_refresh_btn_raise();
    }
#endif
}

void screen_wifi_list_move(int8_t delta)
{
    uint8_t cnt = s_list_built_count;
    if(cnt == 0u || screen_wifi_popup_is_active()) return;
    if(delta < 0 && s_sel_index > 0u) {
        screen_wifi_set_selected_ex((uint8_t)(s_sel_index - 1u), 1u);
    } else if(delta > 0 && (s_sel_index + 1u) < cnt) {
        screen_wifi_set_selected_ex((uint8_t)(s_sel_index + 1u), 1u);
    }
}

void screen_wifi_list_select(uint8_t idx)
{
    if(screen_wifi_popup_is_active()) return;
    if(idx < s_list_built_count) {
        screen_wifi_set_selected(idx);
    }
}

uint8_t screen_wifi_get_sel_index(void)
{
    return s_sel_index;
}

uint8_t screen_wifi_hit_row(lv_coord_t x, lv_coord_t y)
{
    uint8_t i;

    if(screen_wifi_popup_is_active()) {
        return 0xFFu;
    }

    for(i = 0u; i < s_list_built_count; i++) {
        lv_obj_t *row = s_rows[i].row;
        lv_area_t a;
        lv_point_t p;

        if(row == NULL || !lv_obj_is_valid(row)) {
            continue;
        }
        lv_obj_get_coords(row, &a);
        p.x = x;
        p.y = y;
        if(_lv_area_is_point_on(&a, &p, 4)) {
            return i;
        }
    }
    return 0xFFu;
}

uint8_t screen_wifi_point_in_list(lv_coord_t x, lv_coord_t y)
{
    lv_area_t a;
    lv_point_t p;

    if(!lv_obj_is_valid(guider_ui.screen_11_list_panel)) {
        return 0u;
    }
    lv_obj_get_coords(guider_ui.screen_11_list_panel, &a);
    p.x = x;
    p.y = y;
    return _lv_area_is_point_on(&a, &p, 0) ? 1u : 0u;
}

lv_obj_t *screen_wifi_refresh_btn_obj(void)
{
    return s_btn_refresh;
}

uint8_t screen_wifi_hit_refresh_btn(lv_coord_t x, lv_coord_t y)
{
    return screen_wifi_obj_hit(s_btn_refresh, x, y);
}

void screen_wifi_refresh_once(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(g_app_scr != APP_SCR_11 || screen_wifi_popup_is_active()) {
        return;
    }
    app_wifi_connect_gui_recover();
    if(app_wifi_connect_busy() != 0u) {
        if(cloud_aliyun_at_user_wifi_join_active() != 0u ||
           cloud_aliyun_at_wifi_bringup_active() != 0u) {
            printf("[WiFi] refresh ignored: connect busy\r\n");
            return;
        }
        app_wifi_connect_reset();
    }
    if(app_link_guard_mqtt() != 0u) {
        app_link_guard_mqtt_end(0u);
    }
    if(app_link_guard_blocks_scan() != 0u) {
        printf("[WiFi] refresh ignored: link guard\r\n");
        return;
    }
    app_wifi_scan_release_connect_hold();
    g_wifi_scan_abort = 0u;
    app_wifi_scan_request_now();
    printf("[WiFi] refresh tap excl=%u pend=%u busy=%u guard=%u\r\n",
           (unsigned)g_wifi_exclusive,
           (unsigned)g_wifi_scan_pending,
           (unsigned)app_wifi_scan_busy(),
           (unsigned)app_link_guard_blocks_scan());
    screen_wifi_list_conceal_for_scan();
    screen_wifi_show_scanning();
    screen_wifi_scan_dots_anim_reset();
    screen_wifi_gui_wake();
#endif
}

void screen_wifi_list_scroll_by(lv_coord_t dy)
{
    lv_obj_t *panel;
    lv_coord_t max_y;
    int32_t ny;
    lv_coord_t y;

    panel = guider_ui.screen_11_list_panel;
    if(panel == NULL || !lv_obj_is_valid(panel)) {
        return;
    }
    if(dy == 0 || s_list_built_count == 0u) {
        return;
    }
    max_y = screen_wifi_list_scroll_max_y();
    if(max_y <= 0) {
        lv_obj_scroll_to_y(panel, 0, LV_ANIM_OFF);
        return;
    }
    if(dy > 24) {
        dy = 24;
    } else if(dy < -24) {
        dy = -24;
    }
    y = lv_obj_get_scroll_y(panel);
    ny = (int32_t)y - (int32_t)dy * 2;
    if(ny < 0) {
        ny = 0;
    } else if(ny > (int32_t)max_y) {
        ny = (int32_t)max_y;
    }
    lv_obj_scroll_to_y(panel, (lv_coord_t)ny, LV_ANIM_OFF);
}

void screen_wifi_list_ok(void)
{
    const char *ssid;
    if(screen_wifi_popup_is_active()) return;
    if(screen_wifi_scan_in_progress() != 0u) {
        WIFI_DBG("list ok blocked: wait scan done");
        return;
    }
    if(app_wifi_connect_busy()) return;
    if(s_sel_index >= s_list_built_count) return;
    if(s_rows[s_sel_index].lbl_name == NULL) return;
    ssid = lv_label_get_text(s_rows[s_sel_index].lbl_name);
    if(ssid == NULL || ssid[0] == '\0') return;
    screen_wifi_popup_open(ssid);
}

void screen_wifi_prepare_on_enter(void)
{
    s_sel_index = 0u;
    s_sel_ssid[0] = '\0';
    s_connecting_row = 0xFFu;
    app_touch_wifi_reset();
    screen_wifi_popup_close();
    screen_wifi_clear_rows();
    screen_wifi_hide_empty_hint();
    app_wifi_cfg_init_defaults();
    screen_wifi_list_panel_setup();
    screen_wifi_refresh_btn_ensure();
    s_last_list_merge_ms = 0u;
    s_merge_hold_until_ms = 0u;
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    WIFI_DBG("UI enter scr11 (manual refresh mode)");
    app_wifi_scan_reset();
    app_wifi_connect_reset();
    app_wifi_remember_scr11_reset();
    /* 不自动扫描；列表/状态仅响应用户点「扫描」或连接结果 */
#endif
    screen_wifi_show_enter_idle_hint();
    screen_wifi_update_connected_bar();
    if(cloud_aliyun_at_wifi_link_ready() != 0u) {
        cloud_aliyun_at_request_wifi_ip_verify();
    }
    screen_wifi_refresh_btn_raise();
}

void screen_wifi_gui_wake(void)
{
    s_gui_wake = 1u;
}

void screen_wifi_notify_scan_start(void)
{
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(g_app_scr != APP_SCR_11) {
        return;
    }
    screen_wifi_list_conceal_for_scan();
    screen_wifi_show_scanning();
    screen_wifi_scan_dots_anim_reset();
    screen_wifi_gui_wake();
#endif
}

void screen_wifi_notify_scan_done(void)
{
    s_force_list_merge = 1u;
    s_scan_dots_anim_on = 0u;
    screen_wifi_gui_wake();
}

void screen_wifi_notify_sta_up(void)
{
    s_connecting_row = 0xFFu;
    if(g_app_scr == APP_SCR_11) {
        screen_wifi_show_connect_result(1u);
    }
    screen_wifi_update_connected_bar();
    screen_wifi_gui_wake();
}

void screen_wifi_notify_sta_down(void)
{
    s_connecting_row = 0xFFu;
    s_pending_ssid[0] = '\0';
    s_scan_dots_anim_on = 0u;
    screen_wifi_update_connected_bar();
    screen_wifi_gui_wake();
}

void screen_wifi_notify_connect_fail(void)
{
    s_connecting_row = 0xFFu;
    if(g_app_scr != APP_SCR_11) {
        return;
    }
    screen_wifi_show_connect_result(0u);
    screen_wifi_gui_wake();
}

uint8_t screen_wifi_gui_work_pending(void)
{
    if(g_app_scr != APP_SCR_11) {
        return 0u;
    }
    if(s_gui_wake != 0u) {
        return 1u;
    }
    if(screen_wifi_popup_is_active()) {
        return 1u;
    }
    if(s_scan_dots_anim_on != 0u) {
        return 1u;
    }
#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    if(app_wifi_scan_busy() != 0u || app_wifi_scan_has_pending() != 0u) {
        return 1u;
    }
    if(app_wifi_connect_busy() != 0u) {
        return 1u;
    }
#endif
    return 0u;
}

void screen_wifi_poll_tick(void)
{
    static uint8_t s_last_scan_busy = 0u;
    uint8_t busy;

    if(g_app_scr != APP_SCR_11) {
        s_scan_dots_anim_on = 0u;
        s_last_scan_busy = 0u;
        s_gui_wake = 0u;
        return;
    }

    app_wifi_scan_gui_tick();
    app_wifi_connect_gui_recover();

#if (APP_WIFI_UI_SCAN_ENABLE == 1)
    busy = app_wifi_scan_busy();

    if(!screen_wifi_popup_is_active()) {
        if(app_wifi_connect_busy() != 0u) {
            if(!s_scan_dots_anim_on) {
                screen_wifi_show_connecting();
                screen_wifi_scan_dots_anim_reset();
            } else {
                screen_wifi_scan_dots_anim_tick();
            }
        } else {
            /* 仅真实扫描进行中显示「扫描中」；仅 pending 时 Cloud 可能还在等 mutex */
            uint8_t scan_ui = (uint8_t)(busy != 0u ||
                                        cloud_aliyun_at_cwlap_scan_async_active() != 0u ||
                                        s_scan_dots_anim_on != 0u);
            if(scan_ui != 0u) {
                if(!s_scan_dots_anim_on) {
                    screen_wifi_show_scanning();
                    screen_wifi_scan_dots_anim_reset();
                } else {
                    screen_wifi_scan_dots_anim_tick();
                }
            }
        }
    }

    if(s_last_scan_busy != 0u && busy == 0u && !screen_wifi_popup_is_active()) {
        screen_wifi_update_scan_hint();
        app_wifi_scan_release_uart2();
        if(screen_wifi_should_merge_list_now() != 0u) {
            (void)app_wifi_scan_take_list_dirty();
            s_last_list_merge_ms = lv_tick_get();
            screen_wifi_merge_list();
        }
        screen_wifi_gui_wake();
    } else if(screen_wifi_should_merge_list_now() != 0u) {
        (void)app_wifi_scan_take_list_dirty();
        s_last_list_merge_ms = lv_tick_get();
        screen_wifi_merge_list();
    }

    s_last_scan_busy = busy;

    if(s_force_list_merge != 0u && !screen_wifi_popup_is_active() &&
       screen_wifi_scan_pass_done() != 0u && app_wifi_scan_count() > 0u) {
        s_force_list_merge = 0u;
        (void)app_wifi_scan_take_list_dirty();
        s_last_list_merge_ms = lv_tick_get();
        screen_wifi_merge_list();
        screen_wifi_update_scan_hint();
    } else if(s_force_list_merge != 0u && screen_wifi_scan_pass_done() != 0u) {
        s_force_list_merge = 0u;
        screen_wifi_list_reveal();
        screen_wifi_update_scan_hint();
    }

    if(app_wifi_remember_scr11_poll() != 0u) {
        screen_wifi_update_connected_bar();
    }

#endif
}

void screen_wifi_cursor_blink_handle(void)
{
    if(screen_wifi_popup_is_active()) {
        ui_cursor_blink_step(s_wifi_cursor, &s_wifi_cursor_visible, &s_wifi_cursor_last_ms, 500u);
    }
}

void enter_screen_wifi(void)
{
    ui_nav_ctx_t ctx = build_nav_ctx();

    g_screen3_pending_index = 4u;
    ui_nav_enter_screen_11(&ctx);
    screen_wifi_prepare_on_enter();
}
