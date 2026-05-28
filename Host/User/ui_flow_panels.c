#include "ui_flow_panels.h"

#include "ui_menu_popup_utils.h"

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_18);
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20);

void ui_screen6_set_focus_style(uint8_t focus, uint8_t *focus_state,
                                lv_obj_t *btn_acc, lv_obj_t *btn_pwd, lv_obj_t *btn_fp, lv_obj_t *btn_nfc,
                                lv_obj_t *dd)
{
    if(focus > 3u) focus = 0u;
    if(focus_state != NULL) *focus_state = focus;

    if(lv_obj_is_valid(btn_acc)) {
        lv_obj_set_style_bg_color(btn_acc, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btn_acc, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if(lv_obj_is_valid(btn_pwd)) {
        lv_obj_set_style_bg_color(btn_pwd, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btn_pwd, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if(lv_obj_is_valid(btn_fp)) {
        lv_obj_set_style_bg_color(btn_fp, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btn_fp, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if(lv_obj_is_valid(btn_nfc)) {
        lv_obj_set_style_bg_color(btn_nfc, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btn_nfc, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if(focus == 0u && lv_obj_is_valid(btn_pwd)) {
        lv_obj_set_style_bg_color(btn_pwd, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btn_pwd, 220, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if(focus == 2u && lv_obj_is_valid(btn_fp)) {
        lv_obj_set_style_bg_color(btn_fp, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btn_fp, 220, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if(focus == 3u && lv_obj_is_valid(btn_nfc)) {
        lv_obj_set_style_bg_color(btn_nfc, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(btn_nfc, 220, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if(lv_obj_is_valid(dd)) {
        if(focus == 1u) {
            lv_obj_set_style_border_width(dd, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(dd, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_border_width(dd, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(dd, lv_color_hex(0xe1e6ee), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void ui_screen9_set_focus_style(uint8_t focus, uint8_t *focus_state, lv_obj_t *btn_replace, lv_obj_t *btn_delete)
{
    if(focus > 1u) focus = 0u;
    if(focus_state != NULL) *focus_state = focus;

    if(lv_obj_is_valid(btn_replace)) {
        lv_obj_set_style_bg_opa(btn_replace, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_replace, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if(lv_obj_is_valid(btn_delete)) {
        lv_obj_set_style_bg_opa(btn_delete, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if(focus == 0u && lv_obj_is_valid(btn_replace)) {
        lv_obj_set_style_bg_opa(btn_replace, 160, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_replace, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if(focus == 1u && lv_obj_is_valid(btn_delete)) {
        lv_obj_set_style_bg_opa(btn_delete, 160, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void ui_screen10_set_focus_style(uint8_t focus, uint8_t *focus_state,
                                 lv_obj_t *btn_replace, lv_obj_t *btn_delete, lv_obj_t *btn_esc)
{
    if(focus > 2u) focus = 0u;
    if(focus_state != NULL) *focus_state = focus;

    if(lv_obj_is_valid(btn_replace)) {
        lv_obj_set_style_bg_opa(btn_replace, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_replace, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if(lv_obj_is_valid(btn_delete)) {
        lv_obj_set_style_bg_opa(btn_delete, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if(lv_obj_is_valid(btn_esc)) {
        lv_obj_set_style_bg_opa(btn_esc, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_esc, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if(focus == 0u && lv_obj_is_valid(btn_replace)) {
        lv_obj_set_style_bg_opa(btn_replace, 160, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_replace, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if(focus == 1u && lv_obj_is_valid(btn_delete)) {
        lv_obj_set_style_bg_opa(btn_delete, 160, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if(focus == 2u && lv_obj_is_valid(btn_esc)) {
        lv_obj_set_style_bg_opa(btn_esc, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_esc, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void ui_common_hide_msgbox(lv_obj_t **popup, lv_obj_t **btn_yes, lv_obj_t **btn_no, uint8_t *msgbox_state)
{
    if(popup != NULL && *popup != NULL && lv_obj_is_valid(*popup)) {
        lv_obj_del(*popup);
    }
    if(popup != NULL) *popup = NULL;
    if(btn_yes != NULL) *btn_yes = NULL;
    if(btn_no != NULL) *btn_no = NULL;
    if(msgbox_state != NULL) *msgbox_state = 0u;
}

void ui_common_set_yes_no_choice(lv_obj_t *btn_yes, lv_obj_t *btn_no, uint8_t yes_selected, uint8_t *choice_yes)
{
    ui_popup_set_yes_no_choice(btn_yes, btn_no, yes_selected, choice_yes);
}

void ui_common_show_yes_no_popup(lv_obj_t *parent, lv_obj_t **popup, lv_obj_t **btn_yes, lv_obj_t **btn_no,
                                 const char *text_utf8, uint8_t *msgbox_state, uint8_t confirm_state)
{
    lv_obj_t *panel;
    lv_obj_t *lbl;
    lv_obj_t *btn_lbl;
    if(!lv_obj_is_valid(parent) || popup == NULL || btn_yes == NULL || btn_no == NULL) return;

    *popup = lv_obj_create(parent);
    lv_obj_set_size(*popup, 180, 110);
    lv_obj_align(*popup, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(*popup, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(*popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(*popup, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(*popup, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(*popup, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(*popup, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(*popup, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    panel = *popup;
    lbl = lv_label_create(panel);
    lv_label_set_text(lbl, text_utf8);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 170);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(lbl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(lbl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x4e4e4e), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 18);

    *btn_yes = lv_btn_create(panel);
    lv_obj_set_size(*btn_yes, 54, 28);
    lv_obj_align(*btn_yes, LV_ALIGN_BOTTOM_LEFT, 22, -12);
    btn_lbl = lv_label_create(*btn_yes);
    lv_label_set_text(btn_lbl, "YES");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(btn_lbl);

    *btn_no = lv_btn_create(panel);
    lv_obj_set_size(*btn_no, 54, 28);
    lv_obj_align(*btn_no, LV_ALIGN_BOTTOM_RIGHT, -22, -12);
    btn_lbl = lv_label_create(*btn_no);
    lv_label_set_text(btn_lbl, "NO");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(btn_lbl);

    if(msgbox_state != NULL) *msgbox_state = confirm_state;
}

void ui_common_show_result_popup(lv_obj_t *parent, lv_obj_t **popup, const char *text_utf8,
                                 uint8_t *msgbox_state, uint8_t state_value)
{
    lv_obj_t *panel;
    lv_obj_t *lbl;
    if(!lv_obj_is_valid(parent) || popup == NULL) return;
    *popup = lv_obj_create(parent);
    lv_obj_set_size(*popup, 180, 88);
    lv_obj_align(*popup, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(*popup, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(*popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(*popup, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(*popup, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(*popup, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(*popup, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(*popup, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    panel = *popup;
    lbl = lv_label_create(panel);
    lv_label_set_text(lbl, text_utf8);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 170);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x4e4e4e), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 26);
    if(msgbox_state != NULL) *msgbox_state = state_value;
}
