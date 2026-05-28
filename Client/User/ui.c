/**
 * @file ui.c
 * @brief 与 PC 工程 main.c 中 UI_SCREEN_BG / 双入口（User、Admin）思路一致；无 SDL、无 PC 路径图片、无文件库。
 *        当前工程 lv_conf 仅启用 Montserrat 14，故使用英文文案。
 */

#include "ui.h"
#include <stdio.h>

#define UI_SCREEN_BG   0xCDE8F5
#define UI_ACCENT      0x2B6CB0
#define UI_FIELD_BG    0xE8F4FC
#define UI_TEXT_DARK   0x1A365D
#define UI_WEATHER_GOLD 0xC05621

static lv_obj_t *s_home_scr;
static lv_obj_t *s_lbl_time;
static lv_obj_t *s_lbl_status;

static void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    uint32_t t   = lv_tick_get() / 1000U;
    uint32_t h   = (t / 3600U) % 24U;
    uint32_t m   = (t / 60U) % 60U;
    uint32_t sec = t % 60U;
    char buf[12];

    lv_snprintf(buf, sizeof(buf), "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)sec);
    lv_label_set_text(s_lbl_time, buf);
}

static void open_unlock_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_label_set_text(s_lbl_status, "Unlock: user PIN page (demo)");
}

static void open_menu_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_label_set_text(s_lbl_status, "Menu: admin (demo)");
}

void ui_init(void)
{
    lv_coord_t w = lv_disp_get_hor_res(NULL);
    lv_coord_t h = lv_disp_get_ver_res(NULL);

    s_home_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_home_scr, w, h);
    lv_obj_set_style_bg_color(s_home_scr, lv_color_hex(UI_SCREEN_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_home_scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(s_home_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 右上：气温（PC 端为白字+底图，此处改为深蓝字以适配浅底） */
    lv_obj_t *lbl_temp = lv_label_create(s_home_scr);
    lv_label_set_text(lbl_temp, "26 C");
    lv_obj_set_style_text_color(lbl_temp, lv_color_hex(UI_ACCENT), LV_PART_MAIN);
    lv_obj_align(lbl_temp, LV_ALIGN_TOP_RIGHT, -10, 12);

    lv_obj_t *lbl_weather = lv_label_create(s_home_scr);
    lv_label_set_text(lbl_weather, "Sunny");
    lv_obj_set_style_text_color(lbl_weather, lv_color_hex(UI_WEATHER_GOLD), LV_PART_MAIN);
    lv_obj_align_to(lbl_weather, lbl_temp, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    /* 大时间（使用默认可用字体，避免额外字体配置依赖） */
    s_lbl_time = lv_label_create(s_home_scr);
    lv_label_set_text(s_lbl_time, "00:00:00");
    lv_obj_set_style_text_font(s_lbl_time, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_time, lv_color_hex(UI_TEXT_DARK), LV_PART_MAIN);
    lv_obj_align(s_lbl_time, LV_ALIGN_TOP_MID, 0, 56);

    lv_obj_t *lbl_date = lv_label_create(s_home_scr);
    lv_label_set_text(lbl_date, "2026-04-13 Mon");
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(UI_ACCENT), LV_PART_MAIN);
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, 100);

    /* 中部圆角块 + 图标，对应 PC 中央图标区 */
    lv_obj_t *mid = lv_obj_create(s_home_scr);
    lv_obj_set_size(mid, 108, 108);
    lv_obj_align(mid, LV_ALIGN_CENTER, 0, 8);
    lv_obj_set_style_radius(mid, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(mid, lv_color_hex(UI_FIELD_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mid, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(mid, lv_color_hex(UI_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_width(mid, 2, LV_PART_MAIN);
    lv_obj_clear_flag(mid, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ic = lv_label_create(mid);
    lv_label_set_text(ic, LV_SYMBOL_HOME);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(ic, lv_color_hex(UI_ACCENT), LV_PART_MAIN);
    lv_obj_center(ic);

    /* 底栏：开锁（右下） / 菜单（左下），与 PC create_home_ui 一致 */
    lv_obj_t *lbl_menu = lv_label_create(s_home_scr);
    lv_label_set_text(lbl_menu, "Menu");
    lv_obj_set_style_text_color(lbl_menu, lv_color_hex(UI_ACCENT), LV_PART_MAIN);
    lv_obj_add_flag(lbl_menu, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(lbl_menu, LV_ALIGN_BOTTOM_LEFT, 14, -14);
    lv_obj_add_event_cb(lbl_menu, open_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_unlock = lv_label_create(s_home_scr);
    lv_label_set_text(lbl_unlock, "Unlock");
    lv_obj_set_style_text_color(lbl_unlock, lv_color_hex(UI_ACCENT), LV_PART_MAIN);
    lv_obj_add_flag(lbl_unlock, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(lbl_unlock, LV_ALIGN_BOTTOM_RIGHT, -14, -14);
    lv_obj_add_event_cb(lbl_unlock, open_unlock_cb, LV_EVENT_CLICKED, NULL);

    s_lbl_status = lv_label_create(s_home_scr);
    lv_label_set_text(s_lbl_status, " ");
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0x718096), LV_PART_MAIN);
    lv_obj_set_width(s_lbl_status, w - 24);
    lv_label_set_long_mode(s_lbl_status, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_lbl_status, LV_ALIGN_BOTTOM_MID, 0, -40);

    lv_timer_create(clock_timer_cb, 1000, NULL);

    lv_scr_load(s_home_scr);
}

lv_obj_t *ui_get_home_screen(void)
{
    return s_home_scr;
}
