#include "app_screen4_table.h"

#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "app_user_ops.h"
#include "user_auth_portable.h"

extern lv_ui guider_ui;
extern user_cred_t g_users[APP_USER_MAX];
extern uint8_t g_user_count;

#define SCR4_ROW_H          34
#define SCR4_LIST_PANEL_X   0
#define SCR4_LIST_PANEL_Y   48
#define SCR4_LIST_PANEL_W   240
#define SCR4_LIST_PANEL_H   220
#define SCR4_HDR_H          28
#define SCR4_ROW_MAX        APP_USER_MAX

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20);

static lv_obj_t *s_hdr_bar = NULL;
static lv_obj_t *s_list_panel = NULL;
static lv_obj_t *s_empty_hint = NULL;
static lv_obj_t *s_row_objs[SCR4_ROW_MAX];
static uint8_t s_row_count = 0u;

static void screen4_clear_rows(void)
{
    uint8_t i;

    for(i = 0u; i < s_row_count; i++) {
        if(s_row_objs[i] != NULL && lv_obj_is_valid(s_row_objs[i])) {
            lv_obj_del(s_row_objs[i]);
        }
        s_row_objs[i] = NULL;
    }
    s_row_count = 0u;
    s_empty_hint = NULL;
    if(s_list_panel != NULL && lv_obj_is_valid(s_list_panel)) {
        lv_obj_clean(s_list_panel);
    }
}

