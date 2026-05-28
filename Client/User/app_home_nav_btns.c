#include "app_home_nav_btns.h"

#include "gui_guider.h"
#include "ui_common_utils.h"
#include "app_state.h"

void lock_btn_set_selected(uint8_t selected)
{
    ui_lock_btn_set_selected(guider_ui.screen_btn_2, guider_ui.screen_btn_3,
                             &g_lock_btn_selected, &g_menu_btn_selected, selected);
}

void menu_btn_set_selected(uint8_t selected)
{
    ui_menu_btn_set_selected(guider_ui.screen_btn_2, guider_ui.screen_btn_3,
                             &g_lock_btn_selected, &g_menu_btn_selected, selected);
}
