#include "app_screen3_menu.h"

#include "lvgl.h"
#include "gui_guider.h"
#include "ui_menu_popup_utils.h"

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_21);

extern lv_ui guider_ui;
extern uint8_t g_screen3_menu_index;

#define SCREEN3_MENU_PANEL_Y    55
#define SCREEN3_MENU_PANEL_H    218
#define SCREEN3_MENU_ROW_STEP   43
#define SCREEN3_MENU_ROW_H      30
#define SCREEN3_MENU_ROW_X      8
#define SCREEN3_MENU_ROW_W      220
#define SCREEN3_MENU_FIRST_Y    5

static lv_obj_t *s_menu_panel = NULL;
static lv_obj_t *s_btn_item6 = NULL;
static lv_obj_t *s_btn_item6_label = NULL;
static uint8_t s_menu_layout_done = 0u;

static lv_obj_t *screen3_all_btns[SCREEN3_MENU_ITEM_COUNT];
static lv_obj_t *screen3_all_lbls[SCREEN3_MENU_ITEM_COUNT];

static void screen3_menu_btn_style(lv_obj_t *btn, lv_obj_t *lbl)
{
    if(!lv_obj_is_valid(btn) || !lv_obj_is_valid(lbl)) {
        return;
    }
    lv_obj_set_size(btn, SCREEN3_MENU_ROW_W, SCREEN3_MENU_ROW_H);
    lv_obj_set_style_bg_opa(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(btn, &lv_font_SourceHanSerifSC_Regular_21, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_21, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(btn, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_width(lbl, LV_PCT(100));
}

static void screen3_create_item6(void)
{
    if(lv_obj_is_valid(s_btn_item6)) {
        return;
    }
    if(!lv_obj_is_valid(s_menu_panel)) {
        return;
    }

    s_btn_item6 = lv_btn_create(s_menu_panel);
    s_btn_item6_label = lv_label_create(s_btn_item6);
    lv_label_set_text(s_btn_item6_label, "6.App配对");
    lv_obj_set_pos(s_btn_item6, SCREEN3_MENU_ROW_X, (lv_coord_t)(SCREEN3_MENU_FIRST_Y + 5 * SCREEN3_MENU_ROW_STEP));
    lv_obj_set_style_pad_all(s_btn_item6, 0, LV_STATE_DEFAULT);
    screen3_menu_btn_style(s_btn_item6, s_btn_item6_label);
}

static lv_coord_t screen3_menu_scroll_max_y(void)
{
    lv_coord_t content_h;

    if(!lv_obj_is_valid(s_menu_panel)) {
        return 0;
    }
    content_h = (lv_coord_t)(SCREEN3_MENU_FIRST_Y + SCREEN3_MENU_ITEM_COUNT * SCREEN3_MENU_ROW_STEP);
    if(content_h <= SCREEN3_MENU_PANEL_H) {
        return 0;
    }
    return (lv_coord_t)(content_h - SCREEN3_MENU_PANEL_H);
}

static void screen3_clamp_menu_scroll(void)
{
    lv_coord_t max_y;
    lv_coord_t y;

    if(!lv_obj_is_valid(s_menu_panel)) {
        return;
    }
    max_y = screen3_menu_scroll_max_y();
    y = lv_obj_get_scroll_y(s_menu_panel);
    if(max_y <= 0) {
        if(y != 0) {
            lv_obj_scroll_to_y(s_menu_panel, 0, LV_ANIM_OFF);
        }
        return;
    }
    if(y < 0) {
        lv_obj_scroll_to_y(s_menu_panel, 0, LV_ANIM_OFF);
    } else if(y > max_y) {
        lv_obj_scroll_to_y(s_menu_panel, max_y, LV_ANIM_OFF);
    }
}

void screen3_init_scroll_layout(void)
{
    uint8_t i;
    lv_obj_t *btns[5];
    lv_obj_t *lbls[5];
    lv_coord_t scroll_keep = 0;

    if(!lv_obj_is_valid(guider_ui.screen_3_cont_1)) {
        return;
    }
    if(!lv_obj_is_valid(s_menu_panel)) {
        s_menu_layout_done = 0u;
        s_menu_panel = lv_obj_create(guider_ui.screen_3_cont_1);
        lv_obj_set_pos(s_menu_panel, 0, SCREEN3_MENU_PANEL_Y);
        lv_obj_set_size(s_menu_panel, 240, SCREEN3_MENU_PANEL_H);
        lv_obj_set_style_bg_opa(s_menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(s_menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(s_menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(s_menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(s_menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(s_menu_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_scroll_dir(s_menu_panel, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(s_menu_panel, LV_SCROLLBAR_MODE_OFF);
    } else {
        scroll_keep = lv_obj_get_scroll_y(s_menu_panel);
    }

    btns[0] = guider_ui.screen_3_btn_3;
    btns[1] = guider_ui.screen_3_btn_4;
    btns[2] = guider_ui.screen_3_btn_5;
    btns[3] = guider_ui.screen_3_btn_6;
    btns[4] = guider_ui.screen_3_btn_7;
    lbls[0] = guider_ui.screen_3_btn_3_label;
    lbls[1] = guider_ui.screen_3_btn_4_label;
    lbls[2] = guider_ui.screen_3_btn_5_label;
    lbls[3] = guider_ui.screen_3_btn_6_label;
    lbls[4] = guider_ui.screen_3_btn_7_label;

    for(i = 0u; i < 5u; i++) {
        if(lv_obj_is_valid(btns[i])) {
            lv_obj_set_parent(btns[i], s_menu_panel);
            lv_obj_set_pos(btns[i], SCREEN3_MENU_ROW_X, (lv_coord_t)(SCREEN3_MENU_FIRST_Y + i * SCREEN3_MENU_ROW_STEP));
            lv_obj_set_style_pad_all(btns[i], 0, LV_STATE_DEFAULT);
            screen3_menu_btn_style(btns[i], lbls[i]);
        }
    }

    screen3_create_item6();

    screen3_all_btns[0] = guider_ui.screen_3_btn_3;
    screen3_all_btns[1] = guider_ui.screen_3_btn_4;
    screen3_all_btns[2] = guider_ui.screen_3_btn_5;
    screen3_all_btns[3] = guider_ui.screen_3_btn_6;
    screen3_all_btns[4] = guider_ui.screen_3_btn_7;
    screen3_all_btns[5] = s_btn_item6;
    screen3_all_lbls[0] = guider_ui.screen_3_btn_3_label;
    screen3_all_lbls[1] = guider_ui.screen_3_btn_4_label;
    screen3_all_lbls[2] = guider_ui.screen_3_btn_5_label;
    screen3_all_lbls[3] = guider_ui.screen_3_btn_6_label;
    screen3_all_lbls[4] = guider_ui.screen_3_btn_7_label;
    screen3_all_lbls[5] = s_btn_item6_label;

    if(lv_obj_is_valid(guider_ui.screen_3_label_1)) {
        lv_obj_move_foreground(guider_ui.screen_3_label_1);
    }
    if(lv_obj_is_valid(guider_ui.screen_3_btn_1)) {
        lv_obj_move_foreground(guider_ui.screen_3_btn_1);
    }

    if(s_menu_layout_done == 0u) {
        lv_obj_scroll_to_y(s_menu_panel, 0, LV_ANIM_OFF);
        s_menu_layout_done = 1u;
    } else {
        lv_obj_scroll_to_y(s_menu_panel, scroll_keep, LV_ANIM_OFF);
    }
    screen3_clamp_menu_scroll();
}

void screen3_hide_menu_panel(void)
{
    if(lv_obj_is_valid(s_menu_panel)) {
        lv_obj_add_flag(s_menu_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

void screen3_show_menu_panel(void)
{
    if(lv_obj_is_valid(s_menu_panel)) {
        lv_obj_clear_flag(s_menu_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *screen3_menu_btn_by_index(uint8_t idx)
{
    if(idx >= SCREEN3_MENU_ITEM_COUNT) {
        return NULL;
    }
    return screen3_all_btns[idx];
}

void screen3_scroll_to_item(uint8_t idx)
{
    lv_obj_t *row;

    if(!lv_obj_is_valid(s_menu_panel) || idx >= SCREEN3_MENU_ITEM_COUNT) {
        return;
    }
    row = screen3_all_btns[idx];
    if(!lv_obj_is_valid(row)) {
        return;
    }
    lv_obj_scroll_to_view(row, LV_ANIM_OFF);
    screen3_clamp_menu_scroll();
}

void screen3_list_scroll_by(lv_coord_t dy)
{
    lv_coord_t max_y;
    lv_coord_t y;
    int32_t ny;

    if(!lv_obj_is_valid(s_menu_panel) || dy == 0) {
        return;
    }
    max_y = screen3_menu_scroll_max_y();
    if(max_y <= 0) {
        lv_obj_scroll_to_y(s_menu_panel, 0, LV_ANIM_OFF);
        return;
    }
    y = lv_obj_get_scroll_y(s_menu_panel);
    ny = (int32_t)y + (int32_t)dy;
    if(ny < 0) {
        ny = 0;
    } else if(ny > (int32_t)max_y) {
        ny = (int32_t)max_y;
    }
    lv_obj_scroll_to_y(s_menu_panel, (lv_coord_t)ny, LV_ANIM_OFF);
}

uint8_t screen3_point_in_menu_panel(lv_coord_t x, lv_coord_t y)
{
    lv_area_t a;
    lv_point_t p;

    if(!lv_obj_is_valid(s_menu_panel) || lv_obj_has_flag(s_menu_panel, LV_OBJ_FLAG_HIDDEN)) {
        return 0u;
    }
    lv_obj_get_coords(s_menu_panel, &a);
    p.x = x;
    p.y = y;
    return _lv_area_is_point_on(&a, &p, 0) ? 1u : 0u;
}

void screen3_apply_uniform_menu_style(void)
{
    uint8_t i;

    for(i = 0u; i < SCREEN3_MENU_ITEM_COUNT; i++) {
        if(lv_obj_is_valid(screen3_all_btns[i]) && lv_obj_is_valid(screen3_all_lbls[i])) {
            lv_obj_set_style_text_font(screen3_all_btns[i], &lv_font_SourceHanSerifSC_Regular_21,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(screen3_all_lbls[i], &lv_font_SourceHanSerifSC_Regular_21,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void screen3_set_menu_selected(uint8_t idx)
{
    uint8_t prev = g_screen3_menu_index;

    screen3_init_scroll_layout();
    screen3_apply_uniform_menu_style();
    ui_screen3_set_menu_selected(idx, &g_screen3_menu_index,
                                 screen3_all_btns[0], screen3_all_btns[1], screen3_all_btns[2],
                                 screen3_all_btns[3], screen3_all_btns[4], screen3_all_btns[5],
                                 screen3_all_lbls[0], screen3_all_lbls[1], screen3_all_lbls[2],
                                 screen3_all_lbls[3], screen3_all_lbls[4], screen3_all_lbls[5]);
    if(idx != prev) {
        screen3_scroll_to_item(idx);
    }
}
