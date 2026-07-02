#include "app_ui_v3_anim.h"
#include "app_ui_v3_theme.h"
#include "app_ui_v3_fonts.h"
#include <stdio.h>

static void anim_set_y(void *obj, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)obj, v);
}

static void anim_set_zoom(void *obj, int32_t v)
{
    lv_obj_set_style_transform_zoom((lv_obj_t *)obj, (lv_coord_t)v, 0);
}

static void anim_set_opa(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void anim_set_arc_angle(void *obj, int32_t v)
{
    lv_arc_set_rotation((lv_obj_t *)obj, (lv_coord_t)v);
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

void ui3_anim_zoom_breathe(lv_obj_t *obj, int32_t min_zoom, int32_t max_zoom, uint32_t ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, anim_set_zoom);
    lv_anim_set_values(&a, min_zoom, max_zoom);
    lv_anim_set_time(&a, ms);
    lv_anim_set_playback_time(&a, ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
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
    /* 根 screen 先设透明再淡入：40 行双缓冲上动画常跑不完，会一直透明像白屏 */
    if(scr != NULL) {
        lv_obj_set_style_opa(scr, LV_OPA_COVER, 0);
    }
}

static void ripple_set_scale(void *obj, int32_t v)
{
    lv_obj_set_style_transform_zoom((lv_obj_t *)obj, (lv_coord_t)v, 0);
}

static void ripple_set_opa(void *obj, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void anim_obj_del(lv_anim_t *a)
{
    lv_obj_del((lv_obj_t *)a->var);
}

static void ripple_spawn(lv_obj_t *parent, uint32_t delay_ms)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, 40, 40);
    lv_obj_set_style_radius(r, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(r, UI3_COL_TERRA, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_60, 0);
    lv_obj_center(r);
    lv_obj_set_style_transform_pivot_x(r, 20, 0);
    lv_obj_set_style_transform_pivot_y(r, 20, 0);
    lv_obj_set_style_transform_zoom(r, 64, 0);

    lv_anim_t a1, a2;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, r);
    lv_anim_set_exec_cb(&a1, ripple_set_scale);
    lv_anim_set_values(&a1, 64, 900);
    lv_anim_set_time(&a1, 1400);
    lv_anim_set_delay(&a1, delay_ms);
    lv_anim_set_path_cb(&a1, lv_anim_path_ease_out);
    lv_anim_set_deleted_cb(&a1, anim_obj_del);
    lv_anim_start(&a1);

    lv_anim_init(&a2);
    lv_anim_set_var(&a2, r);
    lv_anim_set_exec_cb(&a2, ripple_set_opa);
    lv_anim_set_values(&a2, LV_OPA_60, LV_OPA_TRANSP);
    lv_anim_set_time(&a2, 1400);
    lv_anim_set_delay(&a2, delay_ms);
    lv_anim_start(&a2);
}

void ui3_show_unlock_ripple(lv_obj_t *parent)
{
    lv_obj_t *layer = lv_obj_create(parent);
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, UI3_W, UI3_H);
    lv_obj_set_style_bg_color(layer, UI3_COL_PAPER, 0);
    lv_obj_set_style_bg_opa(layer, LV_OPA_80, 0);
    lv_obj_center(layer);
    lv_obj_move_foreground(layer);

    lv_obj_t *title = lv_label_create(layer);
    lv_label_set_text(title, "门已打开");
    lv_obj_add_style(title, ui3_style_title(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -16);
    lv_obj_t *sub = lv_label_create(layer);
    lv_label_set_text(sub, "欢迎");
    lv_obj_add_style(sub, ui3_style_label(), 0);
    lv_obj_set_style_text_color(sub, UI3_COL_INK_SOFT, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 12);

    ripple_spawn(layer, 0);
    ripple_spawn(layer, 120);
    ripple_spawn(layer, 240);

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, layer);
    lv_anim_set_exec_cb(&fade, anim_set_opa);
    lv_anim_set_values(&fade, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&fade, 300);
    lv_anim_set_delay(&fade, 1600);
    lv_anim_set_ready_cb(&fade, anim_obj_del);
    lv_anim_start(&fade);
}

