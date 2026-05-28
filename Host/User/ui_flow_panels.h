#ifndef UI_FLOW_PANELS_H
#define UI_FLOW_PANELS_H

#include <stdint.h>
#include "lvgl.h"

void ui_screen6_set_focus_style(uint8_t focus, uint8_t *focus_state,
                                lv_obj_t *btn_acc, lv_obj_t *btn_pwd, lv_obj_t *btn_fp, lv_obj_t *btn_nfc,
                                lv_obj_t *dd);
void ui_screen9_set_focus_style(uint8_t focus, uint8_t *focus_state, lv_obj_t *btn_replace, lv_obj_t *btn_delete);
void ui_screen10_set_focus_style(uint8_t focus, uint8_t *focus_state,
                                 lv_obj_t *btn_replace, lv_obj_t *btn_delete, lv_obj_t *btn_esc);
void ui_common_hide_msgbox(lv_obj_t **popup, lv_obj_t **btn_yes, lv_obj_t **btn_no, uint8_t *msgbox_state);
void ui_common_set_yes_no_choice(lv_obj_t *btn_yes, lv_obj_t *btn_no, uint8_t yes_selected, uint8_t *choice_yes);
void ui_common_show_yes_no_popup(lv_obj_t *parent, lv_obj_t **popup, lv_obj_t **btn_yes, lv_obj_t **btn_no,
                                 const char *text_utf8, uint8_t *msgbox_state, uint8_t confirm_state);
void ui_common_show_result_popup(lv_obj_t *parent, lv_obj_t **popup, const char *text_utf8,
                                 uint8_t *msgbox_state, uint8_t state_value);

#endif
