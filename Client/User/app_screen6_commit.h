#ifndef APP_SCREEN6_COMMIT_H
#define APP_SCREEN6_COMMIT_H

#include <stdbool.h>

void screen6_save_identity(bool is_admin);
bool screen6_try_commit_password_change(const char *new_pwd);

#endif
