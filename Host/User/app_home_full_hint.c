#include "app_home_full_hint.h"

#include "app_unlock_flash_queue.h"
#include "app_state.h"
#include "app_screen.h"
#include "app_config.h"
#include "lvgl.h"
#if (APP_UI_V3_ENABLE == 1)
#include "ui_v3/app_ui_v3_fonts.h"
#endif

static lv_obj_t *s_lbl_full;

void app_home_full_hint_init(lv_obj_t *screen)
{
    if(screen == NULL) {
        return;
    }
    s_lbl_full = lv_label_create(screen);
    lv_label_set_text(s_lbl_full, "\xe5\xb7\xb2\xe6\xbb\xa1");
    lv_obj_set_pos(s_lbl_full, 4, 4);
    lv_obj_set_style_text_color(s_lbl_full, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
#if (APP_UI_V3_ENABLE == 1)
    lv_obj_set_style_text_font(s_lbl_full, UI3_FONT_BODY, LV_PART_MAIN | LV_STATE_DEFAULT);
#else
    LV_FONT_DECLARE(lv_font_cn_home_20);
    lv_obj_set_style_text_font(s_lbl_full, &lv_font_cn_home_20, LV_PART_MAIN | LV_STATE_DEFAULT);
#endif
    lv_obj_add_flag(s_lbl_full, LV_OBJ_FLAG_HIDDEN);
}

void app_home_full_hint_update(void)
{
    if(s_lbl_full == NULL || !lv_obj_is_valid(s_lbl_full)) {
        return;
    }
#if (APP_LEGACY_UI_ENABLE != 0)
    if(g_app_scr != APP_SCR_HOME) {
        lv_obj_add_flag(s_lbl_full, LV_OBJ_FLAG_HIDDEN);
        return;
    }
#endif
    if(app_unlock_flash_is_nearly_full() != 0u) {
        lv_obj_clear_flag(s_lbl_full, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_lbl_full, LV_OBJ_FLAG_HIDDEN);
    }
}
