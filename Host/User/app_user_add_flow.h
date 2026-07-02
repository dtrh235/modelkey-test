#ifndef APP_USER_ADD_FLOW_H
#define APP_USER_ADD_FLOW_H

#include <stdbool.h>
#include <stddef.h>

/* 增加用户：与 Guider screen8 共用的纯业务逻辑（不依赖 LVGL 控件）。 */

bool user_add_validate_acc_pwd(const char *acc, const char *pwd);

/* 注册账号并绑定 pending 的 NFC/指纹；成功时清除 pending。 */
bool user_add_commit(const char *acc, const char *pwd, bool is_admin);

void user_add_clear_pending_bio(void);

/* ui_v3 录入时记录草稿账号，供底层日志/调试（非 Flash 提交）。 */
void user_add_set_draft_acc(const char *acc);
const char *user_add_draft_acc(void);

#endif
