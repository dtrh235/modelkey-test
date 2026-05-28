#ifndef APP_SCREEN6_DIALOG_BASE_H
#define APP_SCREEN6_DIALOG_BASE_H

#include <stdint.h>
#include "lvgl.h"

void screen6_close_dialog(void);
void screen6_dlg_update_cursor_pos(void);
void screen6_dlg_setup_cursor_on_ta(lv_obj_t *ta);
void screen6_dlg_style_ta_like_screen5(lv_obj_t *ta, uint16_t max_len);

#endif
