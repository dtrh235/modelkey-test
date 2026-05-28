#include "ui_common_utils.h"

#include <string.h>

int ui_key_to_digit(KeyValue_t key)
{
    switch(key) {
    case KEY_0: return 0;
    case KEY_1: return 1;
    case KEY_2: return 2;
    case KEY_3: return 3;
    case KEY_4: return 4;
    case KEY_5: return 5;
    case KEY_6: return 6;
    case KEY_7: return 7;
    case KEY_8: return 8;
    case KEY_9: return 9;
    default: return -1;
    }
}

void ui_lock_btn_set_selected(lv_obj_t *lock_btn, lv_obj_t *menu_btn,
                              uint8_t *lock_selected, uint8_t *menu_selected,
                              uint8_t selected)
{
    if(lock_selected == NULL || menu_selected == NULL) return;
    if(!lv_obj_is_valid(lock_btn)) return;

    *lock_selected = selected ? 1u : 0u;
    if(*lock_selected) {
        *menu_selected = 0u;
        if(lv_obj_is_valid(menu_btn)) {
            lv_obj_set_style_bg_opa(menu_btn, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        lv_obj_set_style_bg_opa(lock_btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(lock_btn, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_bg_opa(lock_btn, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(lock_btn, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void ui_menu_btn_set_selected(lv_obj_t *lock_btn, lv_obj_t *menu_btn,
                              uint8_t *lock_selected, uint8_t *menu_selected,
                              uint8_t selected)
{
    if(lock_selected == NULL || menu_selected == NULL) return;
    if(!lv_obj_is_valid(menu_btn)) return;

    *menu_selected = selected ? 1u : 0u;
    if(*menu_selected) {
        *lock_selected = 0u;
        if(lv_obj_is_valid(lock_btn)) {
            lv_obj_set_style_bg_opa(lock_btn, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(lock_btn, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        lv_obj_set_style_bg_opa(menu_btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x006bb3), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_bg_opa(menu_btn, 165, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x009bff), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void ui_textarea_measure_text_run_width(lv_obj_t *ta, lv_coord_t *txt_w, uint32_t mask_buf_len)
{
    const char *txt;
    uint32_t len;
    const lv_font_t *font;
    lv_coord_t letter_space;
    char mask[32];
    uint32_t i;

    if(!lv_obj_is_valid(ta) || txt_w == NULL) return;
    txt = lv_textarea_get_text(ta);
    len = (uint32_t)strlen(txt);
    font = lv_obj_get_style_text_font(ta, LV_PART_MAIN | LV_STATE_DEFAULT);
    letter_space = lv_obj_get_style_text_letter_space(ta, LV_PART_MAIN | LV_STATE_DEFAULT);

    if(mask_buf_len < 2u || mask_buf_len > (uint32_t)sizeof(mask)) {
        mask_buf_len = (uint32_t)sizeof(mask);
    }

    if(lv_textarea_get_password_mode(ta)) {
        if(len >= mask_buf_len) len = mask_buf_len - 1u;
        for(i = 0u; i < len; i++) mask[i] = '*';
        mask[len] = '\0';
        *txt_w = lv_txt_get_width(mask, len, font, letter_space, LV_TEXT_FLAG_NONE);
    } else {
        *txt_w = lv_txt_get_width(txt, len, font, letter_space, LV_TEXT_FLAG_NONE);
    }
}

void ui_label_set_hidden(lv_obj_t *label, uint8_t hidden)
{
    if(!lv_obj_is_valid(label)) return;
    if(hidden) lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
}

void ui_cursor_blink_step(lv_obj_t *cursor, uint8_t *visible, uint32_t *last_ms, uint32_t period_ms)
{
    if(cursor == NULL || visible == NULL || last_ms == NULL) return;
    if(!lv_obj_is_valid(cursor)) return;
    if(lv_tick_elaps(*last_ms) < period_ms) return;
    *last_ms = lv_tick_get();
    *visible = (*visible) ? 0u : 1u;
    if(*visible) lv_obj_clear_flag(cursor, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(cursor, LV_OBJ_FLAG_HIDDEN);
}

void ui_textarea_apply_input_style(lv_obj_t *ta, uint8_t selected)
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

void ui_cursor_attach_bar(lv_obj_t **cursor, lv_obj_t *parent)
{
    if(cursor == NULL || !lv_obj_is_valid(parent)) return;
    if(*cursor == NULL || !lv_obj_is_valid(*cursor)) {
        *cursor = lv_label_create(parent);
        lv_label_set_text(*cursor, "|");
        lv_obj_set_style_text_color(*cursor, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_parent(*cursor, parent);
    }
}

void ui_cursor_activate(lv_obj_t *cursor, uint8_t *visible, uint32_t *last_ms)
{
    if(!lv_obj_is_valid(cursor) || visible == NULL || last_ms == NULL) return;
    *visible = 1u;
    *last_ms = lv_tick_get();
    lv_obj_clear_flag(cursor, LV_OBJ_FLAG_HIDDEN);
}
