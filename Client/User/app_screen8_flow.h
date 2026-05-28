#ifndef APP_SCREEN8_FLOW_H
#define APP_SCREEN8_FLOW_H

#include <stdbool.h>

bool screen8_has_valid_acc_pwd_input(void);
void screen8_attempt_commit(void);
void screen8_success_popup_poll(void);
void enter_screen_8(void);

#endif
