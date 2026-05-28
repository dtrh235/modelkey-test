#include "app_screen1_unlock.h"

#include "lvgl.h"
#include "gui_guider.h"
#include "ui.h"
#include "app_screen.h"
#include "app_state.h"
#include "app_home_nav_btns.h"

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20);

#define APP_SCREEN1_UNLOCK_POPUP_MS 2500u
#define APP_SCREEN1_UNLOCK_LETTER_SPACE 4

void screen1_cancel_unlock_popup(void)
{
    if(g_screen1_unlock_timer != NULL) {
        lv_timer_del(g_screen1_unlock_timer);
        g_screen1_unlock_timer = NULL;
    }
    if(g_screen1_unlock_popup != NULL && lv_obj_is_valid(g_screen1_unlock_popup)) {
        lv_obj_del(g_screen1_unlock_popup);
    }
    g_screen1_unlock_popup = NULL;
}

static void screen1_return_home_after_unlock(void)
{
    if(g_screen1_unlock_popup != NULL && lv_obj_is_valid(g_screen1_unlock_popup)) {
        lv_obj_del(g_screen1_unlock_popup);
        g_screen1_unlock_popup = NULL;
    }
    ui_load_scr_animation(&guider_ui, &guider_ui.screen, guider_ui.screen_del, &guider_ui.screen_1_del,
                          setup_scr_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
    g_app_scr = APP_SCR_HOME;
    g_screen1_field_index = 0;
    g_cursor_visible = 1;
    g_screen1_cursor = NULL;
    lock_btn_set_selected(0);
    menu_btn_set_selected(0);
}

static void screen1_unlock_timer_cb(lv_timer_t *t)
{
    (void)t;
    g_screen1_unlock_timer = NULL;
    screen1_return_home_after_unlock();
}

void screen1_show_unlock_popup(void)
{
    lv_obj_t *mask;
    lv_obj_t *panel;
    lv_obj_t *lbl;

    if(!lv_obj_is_valid(guider_ui.screen_1)) return;

    screen1_cancel_unlock_popup();

    mask = lv_obj_create(guider_ui.screen_1);
    g_screen1_unlock_popup = mask;
    lv_obj_set_size(mask, 240, 320);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_set_scrollbar_mode(mask, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(mask, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(mask, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(mask, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    panel = lv_obj_create(mask);
    lv_obj_set_size(panel, 180, 100);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(panel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl = lv_label_create(panel);
    lv_label_set_text(lbl, "\xE5\xBC\x80\xE9\x94\x81\xE6\x88\x90\xE5\x8A\x9F");
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl, 160);
    lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x0D3055), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(lbl, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(lbl, APP_SCREEN1_UNLOCK_LETTER_SPACE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(lbl);

    lv_obj_move_foreground(mask);
    g_screen1_unlock_timer = lv_timer_create(screen1_unlock_timer_cb, APP_SCREEN1_UNLOCK_POPUP_MS, NULL);
    if(g_screen1_unlock_timer != NULL) {
        lv_timer_set_repeat_count(g_screen1_unlock_timer, 1);
    }
}
