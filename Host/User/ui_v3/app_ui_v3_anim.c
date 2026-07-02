#include "app_ui_v3_anim.h"
#include "app_ui_v3_services.h"
#include "app_ui_v3_theme.h"
#include "app_ui_v3_fonts.h"
#include "app_ui_v3_widgets.h"
#include "app_ui_v3.h"
#include <stdio.h>

static uint8_t s_modal_open;
static uint8_t s_modal_nav_back;

uint8_t ui3_modal_blocks_input(void)
{
    ui3_state_t *st = ui3_state();
    if(st != NULL && st->scr == UI3_SCR_WIFI && st->wifi_modal != 0u) {
        return 0u;
    }
    return s_modal_open;
}

void ui3_modal_release(void)
{
    s_modal_open = 0u;
}

static lv_obj_t *ui3_modal_mask_create(lv_obj_t *parent)
{
    lv_obj_t *mask = lv_obj_create(parent);
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, UI3_W, UI3_H);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_40, 0);
    lv_obj_move_foreground(mask);
    s_modal_open = 1u;
    return mask;
}

static void ui3_modal_sheet_style(lv_obj_t *sheet)
{
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

static void anim_set_y(void *obj, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)obj, v);
}

static void anim_set_opa(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void anim_set_arc_angle(void *obj, int32_t v)
{
    lv_arc_set_rotation((lv_obj_t *)obj, (lv_coord_t)v);
}

static void anim_set_ring_size(void *obj, int32_t v)
{
    lv_obj_set_size((lv_obj_t *)obj, (lv_coord_t)v, (lv_coord_t)v);
    lv_obj_center((lv_obj_t *)obj);
}

static void anim_set_border_opa(void *obj, int32_t v)
{
    lv_obj_set_style_border_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

void ui3_anim_y(lv_obj_t *obj, int32_t from, int32_t to, uint32_t ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, anim_set_y);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void ui3_anim_opa_pulse(lv_obj_t *obj, lv_opa_t min_opa, lv_opa_t max_opa, uint32_t ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, anim_set_opa);
    lv_anim_set_values(&a, (int32_t)min_opa, (int32_t)max_opa);
    lv_anim_set_time(&a, ms);
    lv_anim_set_playback_time(&a, ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

void ui3_anim_arc_rotate(lv_obj_t *arc, uint32_t period_ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, anim_set_arc_angle);
    lv_anim_set_values(&a, 0, 360);
    lv_anim_set_time(&a, period_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_start(&a);
}

void ui3_anim_screen_in(lv_obj_t *scr)
{
    if(scr != NULL) {
        lv_obj_set_style_opa(scr, LV_OPA_COVER, 0);
    }
}

static void anim_obj_del(lv_anim_t *a)
{
    lv_obj_del((lv_obj_t *)a->var);
}

static void welcome_timer_cb(lv_timer_t *t)
{
    lv_obj_t *mask = (lv_obj_t *)t->user_data;
    if(mask != NULL && lv_obj_is_valid(mask)) {
        lv_anim_del(mask, NULL);
        lv_obj_del(mask);
    }
    s_modal_open = 0u;
    lv_timer_del(t);
}

static void modal_result_timer_cb(lv_timer_t *t)
{
    lv_obj_t *mask = (lv_obj_t *)t->user_data;

    if(mask != NULL && lv_obj_is_valid(mask)) {
        lv_obj_del(mask);
    }
    s_modal_open = 0u;
    if(s_modal_nav_back != 0u) {
        s_modal_nav_back = 0u;
        ui3_nav_back();
    }
    lv_timer_del(t);
}

void ui3_show_modal_result(lv_obj_t *parent, const char *title, const char *sub, bool success)
{
    lv_obj_t *mask = ui3_modal_mask_create(parent);

    lv_obj_t *sheet = lv_obj_create(mask);
    lv_obj_set_size(sheet, 200, 140);
    lv_obj_center(sheet);
    lv_obj_set_style_radius(sheet, 18, 0);
    lv_obj_set_style_bg_color(sheet, UI3_COL_SURFACE, 0);
    lv_obj_set_style_pad_all(sheet, 16, 0);
    ui3_modal_sheet_style(sheet);

    lv_obj_t *ico = lv_obj_create(sheet);
    lv_obj_remove_style_all(ico);
    lv_obj_set_size(ico, 48, 48);
    lv_obj_align(ico, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_radius(ico, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ico, 2, 0);
    lv_obj_set_style_border_color(ico, success ? UI3_COL_SAGE : UI3_COL_DANGER, 0);
    lv_obj_set_style_bg_opa(ico, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(ico, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *    mark = lv_label_create(ico);
    lv_label_set_text(mark, success ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(mark, UI3_FONT_SYMBOL, 0);
    lv_obj_set_style_text_color(mark, success ? UI3_COL_SAGE : UI3_COL_DANGER, 0);
    lv_obj_center(mark);

    lv_obj_t *t = lv_label_create(sheet);
    lv_label_set_text(t, title);
    lv_obj_add_style(t, ui3_style_title(), 0);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(t, 168);
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -28);

    if(sub && sub[0]) {
        lv_obj_t *s = lv_label_create(sheet);
        lv_label_set_text(s, sub);
        lv_obj_add_style(s, ui3_style_label(), 0);
        lv_obj_set_style_text_color(s, UI3_COL_INK_FAINT, 0);
        lv_obj_align(s, LV_ALIGN_BOTTOM_MID, 0, -8);
    }

    s_modal_nav_back = success ? 1u : 0u;
    lv_timer_create(modal_result_timer_cb, 3000, mask);
}

static void welcome_ripple_start(lv_obj_t *ring, uint32_t delay_ms)
{
    lv_anim_t a;

    lv_anim_init(&a);
    lv_anim_set_var(&a, ring);
    lv_anim_set_exec_cb(&a, anim_set_ring_size);
    lv_anim_set_values(&a, 36, 132);
    lv_anim_set_time(&a, 900);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, ring);
    lv_anim_set_exec_cb(&a, anim_set_border_opa);
    lv_anim_set_values(&a, LV_OPA_70, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 900);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_start(&a);
}

lv_obj_t *ui3_show_unlock_welcome(lv_obj_t *parent)
{
    lv_obj_t *mask;
    lv_obj_t *sheet;
    lv_obj_t *ring;
    lv_obj_t *mark;
    lv_obj_t *title;
    lv_obj_t *sub;
    lv_coord_t sheet_y;
    uint8_t i;

    mask = lv_obj_create(parent);
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, UI3_W, UI3_H);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_40, 0);
    lv_obj_move_foreground(mask);

    for(i = 0u; i < 3u; i++) {
        lv_obj_t *ripple = lv_obj_create(mask);
        lv_obj_remove_style_all(ripple);
        lv_obj_set_size(ripple, 36, 36);
        lv_obj_align(ripple, LV_ALIGN_CENTER, 0, -18);
        lv_obj_set_style_radius(ripple, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ripple, 2, 0);
        lv_obj_set_style_border_color(ripple, UI3_COL_TERRA, 0);
        lv_obj_set_style_bg_opa(ripple, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(ripple, LV_OBJ_FLAG_SCROLLABLE);
        welcome_ripple_start(ripple, (uint32_t)i * 220u);
    }

    sheet = lv_obj_create(mask);
    lv_obj_remove_style_all(sheet);
    lv_obj_set_size(sheet, 204, 148);
    lv_obj_center(sheet);
    sheet_y = lv_obj_get_y(sheet);
    lv_obj_set_y(sheet, (lv_coord_t)(sheet_y + 28));
    lv_obj_set_style_bg_color(sheet, UI3_COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sheet, 18, 0);
    lv_obj_set_style_border_width(sheet, 1, 0);
    lv_obj_set_style_border_color(sheet, UI3_COL_LINE_STRONG, 0);
    lv_obj_set_style_shadow_width(sheet, 12, 0);
    lv_obj_set_style_shadow_color(sheet, UI3_COL_SHADOW, 0);
    lv_obj_set_style_shadow_opa(sheet, LV_OPA_20, 0);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);

    ring = lv_obj_create(sheet);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 52, 52);
    lv_obj_align(ring, LV_ALIGN_TOP_MID, 0, 14);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ring, UI3_COL_TERRA_SOFT, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, UI3_COL_TERRA, 0);
    ui3_anim_opa_pulse(ring, LV_OPA_70, LV_OPA_COVER, 700);

    mark = lv_label_create(ring);
    lv_label_set_text(mark, LV_SYMBOL_OK);
    lv_obj_set_style_text_font(mark, UI3_FONT_SYMBOL, 0);
    lv_obj_set_style_text_color(mark, UI3_COL_TERRA, 0);
    lv_obj_center(mark);

    title = lv_label_create(sheet);
    lv_label_set_text(title, "门已打开");
    lv_obj_add_style(title, ui3_style_title(), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, 180);
    lv_obj_align(title, LV_ALIGN_BOTTOM_MID, 0, -34);

    sub = lv_label_create(sheet);
    lv_label_set_text(sub, "欢迎");
    lv_obj_add_style(sub, ui3_style_label(), 0);
    lv_obj_set_style_text_color(sub, UI3_COL_TERRA, 0);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(sub, 180);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -12);

    ui3_anim_y(sheet, (int32_t)(sheet_y + 28), (int32_t)sheet_y, 360);
    s_modal_open = 1u;
    return mask;
}

