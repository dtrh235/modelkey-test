#include "app_user_add_flow.h"

#include <string.h>

#include "app_state.h"
#include "app_user_ops.h"

static char s_draft_acc[13];

bool user_add_validate_acc_pwd(const char *acc, const char *pwd)
{
    size_t la;
    size_t lp;

    if(acc == NULL || pwd == NULL) {
        return false;
    }
    la = strlen(acc);
    lp = strlen(pwd);
    return (la >= 1u && la <= 12u && lp >= 4u && lp <= 10u) ? true : false;
}

void user_add_clear_pending_bio(void)
{
    g_nfc_pending_uid_valid = 0u;
    memset(g_nfc_pending_uid, 0, sizeof(g_nfc_pending_uid));
    g_fp_pending_page_valid = 0u;
    g_fp_pending_page_id = 0xFFFFu;
    g_fp_pending_page2_valid = 0u;
    g_fp_pending_page_id_2 = 0xFFFFu;
}

bool user_add_commit(const char *acc, const char *pwd, bool is_admin)
{
    bool ok = true;

    if(!user_add_validate_acc_pwd(acc, pwd)) {
        return false;
    }
    if(!users_try_register(acc, pwd, is_admin)) {
        return false;
    }
    if(g_nfc_pending_uid_valid) {
        if(!users_bind_nfc_by_acc(acc, g_nfc_pending_uid)) {
            (void)users_try_delete_by_acc(acc);
            ok = false;
        }
    }
    if(ok && g_fp_pending_page_valid) {
        if(!users_bind_fp_by_acc(acc, g_fp_pending_page_id)) {
            (void)users_try_delete_by_acc(acc);
            ok = false;
        }
    }
    if(ok && g_fp_pending_page2_valid) {
        (void)users_bind_fp_by_acc(acc, g_fp_pending_page_id_2);
    }
    if(ok) {
        user_add_clear_pending_bio();
        s_draft_acc[0] = '\0';
    }
    return ok;
}

void user_add_set_draft_acc(const char *acc)
{
    if(acc == NULL || acc[0] == '\0') {
        s_draft_acc[0] = '\0';
        return;
    }
    (void)strncpy(s_draft_acc, acc, sizeof(s_draft_acc) - 1u);
    s_draft_acc[sizeof(s_draft_acc) - 1u] = '\0';
}

const char *user_add_draft_acc(void)
{
    return s_draft_acc;
}
