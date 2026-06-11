#include "ui_menu_popup_utils.h"

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_21);

lv_obj_t *ui_menu_btn_by_index6(lv_obj_t *btn0, lv_obj_t *btn1, lv_obj_t *btn2, lv_obj_t *btn3, lv_obj_t *btn4,
                                lv_obj_t *btn5, uint8_t idx)
{
    switch(idx) {
    case 0u: return btn0;
    case 1u: return btn1;
    case 2u: return btn2;
    case 3u: return btn3;
    case 4u: return btn4;
    default: return btn5;
    }
}

lv_obj_t *ui_menu_label_by_index6(lv_obj_t *lbl0, lv_obj_t *lbl1, lv_obj_t *lbl2, lv_obj_t *lbl3, lv_obj_t *lbl4,
                                  lv_obj_t *lbl5, uint8_t idx)
{
    switch(idx) {
    case 0u: return lbl0;
    case 1u: return lbl1;
    case 2u: return lbl2;
    case 3u: return lbl3;
    case 4u: return lbl4;
    default: return lbl5;
    }
}

void ui_screen3_set_menu_selected(uint8_t idx, uint8_t *menu_index,
                                  lv_obj_t *btn0, lv_obj_t *btn1, lv_obj_t *btn2, lv_obj_t *btn3, lv_obj_t *btn4,
                                  lv_obj_t *btn5,
                                  lv_obj_t *lbl0, lv_obj_t *lbl1, lv_obj_t *lbl2, lv_obj_t *lbl3, lv_obj_t *lbl4,
                                  lv_obj_t *lbl5)
{
    uint8_t i;
    lv_obj_t *btn;
    lv_obj_t *lbl;

    if(menu_index == NULL) {
        return;
    }
    if(idx >= SCREEN3_MENU_ITEM_COUNT) {
        idx = 0u;
    }
    *menu_index = idx;

    for(i = 0u; i < SCREEN3_MENU_ITEM_COUNT; i++) {
        btn = ui_menu_btn_by_index6(btn0, btn1, btn2, btn3, btn4, btn5, i);
        lbl = ui_menu_label_by_index6(lbl0, lbl1, lbl2, lbl3, lbl4, lbl5, i);
        if(!lv_obj_is_valid(btn) || !lv_obj_is_valid(lbl)) {
            continue;
        }

        lv_obj_set_style_text_font(btn, &lv_font_SourceHanSerifSC_Regular_21, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_21, LV_PART_MAIN | LV_STATE_DEFAULT);

        if(i == *menu_index) {
            lv_obj_set_style_bg_opa(btn, 220, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(btn, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_bg_opa(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xb1e8fb), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void ui_popup_btn_style(lv_obj_t *btn, lv_obj_t *lbl, uint8_t selected)
{
    if(!lv_obj_is_valid(btn)) return;
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, selected ? lv_color_hex(0x006bb3) : lv_color_hex(0xe6e6e6),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    if(lv_obj_is_valid(lbl)) {
        lv_obj_set_style_text_color(lbl, selected ? lv_color_hex(0xffffff) : lv_color_hex(0x4e4e4e),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void ui_popup_set_yes_no_choice(lv_obj_t *btn_yes, lv_obj_t *btn_no, uint8_t yes_selected, uint8_t *choice_yes)
{
    if(choice_yes != NULL) *choice_yes = yes_selected ? 1u : 0u;
    if(lv_obj_is_valid(btn_yes) && lv_obj_is_valid(btn_no)) {
        ui_popup_btn_style(btn_yes, lv_obj_get_child(btn_yes, 0), yes_selected ? 1u : 0u);
        ui_popup_btn_style(btn_no, lv_obj_get_child(btn_no, 0), yes_selected ? 0u : 1u);
    }
}
