#include "app_screen3_menu.h"

#include "gui_guider.h"
#include "ui_menu_popup_utils.h"

extern lv_ui guider_ui;
extern uint8_t g_screen3_menu_index;

void screen3_set_menu_selected(uint8_t idx)
{
    ui_screen3_set_menu_selected(idx, &g_screen3_menu_index,
                                 guider_ui.screen_3_btn_3, guider_ui.screen_3_btn_4,
                                 guider_ui.screen_3_btn_5, guider_ui.screen_3_btn_6,
                                 guider_ui.screen_3_btn_3_label, guider_ui.screen_3_btn_4_label,
                                 guider_ui.screen_3_btn_5_label, guider_ui.screen_3_btn_6_label);
}