static void modal_close(lv_event_t *e)
{
    lv_obj_del(lv_obj_get_parent(lv_event_get_target(e)));
}

void ui3_show_modal_result(lv_obj_t *parent, const char *title, const char *sub, bool success)
{
    lv_obj_t *mask = lv_obj_create(parent);
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, UI3_W, UI3_H);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_40, 0);
    lv_obj_center(mask);

    lv_obj_t *sheet = lv_obj_create(mask);
    lv_obj_set_size(sheet, 200, 140);
    lv_obj_center(sheet);
    lv_obj_set_style_radius(sheet, 18, 0);
    lv_obj_set_style_bg_color(sheet, UI3_COL_SURFACE, 0);
    lv_obj_set_style_pad_all(sheet, 16, 0);
    lv_obj_add_flag(sheet, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sheet, modal_close, LV_EVENT_CLICKED, NULL);

    lv_obj_t *ico = lv_obj_create(sheet);
    lv_obj_remove_style_all(ico);
    lv_obj_set_size(ico, 48, 48);
    lv_obj_align(ico, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_radius(ico, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ico, 2, 0);
    lv_obj_set_style_border_color(ico, success ? UI3_COL_TERRA : UI3_COL_DANGER, 0);
    lv_obj_set_style_bg_opa(ico, LV_OPA_TRANSP, 0);

    lv_obj_t *mark = lv_label_create(ico);
    lv_label_set_text(mark, success ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(mark, success ? UI3_COL_TERRA : UI3_COL_DANGER, 0);
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

    ui3_anim_screen_in(sheet);
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
    lv_obj_set_style_text_color(l, UI3_COL_SURFACE, 0);
    lv_obj_center(l);
    lv_timer_create(toast_timer_cb, 2200, toast);
}

static void modal_auto_del(lv_timer_t *t)
{
    lv_obj_del((lv_obj_t *)t->user_data);
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

typedef struct {
    lv_obj_t *parent;
    lv_obj_t *mask;
    char title[24];
    uint8_t success;
} ui3_modal_done_t;

static void enroll_done_cb(lv_timer_t *t)
{
    ui3_modal_done_t *ctx = (ui3_modal_done_t *)t->user_data;
    if(ctx != NULL) {
        if(ctx->mask && lv_obj_is_valid(ctx->mask)) {
            lv_obj_del(ctx->mask);
        }
        if(ctx->parent && lv_obj_is_valid(ctx->parent)) {
            ui3_show_modal_result(ctx->parent, ctx->title,
                                  ctx->success ? "" : "请重试", ctx->success != 0u);
        }
        lv_mem_free(ctx);
    }
    lv_timer_del(t);
}

void ui3_show_fp_modal(lv_obj_t *parent, const char *done_title, bool success)
{
    ui3_modal_done_t *ctx = lv_mem_alloc(sizeof(*ctx));
    lv_obj_t *mask;
    lv_obj_t *sheet;

    if(ctx == NULL) {
        return;
    }
    ctx->parent = parent;
    ctx->success = success ? 1u : 0u;
    (void)snprintf(ctx->title, sizeof(ctx->title), "%s", done_title ? done_title : "录入完成");

    mask = lv_obj_create(parent);
    ctx->mask = mask;
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, UI3_W, UI3_H);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_40, 0);

    sheet = lv_obj_create(mask);
    lv_obj_set_size(sheet, 200, 130);
    lv_obj_center(sheet);
    lv_obj_set_style_radius(sheet, 18, 0);
    lv_obj_set_style_bg_color(sheet, UI3_COL_SURFACE, 0);

    lv_obj_t *ico = lv_label_create(sheet);
    lv_label_set_text(ico, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_color(ico, UI3_COL_TERRA, 0);
    lv_obj_align(ico, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_t *tit = lv_label_create(sheet);
    lv_label_set_text(tit, "指纹录入中");
    lv_obj_add_style(tit, ui3_style_title(), 0);
    lv_obj_align(tit, LV_ALIGN_CENTER, 0, 8);
    lv_obj_t *sub = lv_label_create(sheet);
    lv_label_set_text(sub, "请按压手指 3 次");
    lv_obj_add_style(sub, ui3_style_label(), 0);
    lv_obj_set_style_text_color(sub, UI3_COL_INK_FAINT, 0);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_timer_create(enroll_done_cb, 2400, ctx);
}

void ui3_show_nfc_modal(lv_obj_t *parent, const char *done_title, bool success)
{
    ui3_modal_done_t *ctx = lv_mem_alloc(sizeof(*ctx));
    lv_obj_t *mask;
    lv_obj_t *sheet;

    if(ctx == NULL) {
        return;
    }
    ctx->parent = parent;
    ctx->success = success ? 1u : 0u;
    (void)snprintf(ctx->title, sizeof(ctx->title), "%s", done_title ? done_title : "绑定完成");

    mask = lv_obj_create(parent);
    ctx->mask = mask;
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, UI3_W, UI3_H);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_40, 0);

    sheet = lv_obj_create(mask);
    lv_obj_set_size(sheet, 200, 130);
    lv_obj_center(sheet);
    lv_obj_set_style_radius(sheet, 18, 0);
    lv_obj_set_style_bg_color(sheet, UI3_COL_SURFACE, 0);

    lv_obj_t *ring = lv_obj_create(sheet);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 48, 48);
    lv_obj_align(ring, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(ring, UI3_COL_TERRA, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    ui3_anim_opa_pulse(ring, LV_OPA_40, LV_OPA_COVER, 900);

     lv_obj_t *tit = lv_label_create(sheet);
    lv_label_set_text(tit, "NFC 录入中");
    lv_obj_add_style(tit, ui3_style_title(), 0);
    lv_obj_align(tit, LV_ALIGN_CENTER, 0, 8);
    lv_obj_t *sub = lv_label_create(sheet);
    lv_label_set_text(sub, "请将卡片靠近读卡区");
    lv_obj_add_style(sub, ui3_style_label(), 0);
    lv_obj_set_style_text_color(sub, UI3_COL_INK_FAINT, 0);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_timer_create(enroll_done_cb, 2200, ctx);
}

static void wifi_pwd_close(lv_event_t *e)
{
    lv_obj_del(lv_event_get_user_data(e));
}

void ui3_show_wifi_pwd_modal(lv_obj_t *parent, const char *ssid)
{
    char title[40];
    lv_obj_t *mask = lv_obj_create(parent);
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, UI3_W, UI3_H);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_50, 0);

    lv_obj_t *sheet = lv_obj_create(mask);
    lv_obj_set_size(sheet, 210, 150);
    lv_obj_center(sheet);
    lv_obj_set_style_radius(sheet, 18, 0);
    lv_obj_set_style_bg_color(sheet, UI3_COL_SURFACE, 0);
    lv_obj_set_style_pad_all(sheet, 14, 0);

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
    lv_label_set_text(ph, "请输入密码");
    lv_obj_add_style(ph, ui3_style_label(), 0);
    lv_obj_set_style_text_color(ph, UI3_COL_INK_FAINT, 0);
    lv_obj_align(ph, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *ok = lv_obj_create(sheet);
    lv_obj_remove_style_all(ok);
    lv_obj_clear_flag(ok, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ok, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(ok, 180, 34);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_style(ok, ui3_style_btn_fill(), 0);
    lv_obj_add_event_cb(ok, wifi_pwd_close, LV_EVENT_CLICKED, mask);
    lv_obj_t *ol = lv_label_create(ok);
    lv_label_set_text(ol, "连接");
    lv_obj_set_style_text_color(ol, UI3_COL_SURFACE, 0);
    lv_obj_center(ol);
}
