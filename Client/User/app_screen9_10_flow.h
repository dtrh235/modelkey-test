#ifndef APP_SCREEN9_10_FLOW_H
#define APP_SCREEN9_10_FLOW_H

#include <stdint.h>
#include "lvgl.h"

void screen9_set_focus(uint8_t focus);
void screen9_hide_all_msgbox(void);
void screen9_popup_btn_style(lv_obj_t *btn, lv_obj_t *lbl, uint8_t selected);
void screen9_set_msgbox_choice(uint8_t yes_selected);
void screen9_show_delete_confirm_popup(void);
void screen10_show_delete_confirm_popup(void);
void screen9_show_delete_result_popup(uint8_t success);
void screen10_show_delete_result_popup(uint8_t success);
uint8_t screen10_try_delete_fp(void);
void screen9_start_nfc_replace(void);
void enter_screen_9(void);
void screen10_set_focus(uint8_t focus);
void enter_screen_10(void);

#endif
