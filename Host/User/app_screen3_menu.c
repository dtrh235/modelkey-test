#include "app_screen3_menu.h"

#include "lvgl.h"
#include "gui_guider.h"
#include "ui_menu_popup_utils.h"

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_21);

extern lv_ui guider_ui;
extern uint8_t g_screen3_menu_index;

/* 仅第 5 项：与 1–4 相同思源宋体 21（setup 已设，此处防止运行时被其它字体覆盖） */
static void screen3_apply_item5_font(void)
{
    if(lv_obj_is_valid(guider_ui.screen_3_btn_7)) {
        lv_obj_set_style_text_font(guider_ui.screen_3_btn_7, &lv_font_SourceHanSerifSC_Regular_21,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if(lv_obj_is_valid(guider_ui.screen_3_btn_7_label)) {
        lv_obj_set_style_text_font(guider_ui.screen_3_btn_7_label, &lv_font_SourceHanSerifSC_Regular_21,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(guider_ui.screen_3_btn_7_label, LV_LABEL_LONG_WRAP);
    }
}

void screen3_apply_uniform_menu_style(void)
{
    screen3_apply_item5_font();
}

void screen3_set_menu_selected(uint8_t idx)
{
    screen3_apply_item5_font();
    ui_screen3_set_menu_selected(idx, &g_screen3_menu_index,
                                 guider_ui.screen_3_btn_3, guider_ui.screen_3_btn_4,
                                 guider_ui.screen_3_btn_5, guider_ui.screen_3_btn_6,
                                 guider_ui.screen_3_btn_7,
                                 guider_ui.screen_3_btn_3_label, guider_ui.screen_3_btn_4_label,
                                 guider_ui.screen_3_btn_5_label, guider_ui.screen_3_btn_6_label,
                                 guider_ui.screen_3_btn_7_label);
}
