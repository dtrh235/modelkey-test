#include "app_screen6_commit.h"

#include <string.h>

#include "app_config.h"
#include "cloud_ota_service.h"
#include "app_user_ops.h"
#include "user_auth_portable.h"

extern char g_screen5_found_acc[13];
extern uint8_t g_screen5_found_is_admin;
extern user_cred_t g_users[APP_USER_MAX];
extern uint8_t g_default_admin_deleted;
extern uint8_t g_default_admin_is_admin_role;
extern char g_default_admin_account[13];
extern char g_default_admin_password[11];

void screen6_save_identity(bool is_admin)
{
    int idx;
    const char *acc = g_screen5_found_acc;
    uint8_t now_admin = 0u;

    if(acc == NULL || acc[0] == '\0') return;

    if(!g_default_admin_deleted && strcmp(acc, g_default_admin_account) == 0) {
        now_admin = g_default_admin_is_admin_role;
        if(now_admin != 0u && !is_admin && users_admin_count() <= 1u) {
            return;
        }
        g_default_admin_is_admin_role = is_admin ? 1u : 0u;
    } else {
        idx = users_find_index_by_acc(acc);
        if(idx >= 0) {
            now_admin = g_users[idx].is_admin;
            if(now_admin != 0u && !is_admin && users_admin_count() <= 1u) {
                return;
            }
            g_users[idx].is_admin = is_admin ? 1u : 0u;
        }
    }

    g_screen5_found_is_admin = is_admin ? 1u : 0u;
    (void)users_storage_save();
    cloud_ota_service_report_event(CLOUD_EVT_USER_ROLE_CHANGE, acc);
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
    users_mirror_schedule_acc(acc);
#endif
}

bool screen6_try_commit_password_change(const char *new_pwd)
{
    size_t len;
    const char *acc;
    int idx;

    if(new_pwd == NULL) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_FAIL, g_screen5_found_acc);
        return false;
    }
    len = strlen(new_pwd);
    if(len < 4u || len > 10u) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_FAIL, g_screen5_found_acc);
        return false;
    }

    acc = g_screen5_found_acc;
    if(acc[0] == '\0') {
        cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_FAIL, g_screen5_found_acc);
        return false;
    }

    if(!g_default_admin_deleted && strcmp(acc, g_default_admin_account) == 0) {
        strncpy(g_default_admin_password, new_pwd, sizeof(g_default_admin_password) - 1u);
        g_default_admin_password[sizeof(g_default_admin_password) - 1u] = '\0';
        (void)users_storage_save();
        cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_OK, acc);
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
        users_mirror_schedule_acc(acc);
#endif
        return true;
    }

    idx = users_find_index_by_acc(acc);
    if(idx < 0) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_FAIL, acc);
        return false;
    }
    strncpy(g_users[idx].pwd, new_pwd, sizeof(g_users[idx].pwd) - 1u);
    g_users[idx].pwd[sizeof(g_users[idx].pwd) - 1u] = '\0';
    (void)users_storage_save();
    cloud_ota_service_report_event(CLOUD_EVT_USER_PWD_CHANGE_OK, acc);
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
    users_mirror_schedule_acc(acc);
#endif
    return true;
}
