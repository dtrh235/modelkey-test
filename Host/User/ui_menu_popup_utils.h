#ifndef UI_MENU_POPUP_UTILS_H
#define UI_MENU_POPUP_UTILS_H

#include <stdint.h>
#include "lvgl.h"

#define SCREEN3_MENU_ITEM_COUNT 6u

lv_obj_t *ui_menu_btn_by_index6(lv_obj_t *btn0, lv_obj_t *btn1, lv_obj_t *btn2, lv_obj_t *btn3, lv_obj_t *btn4,
                                lv_obj_t *btn5, uint8_t idx);
lv_obj_t *ui_menu_label_by_index6(lv_obj_t *lbl0, lv_obj_t *lbl1, lv_obj_t *lbl2, lv_obj_t *lbl3, lv_obj_t *lbl4,
                                  lv_obj_t *lbl5, uint8_t idx);
void ui_screen3_set_menu_selected(uint8_t idx, uint8_t *menu_index,
                                  lv_obj_t *btn0, lv_obj_t *btn1, lv_obj_t *btn2, lv_obj_t *btn3, lv_obj_t *btn4,
                                  lv_obj_t *btn5,
                                  lv_obj_t *lbl0, lv_obj_t *lbl1, lv_obj_t *lbl2, lv_obj_t *lbl3, lv_obj_t *lbl4,
                                  lv_obj_t *lbl5);
void ui_popup_btn_style(lv_obj_t *btn, lv_obj_t *lbl, uint8_t selected);
void ui_popup_set_yes_no_choice(lv_obj_t *btn_yes, lv_obj_t *btn_no, uint8_t yes_selected, uint8_t *choice_yes);

#endif