void ui3_welcome_schedule_close(lv_obj_t *mask, uint32_t ms)
{
    if(mask == NULL) {
        return;
    }
    lv_timer_create(welcome_timer_cb, ms, mask);
}

static void toast_timer_cb(lv_timer_t *t)
{
    lv_obj_del((lv_obj_t *)t->user_data);
    lv_timer_del(t);
}

void ui3_show_toast(lv_obj_t *parent, const char *msg)
{
    lv_obj_t *toast = lv_obj_create(parent);
    lv_obj_remove_style_all(toast);
    lv_obj_set_size(toast, LV_SIZE_CONTENT, 28);
    lv_obj_set_style_bg_color(toast, UI3_COL_INK, 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_80, 0);
    lv_obj_set_style_radius(toast, 14, 0);
    lv_obj_set_style_pad_hor(toast, 12, 0);
    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_t *l = lv_label_create(toast);
    lv_label_set_text(l, msg);
    lv_obj_add_style(l, ui3_style_label(), 0);
    lv_obj_set_style_text_color(l, UI3_COL_SURFACE, 0);
    lv_obj_center(l);
    lv_timer_create(toast_timer_cb, 2200, toast);
}

static void modal_auto_del(lv_timer_t *t)
{
    lv_obj_del((lv_obj_t *)t->user_data);
    s_modal_open = 0u;
    lv_timer_del(t);
}

