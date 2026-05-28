#include "ui_screen8_utils.h"

lv_obj_t *ui_screen8_focus_get_ta(uint8_t focus, lv_obj_t *ta1, lv_obj_t *ta2)
{
    if(focus == 0u) return ta1;
    if(focus == 1u) return ta2;
    return NULL;
}

void ui_screen8_hide_status_labels(lv_obj_t *label_5, lv_obj_t *label_6)
{
    if(lv_obj_is_valid(label_5)) {
        lv_obj_add_flag(label_5, LV_OBJ_FLAG_HIDDEN);
    }
    if(lv_obj_is_valid(label_6)) {
        lv_obj_add_flag(label_6, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_screen8_clear_error_label(lv_obj_t *label_5)
{
    if(lv_obj_is_valid(label_5)) {
        lv_obj_add_flag(label_5, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_screen8_style_ta(lv_obj_t *ta, uint8_t selected)
{
    if(!lv_obj_is_valid(ta)) return;
    lv_obj_set_style_border_color(ta, selected ? lv_color_hex(0x006bb3) : lv_color_hex(0xe6e6e6),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ta, selected ? 3 : 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(ta, 0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_width(ta, 0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ta, 0, LV_PART_CURSOR | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ta, 0, LV_PART_CURSOR | LV_STATE_DEFAULT);
}

void ui_screen8_style_btn(lv_obj_t *btn, lv_obj_t *lbl, uint8_t selected)
{
    if(!lv_obj_is_valid(btn)) return;
    lv_obj_set_style_bg_opa(btn, selected ? 220 : 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, selected ? lv_color_hex(0x006bb3) : lv_color_hex(0x009bff),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    if(lv_obj_is_valid(lbl)) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}
