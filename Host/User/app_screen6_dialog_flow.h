#ifndef APP_SCREEN6_DIALOG_FLOW_H
#define APP_SCREEN6_DIALOG_FLOW_H

#include "./BSP/KEY/key.h"

void screen6_open_edit_account_dialog(void);
void screen6_open_edit_password_dialog(void);
void screen6_dlg_handle_key(KeyValue_t key);
void screen6_dlg_cursor_blink_handle(void);

#endif