void ui3_show_scan_modal(lv_obj_t *parent, const char *title, const char *sub)
{
    lv_obj_t *mask = lv_obj_create(parent);
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, UI3_W, UI3_H);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_40, 0);

    lv_obj_t *sheet = lv_obj_create(mask);
    lv_obj_set_size(sheet, 200, 120);
    lv_obj_center(sheet);
    lv_obj_set_style_radius(sheet, 18, 0);
    lv_obj_set_style_bg_color(sheet, UI3_COL_SURFACE, 0);

    lv_obj_t *ring = lv_arc_create(sheet);
    lv_obj_set_size(ring, 40, 40);
    lv_obj_align(ring, LV_ALIGN_TOP_MID, 0, 10);
    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_angles(ring, 0, 270);
    ui3_anim_arc_rotate(ring, 800);

    lv_obj_t *t = lv_label_create(sheet);
    lv_label_set_text(t, title);
    lv_obj_add_style(t, ui3_style_title(), 0);
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -24);
    if(sub) {
        lv_obj_t *s = lv_label_create(sheet);
        lv_label_set_text(s, sub);
        lv_obj_add_style(s, ui3_style_label(), 0);
        lv_obj_set_style_text_color(s, UI3_COL_INK_FAINT, 0);
        lv_obj_align(s, LV_ALIGN_BOTTOM_MID, 0, -6);
    }
    lv_timer_create(modal_auto_del, 1600, mask);
}

