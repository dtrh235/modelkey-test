#include "ui_auth_input.h"

#include <string.h>
#include "ui_common_utils.h"

lv_obj_t *ui_auth_get_field_by_index(lv_obj_t *first_field, lv_obj_t *second_field, uint8_t idx)
{
    return (idx == 0u) ? first_field : second_field;
}

uint16_t ui_auth_get_field_max_len(uint8_t idx)
{
    return (idx == 0u) ? 12u : 10u;
}

void ui_auth_update_cursor_pos(lv_obj_t *cursor, lv_obj_t *ta)
{
    lv_coord_t txt_w;
    lv_coord_t x_off;
    lv_coord_t max_x;

    if(cursor == NULL || !lv_obj_is_valid(cursor) || !lv_obj_is_valid(ta)) return;

    ui_textarea_measure_text_run_width(ta, &txt_w, 16u);
    x_off = 4 + txt_w;
    max_x = lv_obj_get_content_width(ta) - 4;
    if(x_off > max_x) x_off = max_x;
    if(x_off < 4) x_off = 4;
    lv_obj_align(cursor, LV_ALIGN_LEFT_MID, x_off, 0);
}

void ui_auth_handle_input_key(lv_obj_t *ta, KeyValue_t key, uint16_t max_len,
                              void (*hide_error_cb)(void),
                              void (*update_cursor_cb)(void))
{
    int digit;
    uint16_t cur_len;

    if(!lv_obj_is_valid(ta)) return;

    if(key == KEY_LEFT) {
        if(hide_error_cb != NULL) hide_error_cb();
        lv_textarea_del_char(ta);
        if(update_cursor_cb != NULL) update_cursor_cb();
        return;
    }

    digit = ui_key_to_digit(key);
    if(digit < 0) return;

    cur_len = (uint16_t)strlen(lv_textarea_get_text(ta));
    if(cur_len >= max_len) return;

    if(hide_error_cb != NULL) hide_error_cb();
    lv_textarea_add_char(ta, (uint32_t)('0' + digit));
    if(update_cursor_cb != NULL) update_cursor_cb();
}