static void screen4_hide_empty_hint(void)
{
    if(s_empty_hint != NULL && lv_obj_is_valid(s_empty_hint)) {
        lv_obj_add_flag(s_empty_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

static void screen4_show_empty_hint(void)
{
    if(s_list_panel == NULL || !lv_obj_is_valid(s_list_panel)) {
        return;
    }
    if(s_empty_hint == NULL || !lv_obj_is_valid(s_empty_hint)) {
        s_empty_hint = lv_label_create(s_list_panel);
        lv_label_set_text(s_empty_hint, "无用户");
        lv_obj_set_style_text_font(s_empty_hint, &lv_font_SourceHanSerifSC_Regular_20,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(s_empty_hint, lv_color_hex(0x666666),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(s_empty_hint, LV_ALIGN_CENTER, 0, 0);
    }
    lv_obj_clear_flag(s_empty_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_empty_hint);
}

static void screen4_list_panel_setup(void)
{
    lv_obj_t *parent;
    lv_obj_t *lbl_acc;
    lv_obj_t *lbl_role;

    if(s_list_panel != NULL && lv_obj_is_valid(s_list_panel)) {
        return;
    }

    parent = guider_ui.screen_4_cont_1;
    if(parent == NULL || !lv_obj_is_valid(parent)) {
        return;
    }

    if(lv_obj_is_valid(guider_ui.screen_4_table_1)) {
        lv_obj_add_flag(guider_ui.screen_4_table_1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(guider_ui.screen_4_table_1, LV_OBJ_FLAG_CLICKABLE);
    }

    s_hdr_bar = lv_obj_create(parent);
    lv_obj_set_pos(s_hdr_bar, SCR4_LIST_PANEL_X, SCR4_LIST_PANEL_Y);
    lv_obj_set_size(s_hdr_bar, SCR4_LIST_PANEL_W, SCR4_HDR_H);
    lv_obj_clear_flag(s_hdr_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_hdr_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_hdr_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_hdr_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_hdr_bar, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s_hdr_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl_acc = lv_label_create(s_hdr_bar);
    lv_label_set_text(lbl_acc, "账号");
    lv_obj_set_pos(lbl_acc, 8, 4);
    lv_obj_set_style_text_font(lbl_acc, &lv_font_SourceHanSerifSC_Regular_20,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl_role = lv_label_create(s_hdr_bar);
    lv_label_set_text(lbl_role, "身份");
    lv_obj_set_pos(lbl_role, 128, 4);
    lv_obj_set_style_text_font(lbl_role, &lv_font_SourceHanSerifSC_Regular_20,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    s_list_panel = lv_obj_create(parent);
    lv_obj_set_pos(s_list_panel, SCR4_LIST_PANEL_X, (lv_coord_t)(SCR4_LIST_PANEL_Y + SCR4_HDR_H));
    lv_obj_set_size(s_list_panel, SCR4_LIST_PANEL_W,
                    (lv_coord_t)(SCR4_LIST_PANEL_H - SCR4_HDR_H));
    lv_obj_set_layout(s_list_panel, 0);
    lv_obj_add_flag(s_list_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_list_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list_panel, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(s_list_panel, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(s_list_panel, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_style_pad_all(s_list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_list_panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_list_panel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_scroll_to_y(s_list_panel, 0, LV_ANIM_OFF);
}

static lv_coord_t screen4_list_scroll_max_y(void)
{
    lv_coord_t content_h;

    if(s_list_panel == NULL || !lv_obj_is_valid(s_list_panel) || s_row_count == 0u) {
        return 0;
    }
    content_h = (lv_coord_t)((lv_coord_t)s_row_count * SCR4_ROW_H);
    if(content_h <= lv_obj_get_height(s_list_panel)) {
        return 0;
    }
    return (lv_coord_t)(content_h - lv_obj_get_height(s_list_panel));
}

static void screen4_clamp_list_scroll(void)
{
    lv_coord_t y;
    lv_coord_t max_y;

    if(s_list_panel == NULL || !lv_obj_is_valid(s_list_panel)) {
        return;
    }
    max_y = screen4_list_scroll_max_y();
    y = lv_obj_get_scroll_y(s_list_panel);
    if(y < 0) {
        lv_obj_scroll_to_y(s_list_panel, 0, LV_ANIM_OFF);
    } else if(max_y > 0 && y > max_y) {
        lv_obj_scroll_to_y(s_list_panel, max_y, LV_ANIM_OFF);
    } else if(max_y <= 0 && y != 0) {
        lv_obj_scroll_to_y(s_list_panel, 0, LV_ANIM_OFF);
    }
}

static void screen4_append_row(const char *acc, const char *role)
{
    lv_obj_t *row;
    lv_obj_t *lbl_acc;
    lv_obj_t *lbl_role;

    if(s_list_panel == NULL || !lv_obj_is_valid(s_list_panel) ||
       acc == NULL || s_row_count >= SCR4_ROW_MAX) {
        return;
    }

    screen4_hide_empty_hint();

    row = lv_obj_create(s_list_panel);
    lv_obj_set_pos(row, 0, (lv_coord_t)((lv_coord_t)s_row_count * SCR4_ROW_H));
    lv_obj_set_size(row, SCR4_LIST_PANEL_W, SCR4_ROW_H);
    lv_obj_set_style_pad_all(row, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    lbl_acc = lv_label_create(row);
    lv_label_set_text(lbl_acc, acc);
    lv_label_set_long_mode(lbl_acc, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl_acc, 110);
    lv_obj_set_pos(lbl_acc, 8, 6);
    lv_obj_set_style_text_font(lbl_acc, &lv_font_SourceHanSerifSC_Regular_20,
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl_role = lv_label_create(row);
    lv_label_set_text(lbl_role, role);
    lv_label_set_long_mode(lbl_role, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl_role, 90);
    lv_obj_set_pos(lbl_role, 128, 6);
    lv_obj_set_style_text_font(lbl_role, &lv_font_SourceHanSerifSC_Regular_20,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    s_row_objs[s_row_count] = row;
    s_row_count++;
}

void screen4_refresh_table(void)
{
    uint8_t i;
    uint8_t n = 0u;

    screen4_list_panel_setup();
    screen4_clear_rows();

    if(s_list_panel == NULL || !lv_obj_is_valid(s_list_panel)) {
        return;
    }

    for(i = 0u; i < g_user_count; i++) {
        if(g_users[i].active != 0u) {
            n++;
        }
    }

    if(n == 0u) {
        screen4_show_empty_hint();
        lv_obj_scroll_to_y(s_list_panel, 0, LV_ANIM_OFF);
        return;
    }

    for(i = 0u; i < g_user_count; i++) {
        if(g_users[i].active == 0u) {
            continue;
        }
        screen4_append_row(g_users[i].acc, g_users[i].is_admin ? "管理员" : "用户");
    }

    lv_obj_scroll_to_y(s_list_panel, 0, LV_ANIM_OFF);
    screen4_clamp_list_scroll();
}

void screen4_list_scroll_by(lv_coord_t dy)
{
    lv_coord_t max_y;
    int32_t ny;
    lv_coord_t y;

    if(s_list_panel == NULL || !lv_obj_is_valid(s_list_panel)) {
        return;
    }
    if(dy == 0 || s_row_count == 0u) {
        return;
    }
    max_y = screen4_list_scroll_max_y();
    if(max_y <= 0) {
        lv_obj_scroll_to_y(s_list_panel, 0, LV_ANIM_OFF);
        return;
    }
    if(dy > 24) {
        dy = 24;
    } else if(dy < -24) {
        dy = -24;
    }
    y = lv_obj_get_scroll_y(s_list_panel);
    ny = (int32_t)y - (int32_t)dy * 2;
    if(ny < 0) {
        ny = 0;
    } else if(ny > (int32_t)max_y) {
        ny = (int32_t)max_y;
    }
    lv_obj_scroll_to_y(s_list_panel, (lv_coord_t)ny, LV_ANIM_OFF);
}

void screen4_handle_table_key(KeyValue_t key)
{
    if(key == KEY_UP) {
        screen4_list_scroll_by(-12);
    } else if(key == KEY_DOWN) {
        screen4_list_scroll_by(12);
    }
}

uint8_t screen4_point_in_list(lv_coord_t x, lv_coord_t y)
{
    lv_area_t a;
    lv_point_t p;

    if(s_list_panel == NULL || !lv_obj_is_valid(s_list_panel)) {
        return 0u;
    }
    lv_obj_get_coords(s_list_panel, &a);
    p.x = x;
    p.y = y;
    return _lv_area_is_point_on(&a, &p, 0) ? 1u : 0u;
}