static lv_obj_t *s_wifi_pwd_mask;
static lv_obj_t *s_wifi_pwd_val;

static void wifi_pwd_close(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    lv_obj_t *mask = (lv_obj_t *)lv_event_get_user_data(e);

    if(st != NULL) {
        st->wifi_modal = 0u;
        st->wifi_pwd[0] = '\0';
    }
    ui3_wifi_modal_close();
    if(mask != NULL && lv_obj_is_valid(mask)) {
        lv_obj_del(mask);
    }
    s_wifi_pwd_mask = NULL;
    s_wifi_pwd_val = NULL;
}

static void wifi_pwd_connect(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();

    (void)e;
    if(st != NULL) {
        ui3_wifi_modal_confirm(st);
        st->wifi_modal = 0u;
        st->wifi_pwd[0] = '\0';
        ui3_wifi_modal_close();
        ui3_wifi_pwd_modal_close();
        ui3_reload_current();
    }
}

void ui3_wifi_pwd_modal_refresh(const char *pwd)
{
    char mask[32];

    if(s_wifi_pwd_val == NULL || !lv_obj_is_valid(s_wifi_pwd_val)) {
        return;
    }
    if(pwd == NULL || pwd[0] == '\0') {
        lv_label_set_text(s_wifi_pwd_val, "");
        return;
    }
    ui3_pwd_mask_fill(mask, sizeof(mask), pwd);
    lv_label_set_text(s_wifi_pwd_val, mask);
}

void ui3_wifi_pwd_modal_close(void)
{
    if(s_wifi_pwd_mask != NULL && lv_obj_is_valid(s_wifi_pwd_mask)) {
        lv_obj_del(s_wifi_pwd_mask);
    }
    s_wifi_pwd_mask = NULL;
    s_wifi_pwd_val = NULL;
    s_modal_open = 0u;
    ui3_wifi_modal_close();
}

void ui3_show_wifi_pwd_modal(lv_obj_t *parent, const char *ssid)
{
    char title[40];
    lv_obj_t *mask;
    lv_obj_t *sheet;

    mask = lv_obj_create(parent);
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, UI3_W, UI3_H);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_40, 0);
    lv_obj_move_foreground(mask);

    sheet = lv_obj_create(mask);
    lv_obj_set_size(sheet, 210, 150);
    lv_obj_center(sheet);
    lv_obj_set_style_radius(sheet, 18, 0);
    lv_obj_set_style_bg_color(sheet, UI3_COL_SURFACE, 0);
    lv_obj_set_style_pad_all(sheet, 14, 0);
    ui3_modal_sheet_style(sheet);

    (void)snprintf(title, sizeof(title), "连接 %s", ssid ? ssid : "");
    lv_obj_t *t = lv_label_create(sheet);
    lv_label_set_text(t, title);
    lv_obj_add_style(t, ui3_style_title(), 0);
    lv_obj_set_width(t, 180);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *fld = lv_obj_create(sheet);
    lv_obj_remove_style_all(fld);
    lv_obj_set_size(fld, 180, 32);
    lv_obj_align(fld, LV_ALIGN_CENTER, 0, -4);
    lv_obj_add_style(fld, ui3_style_field(), 0);
    lv_obj_add_style(fld, ui3_style_field_focus(), 0);

    lv_obj_t *ph = lv_label_create(fld);
    s_wifi_pwd_val = ph;
    lv_label_set_text(ph, "");
    lv_obj_add_style(ph, ui3_style_label(), 0);
    lv_obj_align(ph, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *ok = lv_obj_create(sheet);
    lv_obj_remove_style_all(ok);
    lv_obj_clear_flag(ok, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ok, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(ok, 180, 34);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_style(ok, ui3_style_btn_fill(), 0);
    lv_obj_add_event_cb(ok, wifi_pwd_connect, LV_EVENT_CLICKED, mask);
    lv_obj_t *ol = lv_label_create(ok);
    lv_label_set_text(ol, "连接");
    lv_obj_set_style_text_color(ol, UI3_COL_SURFACE, 0);
    lv_obj_center(ol);
    s_wifi_pwd_mask = mask;
}
