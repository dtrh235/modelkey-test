#include "app_slave_ui_fixup.h"

#if APP_RS485_IS_SLAVE

#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"

extern lv_ui guider_ui;

static void hide_ok_esc_btn(lv_obj_t *btn, lv_obj_t *lbl)
{
    const char *txt;

    if(btn == NULL || !lv_obj_is_valid(btn)) {
        return;
    }
    if(lbl != NULL && lv_obj_is_valid(lbl)) {
        txt = lv_label_get_text(lbl);
        if(txt != NULL && (strcmp(txt, "OK") == 0 || strcmp(txt, "ESC") == 0)) {
            lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void app_slave_ui_hide_corner_ok_esc(void)
{
    /* screen_1 保留 OK/ESC，与主机开锁页触屏区域一致 */
    hide_ok_esc_btn(guider_ui.screen_2_btn_2, guider_ui.screen_2_btn_2_label);
    hide_ok_esc_btn(guider_ui.screen_2_btn_1, guider_ui.screen_2_btn_1_label);
    hide_ok_esc_btn(guider_ui.screen_3_btn_1, guider_ui.screen_3_btn_1_label);
    hide_ok_esc_btn(guider_ui.screen_4_btn_4, guider_ui.screen_4_btn_4_label);
    hide_ok_esc_btn(guider_ui.screen_5_btn_1, guider_ui.screen_5_btn_1_label);
    hide_ok_esc_btn(guider_ui.screen_6_btn_2, guider_ui.screen_6_btn_2_label);
    hide_ok_esc_btn(guider_ui.screen_7_btn_1, guider_ui.screen_7_btn_1_label);
    hide_ok_esc_btn(guider_ui.screen_7_btn_2, guider_ui.screen_7_btn_2_label);
    hide_ok_esc_btn(guider_ui.screen_8_btn_1, guider_ui.screen_8_btn_1_label);
    hide_ok_esc_btn(guider_ui.screen_8_btn_2, guider_ui.screen_8_btn_2_label);
    hide_ok_esc_btn(guider_ui.screen_9_btn_4, guider_ui.screen_9_btn_4_label);
    hide_ok_esc_btn(guider_ui.screen_10_btn_3, guider_ui.screen_10_btn_3_label);
}

#endif /* APP_RS485_IS_SLAVE */
