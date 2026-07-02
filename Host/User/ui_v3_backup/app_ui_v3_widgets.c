#include "app_ui_v3_widgets.h"
#include "app_ui_v3_theme.h"
#include "app_ui_v3_fonts.h"
#include "app_ui_v3_anim.h"
#include "app_wall_clock.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

lv_obj_t *ui3_screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, UI3_W, UI3_H);
    lv_obj_add_style(scr, ui3_style_paper(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    ui3_paint_paper_bg(scr);
    return scr;
}

void ui3_update_clock_label(lv_obj_t *time_lbl)
{
    int y, mo, d, h, mi, s;
    char buf[8];

    if(time_lbl == NULL) {
        return;
    }
    if(app_wall_clock_get_datetime(&y, &mo, &d, &h, &mi, &s) != 0u) {
        snprintf(buf, sizeof(buf), "%02d:%02d", h, mi);
    } else {
        uint32_t sec = (lv_tick_get() / 1000u) % 86400u;
        snprintf(buf, sizeof(buf), "%02u:%02u",
                 (unsigned)((sec / 3600u) % 24u), (unsigned)((sec / 60u) % 60u));
    }
    lv_label_set_text(time_lbl, buf);
}

void ui3_update_date_label(lv_obj_t *date_lbl)
{
    int y, mo, d, h, mi, s;
    char buf[24];

    if(date_lbl == NULL) {
        return;
    }
    if(app_wall_clock_get_datetime(&y, &mo, &d, &h, &mi, &s) != 0u) {
        snprintf(buf, sizeof(buf), "%d \xC2\xB7 %02d \xC2\xB7 %02d", y, mo, d);
    } else {
        snprintf(buf, sizeof(buf), "---- \xC2\xB7 -- \xC2\xB7 --");
    }
    lv_label_set_text(date_lbl, buf);
}

lv_obj_t *ui3_signal_dots(lv_obj_t *parent, uint8_t level)
{
    static const lv_coord_t hts[] = { 5, 7, 9, 12 };
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    ui3_style_obj_transparent(row);
    lv_obj_set_size(row, 24, 16);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 2, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    for(uint8_t i = 0; i < 4u; i++) {
        lv_obj_t *bar = lv_obj_create(row);
        lv_obj_remove_style_all(bar);
        lv_obj_set_width(bar, 4);
        lv_obj_set_height(bar, hts[i]);
        lv_obj_set_style_radius(bar, 1, 0);
        lv_obj_set_style_bg_color(bar, (i < level) ? UI3_COL_SAGE_MID : UI3_COL_INK_FAINT, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    }
    return row;
}

void ui3_topbar(ui3_layout_t *lo, ui3_state_t *st)
{
    lo->top_time = lv_label_create(lo->root);
    lv_obj_add_style(lo->top_time, ui3_style_label(), 0);
    lv_obj_set_style_text_font(lo->top_time, UI3_FONT_TITLE, 0);
    lv_obj_set_style_text_color(lo->top_time, UI3_COL_INK_SOFT, 0);
    lv_obj_align(lo->top_time, LV_ALIGN_TOP_LEFT, 12, 8);
    ui3_update_clock_label(lo->top_time);
    st->lbl_top_time = lo->top_time;

    lv_obj_t *meta = lv_obj_create(lo->root);
    lv_obj_remove_style_all(meta);
    ui3_style_obj_transparent(meta);
    lv_obj_set_height(meta, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(meta, 30, 0);
    lv_obj_set_width(meta, LV_SIZE_CONTENT);
    lv_obj_align(meta, LV_ALIGN_TOP_RIGHT, -8, 4);
    lv_obj_set_flex_flow(meta, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(meta, 8, 0);
    lv_obj_set_flex_align(meta, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(meta, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    ui3_signal_dots(meta, 4);

    lo->cloud = lv_obj_create(meta);
    lv_obj_remove_style_all(lo->cloud);
    lv_obj_set_height(lo->cloud, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(lo->cloud, 28, 0);
    lv_obj_set_width(lo->cloud, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(lo->cloud, 14, 0);
    lv_obj_set_style_bg_color(lo->cloud, st->mqtt_online ? UI3_COL_CLOUD_BG : UI3_COL_PAPER_DEEP, 0);
    lv_obj_set_style_bg_opa(lo->cloud, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lo->cloud, 1, 0);
    lv_obj_set_style_border_color(lo->cloud, UI3_COL_SURFACE, 0);
    lv_obj_set_style_pad_hor(lo->cloud, 10, 0);
    lv_obj_set_style_pad_ver(lo->cloud, 5, 0);
    lv_obj_add_flag(lo->cloud, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_flex_flow(lo->cloud, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lo->cloud, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lo->cloud, 5, 0);

    lv_obj_t *dot = lv_obj_create(lo->cloud);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, st->mqtt_online ? UI3_COL_SAGE : UI3_COL_INK_FAINT, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

    lv_obj_t *cl = lv_label_create(lo->cloud);
    lv_label_set_text(cl, st->mqtt_online ? "云端" : "离线");
    lv_obj_set_style_text_font(cl, UI3_FONT_TITLE, 0);
    lv_obj_set_style_text_color(cl, st->mqtt_online ? UI3_COL_SAGE : UI3_COL_INK_FAINT, 0);
    lv_obj_set_style_pad_ver(cl, 0, 0);
}

void ui3_page_head(ui3_layout_t *lo, const char *title, const char *sub, bool tight)
{
    lv_obj_t *h = lv_label_create(lo->root);
    lv_label_set_text(h, title);
    lv_obj_add_style(h, ui3_style_title(), 0);
    lv_obj_align(h, LV_ALIGN_TOP_LEFT, 16, tight ? 34 : 36);

    if(sub && sub[0]) {
        lv_obj_t *p = lv_label_create(lo->root);
        lv_label_set_text(p, sub);
        lv_obj_add_style(p, ui3_style_label(), 0);
        lv_obj_set_style_text_color(p, UI3_COL_INK_FAINT, 0);
        lv_obj_align(p, LV_ALIGN_TOP_LEFT, 16, tight ? 58 : 60);
    }
}

lv_obj_t *ui3_body_fill(ui3_layout_t *lo, lv_coord_t top_y, lv_coord_t bottom_h)
{
    lo->body = lv_obj_create(lo->root);
    lv_obj_remove_style_all(lo->body);
    lv_obj_set_size(lo->body, UI3_W, (lv_coord_t)(UI3_H - top_y - bottom_h));
    lv_obj_align(lo->body, LV_ALIGN_TOP_MID, 0, top_y);
    lv_obj_clear_flag(lo->body, LV_OBJ_FLAG_SCROLLABLE);
    ui3_style_obj_paper(lo->body);

    /* body 后创建会盖住顶栏，必须把顶栏提到最前 */
    if(lo->top_time != NULL) {
        lv_obj_move_foreground(lo->top_time);
    }
    if(lo->cloud != NULL) {
        lv_obj_t *meta = lv_obj_get_parent(lo->cloud);
        if(meta != NULL) {
            lv_obj_move_foreground(meta);
        }
    }
    return lo->body;
}

lv_obj_t *ui3_scroll_viewport(ui3_layout_t *lo, int16_t h)
{
    lv_obj_t *vp = lv_obj_create(lo->root);
    lv_obj_remove_style_all(vp);
    lv_obj_set_size(vp, UI3_W - 32, h);
    lv_obj_align(vp, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_set_style_clip_corner(vp, true, 0);
    lv_obj_clear_flag(vp, LV_OBJ_FLAG_SCROLLABLE);

    lo->scroll_track = lv_obj_create(vp);
    lv_obj_remove_style_all(lo->scroll_track);
    lv_obj_set_width(lo->scroll_track, lv_pct(100));
    lv_obj_set_height(lo->scroll_track, LV_SIZE_CONTENT);
    lv_obj_set_pos(lo->scroll_track, 0, 0);
    lv_obj_clear_flag(lo->scroll_track, LV_OBJ_FLAG_SCROLLABLE);
    return lo->scroll_track;
}

lv_obj_t *ui3_scroll_dots(ui3_layout_t *lo, ui3_state_t *st, lv_coord_t y)
{
    lv_obj_t *dots = lv_obj_create(lo->root);
    lv_obj_remove_style_all(dots);
    lv_obj_set_size(dots, UI3_W, 12);
    lv_obj_align(dots, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 4, 0);

    uint8_t pages = st->scroll_max + 1u;
    if(pages < 2u) {
        lv_obj_add_flag(dots, LV_OBJ_FLAG_HIDDEN);
        return dots;
    }
    for(uint8_t i = 0; i < pages && i < 6u; i++) {
        lv_obj_t *d = lv_obj_create(dots);
        lv_obj_remove_style_all(d);
        if(i == st->scroll_px) {
            lv_obj_set_size(d, 14, 4);
            lv_obj_set_style_radius(d, 2, 0);
            lv_obj_set_style_bg_color(d, UI3_COL_TERRA, 0);
        } else {
            lv_obj_set_size(d, 4, 4);
            lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(d, UI3_COL_LINE, 0);
        }
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    }
    return dots;
}

static void ui3_dock_btn_apply(lv_obj_t *b, lv_obj_t *lbl, bool fill, bool sel)
{
    lv_coord_t w = lv_obj_get_width(b);
    lv_coord_t h = lv_obj_get_height(b);

  /* remove_style_all 会清掉宽高，导致 36×36 圆钮；先记住尺寸再恢复 */
    if(w < (UI3_DOCK_BTN_H + 8)) {
        w = UI3_DOCK_HOME_BTN_W;
    }
    if(h < UI3_DOCK_BTN_H) {
        h = UI3_DOCK_BTN_H;
    }

    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(b, fill ? ui3_style_btn_fill() : ui3_style_btn_ghost(), 0);
    lv_obj_set_style_radius(b, UI3_DOCK_BTN_RADIUS, 0);
    if(sel) {
        lv_obj_set_style_border_color(b, UI3_COL_TERRA, 0);
        lv_obj_set_style_border_width(b, 2, 0);
    }
    if(lbl) {
        lv_obj_set_style_text_font(lbl, UI3_FONT_TITLE, 0);
        lv_obj_set_style_text_color(lbl, fill ? UI3_COL_SURFACE : UI3_COL_INK_SOFT, 0);
    }
}

lv_obj_t *ui3_dock_btn(lv_obj_t *parent, const char *txt, bool fill, lv_coord_t w)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(b, UI3_DOCK_BTN_H);
    if(w > 0) {
        lv_obj_set_width(b, w);
    } else {
        lv_obj_set_width(b, UI3_DOCK_HOME_BTN_W);
        lv_obj_set_flex_grow(b, 1);
    }
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    ui3_dock_btn_apply(b, l, fill, false);
    return b;
}

void ui3_dock_btn_set_sel(lv_obj_t *btn, bool fill, bool sel)
{
    lv_obj_t *lbl;

    if(btn == NULL) {
        return;
    }
    lbl = lv_obj_get_child(btn, 0);
    ui3_dock_btn_apply(btn, lbl, fill, sel);
}

lv_obj_t *ui3_footer_dock(ui3_layout_t *lo, const char *ok, bool show_esc,
                          lv_event_cb_t on_ok, lv_event_cb_t on_esc, void *user,
                          bool primary_fill)
{
    lo->footer = lv_obj_create(lo->root);
    lv_obj_remove_style_all(lo->footer);
    lv_obj_set_size(lo->footer, UI3_W, 52);
    lv_obj_align(lo->footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(lo->footer, UI3_COL_PAPER, 0);
    lv_obj_set_style_bg_opa(lo->footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(lo->footer, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(lo->footer, UI3_COL_LINE_STRONG, 0);
    lv_obj_set_style_border_width(lo->footer, 1, 0);
    lv_obj_set_flex_flow(lo->footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lo->footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lo->footer, 10, 0);
    lv_obj_set_style_pad_hor(lo->footer, 14, 0);
    lv_obj_set_style_pad_top(lo->footer, 8, 0);

    if(show_esc) {
        lv_obj_t *esc = ui3_dock_btn(lo->footer, "返回", false, 52);
        if(on_esc) {
            lv_obj_add_event_cb(esc, on_esc, LV_EVENT_CLICKED, user);
        }
    }
    lv_obj_t *primary = ui3_dock_btn(lo->footer, ok, primary_fill, 0);
    lv_obj_set_flex_grow(primary, 1);
    if(on_ok) {
        lv_obj_add_event_cb(primary, on_ok, LV_EVENT_CLICKED, user);
    }
    return lo->footer;
}

lv_obj_t *ui3_footer_back(ui3_layout_t *lo, lv_event_cb_t on_esc, void *user)
{
    lo->footer = lv_obj_create(lo->root);
    lv_obj_remove_style_all(lo->footer);
    lv_obj_set_size(lo->footer, UI3_W, 52);
    lv_obj_align(lo->footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_hor(lo->footer, 14, 0);
    lv_obj_t *esc = ui3_dock_btn(lo->footer, "返回", false, 0);
    lv_obj_set_width(esc, lv_pct(100));
    if(on_esc) {
        lv_obj_add_event_cb(esc, on_esc, LV_EVENT_CLICKED, user);
    }
    return lo->footer;
}

lv_obj_t *ui3_footer_pair(ui3_layout_t *lo, lv_event_cb_t on_esc, lv_event_cb_t on_regen, void *user)
{
    lo->footer = lv_obj_create(lo->root);
    lv_obj_remove_style_all(lo->footer);
    lv_obj_set_size(lo->footer, UI3_W, 52);
    lv_obj_align(lo->footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(lo->footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(lo->footer, 10, 0);
    lv_obj_set_style_pad_hor(lo->footer, 14, 0);
    lv_obj_t *esc = ui3_dock_btn(lo->footer, "返回", false, 52);
    lv_obj_add_event_cb(esc, on_esc, LV_EVENT_CLICKED, user);
    lv_obj_t *reg = ui3_dock_btn(lo->footer, "重新生成", false, 0);
    lv_obj_set_flex_grow(reg, 1);
    lv_obj_add_event_cb(reg, on_regen, LV_EVENT_CLICKED, user);
    return lo->footer;
}

static void field_box_sync_caret(lv_obj_t *box, lv_obj_t *t, bool focus)
{
    lv_obj_t *caret = NULL;

    if(box == NULL || t == NULL) {
        return;
    }
    if(lv_obj_get_child_cnt(box) > 1u) {
        caret = lv_obj_get_child(box, 1);
    }
    if(focus) {
        if(caret == NULL || !lv_obj_is_valid(caret)) {
            caret = lv_obj_create(box);
            lv_obj_remove_style_all(caret);
            lv_obj_set_size(caret, 2, 14);
            lv_obj_set_style_bg_color(caret, UI3_COL_TERRA, 0);
            lv_obj_set_style_bg_opa(caret, LV_OPA_COVER, 0);
        }
        lv_obj_align_to(caret, t, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
    } else if(caret != NULL && lv_obj_is_valid(caret)) {
        lv_obj_del(caret);
    }
}

static void field_box_apply(lv_obj_t *box, const char *val, bool focus, bool disabled)
{
    lv_obj_t *t;

    if(box == NULL || !lv_obj_is_valid(box)) {
        return;
    }
    lv_obj_remove_style(box, ui3_style_field_focus(), 0);
    if(focus && !disabled) {
        lv_obj_add_style(box, ui3_style_field_focus(), 0);
    }
    if(disabled) {
        lv_obj_set_style_opa(box, LV_OPA_40, 0);
    } else {
        lv_obj_set_style_opa(box, LV_OPA_COVER, 0);
    }
    t = lv_obj_get_child(box, 0);
    if(t == NULL || !lv_obj_is_valid(t)) {
        return;
    }
    lv_label_set_text(t, val ? val : "");
    field_box_sync_caret(box, t, focus && !disabled);
}

static lv_obj_t *field_box_inner(lv_obj_t *parent, const char *val, bool focus, bool disabled,
                                 lv_event_cb_t on_tap, void *user)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_width(box, lv_pct(100));
    lv_obj_set_height(box, 36);
    lv_obj_add_style(box, ui3_style_field(), 0);
    lv_obj_t *t = lv_label_create(box);
    lv_label_set_text(t, val ? val : "");
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, 0);
    field_box_apply(box, val, focus, disabled);
    if(on_tap && !disabled) {
        lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(box, on_tap, LV_EVENT_CLICKED, user);
    }
    return box;
}

void ui3_inputs_reset(ui3_state_t *st)
{
    uint8_t i;

    if(st == NULL) {
        return;
    }
    st->inp_count = 0u;
    st->form_err_wrap = NULL;
    for(i = 0u; i < 2u; i++) {
        st->inp_wrap[i] = NULL;
    }
}

void ui3_inputs_track(ui3_state_t *st, uint8_t idx, lv_obj_t *wrap)
{
    if(st == NULL || wrap == NULL || idx >= 2u) {
        return;
    }
    st->inp_wrap[idx] = wrap;
    if((idx + 1u) > st->inp_count) {
        st->inp_count = (uint8_t)(idx + 1u);
    }
}

void ui3_field_wrap_update(lv_obj_t *wrap, const char *val, bool focus, bool disabled)
{
    lv_obj_t *box;

    if(wrap == NULL || !lv_obj_is_valid(wrap)) {
        return;
    }
    box = lv_obj_get_child(wrap, 1);
    field_box_apply(box, val, focus, disabled);
}

void ui3_pwd_mask_fill(char *buf, size_t sz, const char *pwd)
{
    size_t n;
    size_t i;
    size_t p = 0u;

    if(buf == NULL || sz == 0u) {
        return;
    }
    if(pwd == NULL) {
        pwd = "";
    }
    n = strlen(pwd);
    if(n == 0u) {
        buf[0] = '\0';
        return;
    }
    for(i = 0u; i < n && (p + 2u) < sz; i++) {
        buf[p++] = (char)0xC2;
        buf[p++] = (char)0xB7;
    }
    buf[p] = '\0';
}

bool ui3_inputs_live_refresh(ui3_state_t *st)
{
    char mask[32];
    bool locked;

    if(st == NULL || st->inp_count == 0u) {
        return false;
    }

    locked = (st->scr == UI3_SCR_UNLOCK && st->lockout_until_ms > HAL_GetTick());

    switch(st->scr) {
    case UI3_SCR_UNLOCK:
        if(st->inp_count < 2u) {
            return false;
        }
        ui3_field_wrap_update(st->inp_wrap[0], st->unlock_acc, st->unlock_field == 0u, locked);
        ui3_pwd_mask_fill(mask, sizeof(mask), st->unlock_pwd);
        ui3_field_wrap_update(st->inp_wrap[1], mask, st->unlock_field == 1u, locked);
        break;
    case UI3_SCR_ADMIN:
        if(st->inp_count < 2u) {
            return false;
        }
        ui3_field_wrap_update(st->inp_wrap[0], st->admin_acc, st->admin_field == 0u, false);
        ui3_pwd_mask_fill(mask, sizeof(mask), st->admin_pwd);
        ui3_field_wrap_update(st->inp_wrap[1], mask, st->admin_field == 1u, false);
        break;
    case UI3_SCR_SEARCH:
        ui3_field_wrap_update(st->inp_wrap[0], st->search_acc, true, false);
        break;
    case UI3_SCR_DEL_USER:
        ui3_field_wrap_update(st->inp_wrap[0], st->del_acc, true, false);
        break;
    case UI3_SCR_ADD_USER:
        if(st->inp_count < 2u || st->add_field >= 2u) {
            return false;
        }
        ui3_field_wrap_update(st->inp_wrap[0], st->add_acc, st->add_field == 0u, false);
        ui3_pwd_mask_fill(mask, sizeof(mask), st->add_pwd);
        ui3_field_wrap_update(st->inp_wrap[1], mask, st->add_field == 1u, false);
        break;
    default:
        return false;
    }

    if(st->form_err_wrap != NULL && lv_obj_is_valid(st->form_err_wrap)) {
        if(st->unlock_show_err == 0u && st->admin_show_err == 0u) {
            lv_obj_add_flag(st->form_err_wrap, LV_OBJ_FLAG_HIDDEN);
        }
    }
    return true;
}

lv_obj_t *ui3_field_block(lv_obj_t *parent, const char *label, const char *val, bool focus,
                          bool disabled, lv_event_cb_t on_tap, void *user)
{
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_remove_style_all(wrap);
    ui3_style_obj_transparent(wrap);
    lv_obj_set_width(wrap, lv_pct(100));
    lv_obj_set_height(wrap, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(wrap, 6, 0);
    lv_obj_set_style_pad_bottom(wrap, 10, 0);

    lv_obj_t *lab = lv_label_create(wrap);
    lv_label_set_text(lab, label);
    lv_obj_add_style(lab, ui3_style_label(), 0);
    lv_obj_set_style_text_color(lab, UI3_COL_INK_SOFT, 0);
    field_box_inner(wrap, val, focus, disabled, on_tap, user);
    return wrap;
}

lv_obj_t *ui3_role_picker(lv_obj_t *parent, bool is_admin, bool focus)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 36);
    lv_obj_set_style_bg_color(row, UI3_COL_PAPER_DEEP, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_style_pad_all(row, 3, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    if(focus) {
        lv_obj_add_style(row, ui3_style_field_focus(), 0);
    }
    lv_obj_t *u = ui3_dock_btn(row, "用户", !is_admin, 0);
    lv_obj_set_height(u, 30);
    lv_obj_t *a = ui3_dock_btn(row, "管理员", is_admin, 0);
    lv_obj_set_height(a, 30);
    if(is_admin) {
        lv_obj_set_style_bg_color(a, UI3_COL_SURFACE, 0);
        lv_obj_set_style_text_color(a, UI3_COL_INK, 0);
    } else {
        lv_obj_set_style_bg_color(u, UI3_COL_SURFACE, 0);
        lv_obj_set_style_text_color(u, UI3_COL_INK, 0);
    }
    return row;
}

void ui3_role_picker_set(lv_obj_t *row, bool is_admin)
{
    if(row == NULL) {
        return;
    }
    lv_obj_t *u = lv_obj_get_child(row, 0);
    lv_obj_t *a = lv_obj_get_child(row, 1);
    if(u && a) {
        lv_obj_set_style_bg_color(u, is_admin ? UI3_COL_INK : UI3_COL_SURFACE, 0);
        lv_obj_set_style_text_color(u, is_admin ? UI3_COL_SURFACE : UI3_COL_INK_SOFT, 0);
        lv_obj_set_style_bg_color(a, is_admin ? UI3_COL_SURFACE : UI3_COL_INK, 0);
        lv_obj_set_style_text_color(a, is_admin ? UI3_COL_INK : UI3_COL_SURFACE, 0);
    }
}

void ui3_card_set_sel(lv_obj_t *card, bool sel, bool menu_style)
{
    if(card == NULL) {
        return;
    }
    if(sel) {
        lv_obj_add_style(card, ui3_style_card_sel(), 0);
    } else {
        lv_obj_remove_style(card, ui3_style_card_sel(), 0);
        lv_obj_add_style(card, ui3_style_card(), 0);
    }
    if(menu_style) {
        lv_obj_t *chev = lv_obj_get_child(card, 2);
        if(chev) {
            lv_obj_set_style_text_color(chev, sel ? UI3_COL_TERRA : UI3_COL_INK_FAINT, 0);
        }
        lv_obj_t *ico = lv_obj_get_child(card, 0);
        if(ico) {
            lv_obj_set_style_border_color(ico, sel ? UI3_COL_TERRA : UI3_COL_LINE, 0);
        }
    }
}

lv_obj_t *ui3_list_card(lv_obj_t *parent, const char *title, const char *sub, bool sel)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_width(c, lv_pct(100));
    lv_obj_set_height(c, UI3_LIST_ROW_H - 6);
    lv_obj_add_style(c, sel ? ui3_style_card_sel() : ui3_style_card(), 0);

    lv_obj_t *t = lv_label_create(c);
    lv_label_set_text(t, title);
    lv_obj_add_style(t, ui3_style_label(), 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 4, -8);

    if(sub) {
        lv_obj_t *s = lv_label_create(c);
        lv_label_set_text(s, sub);
        lv_obj_add_style(s, ui3_style_label(), 0);
        lv_obj_set_style_text_color(s, UI3_COL_INK_FAINT, 0);
        lv_obj_align(s, LV_ALIGN_LEFT_MID, 4, 10);
    }
    return c;
}

lv_obj_t *ui3_menu_card(lv_obj_t *parent, const char *icon, const char *title,
                        const char *sub, bool sel, lv_event_cb_t on_click, void *user)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_width(c, lv_pct(100));
    lv_obj_set_height(c, UI3_LIST_ROW_H - 6);
    lv_obj_add_style(c, sel ? ui3_style_card_sel() : ui3_style_card(), 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    if(on_click) {
        lv_obj_add_event_cb(c, on_click, LV_EVENT_CLICKED, user);
    }

    lv_obj_t *ico = lv_obj_create(c);
    lv_obj_remove_style_all(ico);
    lv_obj_set_size(ico, 28, 28);
    lv_obj_align(ico, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_radius(ico, 9, 0);
    lv_obj_set_style_bg_color(ico, UI3_COL_PAPER, 0);
    lv_obj_set_style_bg_opa(ico, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ico, 1, 0);
    lv_obj_set_style_border_color(ico, sel ? UI3_COL_TERRA : UI3_COL_LINE, 0);
    lv_obj_t *il = lv_label_create(ico);
    lv_label_set_text(il, icon);
    lv_obj_set_style_text_color(il, sel ? UI3_COL_TERRA : UI3_COL_INK_SOFT, 0);
    lv_obj_center(il);

    lv_obj_t *col = lv_obj_create(c);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, 130, 36);
    lv_obj_align(col, LV_ALIGN_LEFT_MID, 38, 0);
    lv_obj_t *t = lv_label_create(col);
    lv_label_set_text(t, title);
    lv_obj_add_style(t, ui3_style_label(), 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 2);
    lv_obj_t *s = lv_label_create(col);
    lv_label_set_text(s, sub);
    lv_obj_add_style(s, ui3_style_label(), 0);
    lv_obj_set_style_text_color(s, UI3_COL_INK_FAINT, 0);
    lv_obj_align(s, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *chev = lv_label_create(c);
    lv_label_set_text(chev, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chev, sel ? UI3_COL_TERRA : UI3_COL_INK_FAINT, 0);
    lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -6, 0);
    return c;
}

static void keyhole_draw_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj;
    lv_draw_ctx_t *draw_ctx;
    lv_area_t a;
    lv_draw_rect_dsc_t dsc;
    lv_coord_t cx;
    lv_point_t pts[4];

    if(code != LV_EVENT_DRAW_MAIN) {
        return;
    }
    obj = lv_event_get_target(e);
    draw_ctx = lv_event_get_draw_ctx(e);
    lv_obj_get_content_coords(obj, &a);

    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = UI3_COL_TERRA;
    dsc.bg_opa = LV_OPA_90;
    dsc.border_width = 0;
    dsc.radius = LV_RADIUS_CIRCLE;

    cx = (lv_coord_t)(a.x1 + (lv_area_get_width(&a) - 13) / 2);
    a.x1 = cx;
    a.y2 = (lv_coord_t)(a.y1 + 12);
    a.x2 = (lv_coord_t)(cx + 12);
    lv_draw_rect(draw_ctx, &dsc, &a);

    dsc.radius = 0;
    cx = (lv_coord_t)(cx + 6);
    pts[0].x = (lv_coord_t)(cx - 2);
    pts[0].y = (lv_coord_t)(a.y2 + 1);
    pts[1].x = (lv_coord_t)(cx - 6);
    pts[1].y = (lv_coord_t)(a.y2 + 12);
    pts[2].x = (lv_coord_t)(cx + 6);
    pts[2].y = (lv_coord_t)(a.y2 + 12);
    pts[3].x = (lv_coord_t)(cx + 2);
    pts[3].y = (lv_coord_t)(a.y2 + 1);
    lv_draw_polygon(draw_ctx, &dsc, pts, 4);
}

static lv_obj_t *lock_sculpture(lv_obj_t *parent)
{
    lv_obj_t *lock = lv_obj_create(parent);
    lv_obj_remove_style_all(lock);
    ui3_style_obj_transparent(lock);
    lv_obj_set_size(lock, 96, 96);
    lv_obj_add_flag(lock, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *arc = lv_arc_create(lock);
    lv_obj_set_size(arc, 92, 92);
    lv_obj_center(arc);
    /* preview: stroke-dasharray 180 60 → 约 270° 弧段 + 90° 缺口，底环不绘制 */
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_angles(arc, 135, 45);
    lv_obj_set_style_arc_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, UI3_COL_TERRA, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    ui3_anim_arc_rotate(arc, 12000u);

    lv_obj_t *core = lv_obj_create(lock);
    lv_obj_remove_style_all(core);
    lv_obj_set_size(core, 56, 56);
    lv_obj_align(core, LV_ALIGN_CENTER, 0, 2);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(core, UI3_COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(core, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(core, UI3_COL_LINE_STRONG, 0);
    lv_obj_set_style_border_width(core, 1, 0);

    lv_obj_t *kh = lv_obj_create(core);
    lv_obj_remove_style_all(kh);
    ui3_style_obj_transparent(kh);
    lv_obj_set_size(kh, 14, 26);
    lv_obj_align(kh, LV_ALIGN_CENTER, 0, 1);
    lv_obj_add_event_cb(kh, keyhole_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_flag(kh, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    return lock;
}

static lv_obj_t *sense_hint(lv_obj_t *parent, const char *txt, bool delay)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    ui3_style_obj_transparent(row);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_width(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 7, 7);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, UI3_COL_TERRA, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    ui3_anim_opa_pulse(dot, LV_OPA_60, LV_OPA_COVER, delay ? 1800u : 2000u);
    lv_obj_t *l = lv_label_create(row);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, UI3_FONT_TITLE, 0);
    lv_obj_set_style_text_color(l, UI3_COL_INK_SOFT, 0);
    return row;
}

void ui3_home_build(ui3_layout_t *lo, ui3_state_t *st,
                    lv_event_cb_t on_menu, lv_event_cb_t on_unlock, lv_event_cb_t on_lock)
{
    lv_obj_t *body = ui3_body_fill(lo, UI3_TOPBAR_H, 52);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(body, 4, 0);
    lv_obj_add_flag(body, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    st->lbl_clock_big = lv_label_create(body);
    lv_obj_set_style_text_font(st->lbl_clock_big, UI3_FONT_CLOCK, 0);
    lv_obj_set_style_text_color(st->lbl_clock_big, UI3_COL_INK, 0);
    ui3_update_clock_label(st->lbl_clock_big);

    st->lbl_date = lv_label_create(body);
    lv_obj_set_style_text_font(st->lbl_date, UI3_FONT_TITLE, 0);
    lv_obj_set_style_text_color(st->lbl_date, UI3_COL_INK_SOFT, 0);
    lv_obj_set_style_text_letter_space(st->lbl_date, 1, 0);
    lv_obj_set_style_pad_top(st->lbl_date, 2, 0);
    lv_obj_set_style_pad_bottom(st->lbl_date, 14, 0);
    ui3_update_date_label(st->lbl_date);

    lv_obj_t *lock = lock_sculpture(body);
    lv_obj_set_style_pad_top(lock, 2, 0);
    lv_obj_set_style_pad_bottom(lock, 8, 0);
    lv_obj_add_flag(lock, LV_OBJ_FLAG_CLICKABLE);
    if(on_lock) {
        lv_obj_add_event_cb(lock, on_lock, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *sense = lv_obj_create(body);
    lv_obj_remove_style_all(sense);
    ui3_style_obj_transparent(sense);
    lv_obj_set_size(sense, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(sense, 16, 0);
    lv_obj_add_flag(sense, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_flex_flow(sense, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sense, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(sense, 16, 0);
    sense_hint(sense, "请刷卡", false);
    sense_hint(sense, "请按指纹", true);

    lo->footer = lv_obj_create(lo->root);
    lv_obj_remove_style_all(lo->footer);
    lv_obj_set_size(lo->footer, UI3_W, 52);
    lv_obj_align(lo->footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    ui3_style_obj_paper(lo->footer);
    lv_obj_clear_flag(lo->footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(lo->footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lo->footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(lo->footer, UI3_FOOTER_PAD_H, 0);
    lv_obj_set_style_pad_top(lo->footer, 8, 0);

    st->btn_home_menu = ui3_dock_btn(lo->footer, "菜单", false, UI3_DOCK_HOME_BTN_W);
    st->btn_home_unlock = ui3_dock_btn(lo->footer, "开锁", true, UI3_DOCK_HOME_BTN_W);
    if(on_menu) {
        lv_obj_add_event_cb(st->btn_home_menu, on_menu, LV_EVENT_CLICKED, (void *)(uintptr_t)0u);
    }
    if(on_unlock) {
        lv_obj_add_event_cb(st->btn_home_unlock, on_unlock, LV_EVENT_CLICKED, (void *)(uintptr_t)1u);
    }
    ui3_home_footer_sel(st);
}

void ui3_home_footer_sel(ui3_state_t *st)
{
    if(st == NULL) {
        return;
    }
    bool menu_sel = (st->home_nav_sel == 0u);
    bool unlock_sel = (st->home_nav_sel == 1u);
    ui3_dock_btn_set_sel(st->btn_home_menu, false, menu_sel);
    ui3_dock_btn_set_sel(st->btn_home_unlock, true, unlock_sel);
}

lv_obj_t *ui3_detail_card(lv_obj_t *parent, const char *k, const char *v,
                          bool show_chg, lv_event_cb_t on_chg, void *user)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_width(c, lv_pct(100));
    lv_obj_set_height(c, UI3_LIST_ROW_H - 6);
    lv_obj_add_style(c, ui3_style_card(), 0);

    lv_obj_t *col = lv_obj_create(c);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, 120, 36);
    lv_obj_align(col, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *kl = lv_label_create(col);
    lv_label_set_text(kl, k);
    lv_obj_add_style(kl, ui3_style_label(), 0);
    lv_obj_set_style_text_color(kl, UI3_COL_INK_FAINT, 0);
    lv_obj_align(kl, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *vl = lv_label_create(col);
    lv_label_set_text(vl, v);
    lv_obj_add_style(vl, ui3_style_label(), 0);
    lv_obj_align(vl, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    if(show_chg) {
        lv_obj_t *chip = ui3_dock_btn(c, "更改", false, 52);
        lv_obj_set_height(chip, 26);
        lv_obj_align(chip, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_set_style_radius(chip, 12, 0);
        if(on_chg) {
            lv_obj_add_event_cb(chip, on_chg, LV_EVENT_CLICKED, user);
        }
    }
    return c;
}

lv_obj_t *ui3_user_row(lv_obj_t *parent, const char *acc, bool admin, bool sel)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_width(c, lv_pct(100));
    lv_obj_set_height(c, UI3_LIST_ROW_H - 6);
    lv_obj_add_style(c, sel ? ui3_style_card_sel() : ui3_style_card(), 0);

    lv_obj_t *acc_l = lv_label_create(c);
    lv_label_set_text(acc_l, acc);
    lv_obj_add_style(acc_l, ui3_style_label(), 0);
    lv_obj_align(acc_l, LV_ALIGN_LEFT_MID, 4, -6);

    lv_obj_t *sub = lv_label_create(c);
    lv_label_set_text(sub, admin ? "管理员" : "用户");
    lv_obj_add_style(sub, ui3_style_label(), 0);
    lv_obj_set_style_text_color(sub, UI3_COL_INK_FAINT, 0);
    lv_obj_align(sub, LV_ALIGN_LEFT_MID, 4, 10);

    lv_obj_t *tag = lv_label_create(c);
    lv_label_set_text(tag, admin ? "管理员" : "用户");
    lv_obj_add_style(tag, ui3_style_label(), 0);
    lv_obj_set_style_text_color(tag, admin ? UI3_COL_GOLD : UI3_COL_TERRA, 0);
    lv_obj_align(tag, LV_ALIGN_RIGHT_MID, -6, 0);
    return c;
}

lv_obj_t *ui3_wifi_banner(lv_obj_t *parent, const char *ssid, const char *sub, uint8_t rssi)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_height(b, 44);
    lv_obj_add_style(b, ui3_style_card(), 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(b, 8, 0);
    lv_obj_set_style_pad_left(b, 8, 0);
    ui3_signal_dots(b, rssi);
    lv_obj_t *col = lv_obj_create(b);
    lv_obj_remove_style_all(col);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, 36);
    lv_obj_t *s = lv_label_create(col);
    lv_label_set_text(s, ssid);
    lv_obj_add_style(s, ui3_style_label(), 0);
    lv_obj_align(s, LV_ALIGN_TOP_LEFT, 0, 2);
    lv_obj_t *p = lv_label_create(col);
    lv_label_set_text(p, sub);
    lv_obj_add_style(p, ui3_style_label(), 0);
    lv_obj_set_style_text_color(p, UI3_COL_INK_FAINT, 0);
    lv_obj_align(p, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return b;
}

lv_obj_t *ui3_wifi_scan_link(lv_obj_t *parent, bool scanning, lv_event_cb_t cb, void *user)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, scanning ? "扫描中…" : "刷新列表");
    lv_obj_add_style(l, ui3_style_label(), 0);
    lv_obj_set_style_text_color(l, UI3_COL_TERRA, 0);
    lv_obj_add_flag(l, LV_OBJ_FLAG_CLICKABLE);
    if(cb) {
        lv_obj_add_event_cb(l, cb, LV_EVENT_CLICKED, user);
    }
    return l;
}

lv_obj_t *ui3_wifi_item(lv_obj_t *parent, const char *ssid, uint8_t rssi,
                        bool connected, bool sel, lv_event_cb_t cb, void *user)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_width(c, lv_pct(100));
    lv_obj_set_height(c, UI3_LIST_ROW_H - 6);
    lv_obj_add_style(c, sel ? ui3_style_card_sel() : ui3_style_card(), 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    if(cb) {
        lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, user);
    }
    ui3_signal_dots(c, rssi);
    lv_obj_t *nm = lv_label_create(c);
    lv_label_set_text(nm, ssid);
    lv_obj_add_style(nm, ui3_style_label(), 0);
    lv_obj_align(nm, LV_ALIGN_LEFT_MID, 24, 0);
    if(connected) {
        lv_obj_t *tg = lv_label_create(c);
        lv_label_set_text(tg, "已连接");
        lv_obj_add_style(tg, ui3_style_label(), 0);
        lv_obj_set_style_text_color(tg, UI3_COL_SAGE, 0);
        lv_obj_align(tg, LV_ALIGN_RIGHT_MID, -6, 0);
    }
    return c;
}

void ui3_pair_zone(ui3_layout_t *lo, ui3_state_t *st)
{
    lv_obj_t *zone = ui3_body_fill(lo, 70, 52);
    lv_obj_set_flex_flow(zone, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(zone, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *brand = lv_label_create(zone);
    lv_label_set_text(brand, "JoyfulZone");
    lv_obj_add_style(brand, ui3_style_title(), 0);
    lv_obj_set_style_text_color(brand, UI3_COL_TERRA, 0);

    lv_obj_t *cloud = lv_label_create(zone);
    lv_label_set_text(cloud, st->mqtt_online ? "云端在线" : "等待连接");
    lv_obj_add_style(cloud, ui3_style_label(), 0);
    lv_obj_set_style_text_color(cloud, st->mqtt_online ? UI3_COL_SAGE : UI3_COL_INK_FAINT, 0);

    lv_obj_t *row = lv_obj_create(zone);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, 40);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 5, 0);
    for(uint8_t i = 0; st->pair_code[i] != '\0' && i < 6u; i++) {
        lv_obj_t *ch = lv_obj_create(row);
        lv_obj_remove_style_all(ch);
        lv_obj_set_size(ch, 30, 38);
        lv_obj_add_style(ch, ui3_style_card(), 0);
        lv_obj_t *d = lv_label_create(ch);
        char one[2] = { st->pair_code[i], '\0' };
        lv_label_set_text(d, one);
        lv_obj_set_style_text_font(d, UI3_FONT_TITLE, 0);
        lv_obj_center(d);
    }
}

lv_obj_t *ui3_bio_block(lv_obj_t *parent, lv_event_cb_t on_fp, lv_event_cb_t on_nfc, void *user)
{
    lv_obj_t *bio = lv_obj_create(parent);
    lv_obj_remove_style_all(bio);
    lv_obj_set_width(bio, lv_pct(100));
    lv_obj_set_height(bio, 96);
    lv_obj_add_style(bio, ui3_style_card(), 0);

    lv_obj_t *bh = lv_label_create(bio);
    lv_label_set_text(bh, "生物凭证");
    lv_obj_add_style(bh, ui3_style_label(), 0);
    lv_obj_set_style_text_color(bh, UI3_COL_INK_FAINT, 0);
    lv_obj_align(bh, LV_ALIGN_TOP_LEFT, 4, 0);

    lv_obj_t *r1 = lv_obj_create(bio);
    lv_obj_remove_style_all(r1);
    lv_obj_set_size(r1, lv_pct(100), 28);
    lv_obj_align(r1, LV_ALIGN_LEFT_MID, 0, -8);
    lv_obj_add_flag(r1, LV_OBJ_FLAG_CLICKABLE);
    if(on_fp) {
        lv_obj_add_event_cb(r1, on_fp, LV_EVENT_CLICKED, user);
    }
    lv_obj_t *l1 = lv_label_create(r1);
    lv_label_set_text(l1, "指纹录入");
    lv_obj_add_style(l1, ui3_style_label(), 0);
    lv_obj_align(l1, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *a1 = lv_label_create(r1);
    lv_label_set_text(a1, "录入");
    lv_obj_add_style(a1, ui3_style_label(), 0);
    lv_obj_set_style_text_color(a1, UI3_COL_TERRA, 0);
    lv_obj_align(a1, LV_ALIGN_RIGHT_MID, -4, 0);

    lv_obj_t *r2 = lv_obj_create(bio);
    lv_obj_remove_style_all(r2);
    lv_obj_set_size(r2, lv_pct(100), 28);
    lv_obj_align(r2, LV_ALIGN_LEFT_MID, 0, 18);
    lv_obj_add_flag(r2, LV_OBJ_FLAG_CLICKABLE);
    if(on_nfc) {
        lv_obj_add_event_cb(r2, on_nfc, LV_EVENT_CLICKED, user);
    }
    lv_obj_t *l2 = lv_label_create(r2);
    lv_label_set_text(l2, "NFC 录入");
    lv_obj_add_style(l2, ui3_style_label(), 0);
    lv_obj_align(l2, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *a2 = lv_label_create(r2);
    lv_label_set_text(a2, "录入");
    lv_obj_add_style(a2, ui3_style_label(), 0);
    lv_obj_set_style_text_color(a2, UI3_COL_TERRA, 0);
    lv_obj_align(a2, LV_ALIGN_RIGHT_MID, -4, 0);
    return bio;
}

lv_obj_t *ui3_action_stack(lv_obj_t *parent, const char *primary, const char *danger,
                           lv_event_cb_t on_pri, lv_event_cb_t on_dan, void *user)
{
    lv_obj_t *st = lv_obj_create(parent);
    lv_obj_remove_style_all(st);
    lv_obj_set_width(st, lv_pct(100));
    lv_obj_set_height(st, 96);
    lv_obj_set_flex_flow(st, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(st, 8, 0);
    lv_obj_align(st, LV_ALIGN_TOP_MID, 0, 88);

    lv_obj_t *p = ui3_dock_btn(st, primary, true, 0);
    lv_obj_set_height(p, 42);
    lv_obj_set_width(p, lv_pct(100));
    if(on_pri) {
        lv_obj_add_event_cb(p, on_pri, LV_EVENT_CLICKED, user);
    }
    lv_obj_t *d = ui3_dock_btn(st, danger, false, 0);
    lv_obj_set_height(d, 42);
    lv_obj_set_width(d, lv_pct(100));
    lv_obj_set_style_text_color(d, UI3_COL_DANGER, 0);
    lv_obj_set_style_border_color(d, UI3_COL_DANGER, 0);
    lv_obj_set_style_border_width(d, 1, 0);
    if(on_dan) {
        lv_obj_add_event_cb(d, on_dan, LV_EVENT_CLICKED, user);
    }
    return st;
}

static lv_obj_t *form_hint_label(lv_obj_t *parent, const char *txt, bool lockout)
{
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_remove_style_all(wrap);
    ui3_style_obj_transparent(wrap);
    lv_obj_set_width(wrap, lv_pct(100));
    lv_obj_set_height(wrap, LV_SIZE_CONTENT);
    if(lockout) {
        lv_obj_set_style_bg_color(wrap, UI3_COL_TERRA_SOFT, 0);
        lv_obj_set_style_bg_opa(wrap, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(wrap, UI3_COL_TERRA, 0);
        lv_obj_set_style_border_width(wrap, 1, 0);
        lv_obj_set_style_border_opa(wrap, LV_OPA_30, 0);
        lv_obj_set_style_radius(wrap, 12, 0);
        lv_obj_set_style_pad_all(wrap, 10, 0);
    } else {
        lv_obj_set_style_bg_color(wrap, UI3_COL_DANGER, 0);
        lv_obj_set_style_bg_opa(wrap, LV_OPA_10, 0);
        lv_obj_set_style_border_side(wrap, LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_border_color(wrap, UI3_COL_DANGER, 0);
        lv_obj_set_style_border_width(wrap, 2, 0);
        lv_obj_set_style_radius(wrap, 8, 0);
        lv_obj_set_style_pad_all(wrap, 8, 0);
    }
    lv_obj_t *e = lv_label_create(wrap);
    lv_label_set_text(e, txt);
    lv_label_set_long_mode(e, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(e, UI3_FONT_HEAD, 0);
    lv_obj_set_style_text_color(e, lockout ? UI3_COL_TERRA : UI3_COL_DANGER, 0);
    if(lockout) {
        lv_obj_set_style_text_align(e, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(e, lv_pct(100));
    }
    return wrap;
}

lv_obj_t *ui3_form_zone(ui3_layout_t *lo, ui3_state_t *st,
                        const char *l0, const char *v0, uint8_t f0,
                        const char *l1, const char *v1, uint8_t f1,
                        const char *err, const char *lock_hint,
                        bool fields_locked, lv_event_cb_t on_field)
{
    lv_obj_t *zone = lv_obj_create(lo->root);
    lv_obj_t *w0;
    lv_obj_t *w1;

    lv_obj_remove_style_all(zone);
    ui3_style_obj_transparent(zone);
    lv_obj_set_width(zone, UI3_W - 32);
    lv_obj_set_height(zone, LV_SIZE_CONTENT);
    lv_obj_align(zone, LV_ALIGN_TOP_MID, 0, 82);
    lv_obj_set_flex_flow(zone, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(zone, 0, 0);

    w0 = ui3_field_block(zone, l0, v0, f0 != 0u, fields_locked, on_field, (void *)(uintptr_t)0u);
    ui3_inputs_track(st, 0u, w0);
    if(l1) {
        w1 = ui3_field_block(zone, l1, v1, f1 != 0u, fields_locked, on_field, (void *)(uintptr_t)1u);
        ui3_inputs_track(st, 1u, w1);
    }
    if(st != NULL) {
        if(err && err[0]) {
            st->form_err_wrap = form_hint_label(zone, err, false);
        } else {
            st->form_err_wrap = NULL;
        }
    } else if(err && err[0]) {
        form_hint_label(zone, err, false);
    }
    if(lock_hint && lock_hint[0]) {
        form_hint_label(zone, lock_hint, true);
    }
    return zone;
}
