#include "user_auth_portable.h"

#include <string.h>

int user_auth_find_index_by_acc(const user_cred_t *users, uint8_t user_count, const char *acc)
{
    uint8_t i;
    if(users == NULL || acc == NULL) return -1;
    for(i = 0u; i < user_count; i++) {
        if(users[i].active && strcmp(acc, users[i].acc) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int user_auth_find_index_by_nfc_uid(const user_cred_t *users, uint8_t user_count, const uint8_t uid[4])
{
    uint8_t i;
    if(users == NULL || uid == NULL) return -1;
    for(i = 0u; i < user_count; i++) {
        if(!users[i].active || !users[i].has_nfc) continue;
        if(memcmp(users[i].nfc_uid, uid, 4) == 0) return (int)i;
    }
    return -1;
}

uint8_t user_auth_match_fp_page(const user_cred_t *users, uint8_t user_count,
                                uint16_t page_id,
                                uint8_t default_admin_deleted,
                                uint8_t default_admin_has_fp,
                                uint16_t default_admin_fp_page_id_1,
                                uint16_t default_admin_fp_page_id_2)
{
    uint8_t i;
    if(!default_admin_deleted && default_admin_has_fp &&
       (default_admin_fp_page_id_1 == page_id || default_admin_fp_page_id_2 == page_id)) {
        return 1u;
    }
    if(users == NULL) return 0u;
    for(i = 0u; i < user_count; i++) {
        if(!users[i].active || !users[i].has_fp) continue;
        if(users[i].fp_page_id_1 == page_id || users[i].fp_page_id_2 == page_id) return 1u;
    }
    return 0u;
}

const char *user_auth_get_acc_by_fp_page(const user_cred_t *users, uint8_t user_count,
                                         uint16_t page_id,
                                         uint8_t default_admin_deleted,
                                         uint8_t default_admin_has_fp,
                                         uint16_t default_admin_fp_page_id_1,
                                         uint16_t default_admin_fp_page_id_2,
                                         const char *default_admin_account)
{
    uint8_t i;
    if(default_admin_account == NULL) default_admin_account = "";
    if(!default_admin_deleted && default_admin_has_fp &&
       (default_admin_fp_page_id_1 == page_id || default_admin_fp_page_id_2 == page_id)) {
        return default_admin_account;
    }
    if(users == NULL) return "";
    for(i = 0u; i < user_count; i++) {
        if(!users[i].active || !users[i].has_fp) continue;
        if(users[i].fp_page_id_1 == page_id || users[i].fp_page_id_2 == page_id) return users[i].acc;
    }
    return "";
}

bool user_auth_admin_credentials_match(const user_cred_t *users, uint8_t user_count,
                                       const char *default_admin_account,
                                       const char *default_admin_password,
                                       uint8_t default_admin_is_admin_role,
                                       const char *acc, const char *pwd)
{
    int idx;
    if(default_admin_account == NULL || default_admin_password == NULL || acc == NULL || pwd == NULL) return false;
    if(strcmp(acc, default_admin_account) == 0 && strcmp(pwd, default_admin_password) == 0) {
        return default_admin_is_admin_role ? true : false;
    }
    idx = user_auth_find_index_by_acc(users, user_count, acc);
    if(idx < 0) return false;
    if(!users[idx].is_admin) return false;
    return strcmp(pwd, users[idx].pwd) == 0;
}

bool user_auth_unlock_credentials_match(const user_cred_t *users, uint8_t user_count,
                                        const char *default_admin_account,
                                        const char *default_admin_password,
                                        const char *acc, const char *pwd)
{
    int idx;
    if(default_admin_account == NULL || default_admin_password == NULL || acc == NULL || pwd == NULL) return false;
    if(strcmp(acc, default_admin_account) == 0 && strcmp(pwd, default_admin_password) == 0) return true;
    idx = user_auth_find_index_by_acc(users, user_count, acc);
    if(idx < 0) return false;
    return strcmp(pwd, users[idx].pwd) == 0;
}

static bool fp_page_owned_by_other(const user_cred_t *users, uint8_t user_count,
                                   const user_auth_admin_t *admin,
                                   const char *acc, uint16_t page_id)
{
    uint8_t i;
    if(users == NULL || admin == NULL || acc == NULL) return false;
    if(!admin->deleted && admin->has_fp &&
       strcmp(acc, admin->account) != 0 &&
       (admin->fp_page_id_1 == page_id || admin->fp_page_id_2 == page_id)) {
        return true;
    }
    for(i = 0u; i < user_count; i++) {
        if(!users[i].active || !users[i].has_fp) continue;
        if(strcmp(users[i].acc, acc) == 0) continue;
        if(users[i].fp_page_id_1 == page_id || users[i].fp_page_id_2 == page_id) return true;
    }
    return false;
}

bool user_auth_bind_nfc_by_acc(user_cred_t *users, uint8_t user_count,
                               user_auth_admin_t *admin,
                               const char *acc, const uint8_t uid[4])
{
    int idx;
    int owner;
    if(users == NULL || admin == NULL || acc == NULL || uid == NULL) return false;
    if(strcmp(acc, admin->account) == 0 && !admin->deleted) {
        owner = user_auth_find_index_by_nfc_uid(users, user_count, uid);
        if(owner >= 0) return false;
        admin->has_nfc = 1u;
        memcpy(admin->nfc_uid, uid, 4);
        return true;
    }
    idx = user_auth_find_index_by_acc(users, user_count, acc);
    if(idx < 0) return false;
    owner = user_auth_find_index_by_nfc_uid(users, user_count, uid);
    if(owner >= 0 && owner != idx) return false;
    if(!admin->deleted && admin->has_nfc && memcmp(admin->nfc_uid, uid, 4) == 0) return false;
    memcpy(users[idx].nfc_uid, uid, 4);
    users[idx].has_nfc = 1u;
    return true;
}

bool user_auth_bind_fp_by_acc(user_cred_t *users, uint8_t user_count,
                              user_auth_admin_t *admin,
                              const char *acc, uint16_t page_id)
{
    int idx;
    if(users == NULL || admin == NULL || acc == NULL) return false;
    if(fp_page_owned_by_other(users, user_count, admin, acc, page_id)) return false;
    if(!admin->deleted && strcmp(acc, admin->account) == 0) {
        admin->has_fp = 1u;
        if(admin->fp_page_id_1 == page_id || admin->fp_page_id_2 == page_id) return true;
        if(admin->fp_page_id_1 == 0xFFFFu) {
            admin->fp_page_id_1 = page_id;
            return true;
        }
        if(admin->fp_page_id_2 == 0xFFFFu) {
            admin->fp_page_id_2 = page_id;
            return true;
        }
        admin->fp_page_id_2 = page_id;
        return true;
    }
    idx = user_auth_find_index_by_acc(users, user_count, acc);
    if(idx < 0) return false;
    users[idx].has_fp = 1u;
    if(users[idx].fp_page_id_1 == page_id || users[idx].fp_page_id_2 == page_id) return true;
    if(users[idx].fp_page_id_1 == 0xFFFFu) {
        users[idx].fp_page_id_1 = page_id;
        return true;
    }
    if(users[idx].fp_page_id_2 == 0xFFFFu) {
        users[idx].fp_page_id_2 = page_id;
        return true;
    }
    users[idx].fp_page_id_2 = page_id;
    return true;
}

uint8_t user_auth_has_fp_by_acc(const user_cred_t *users, uint8_t user_count,
                                const user_auth_admin_t *admin,
                                const char *acc, uint16_t *page_id_out)
{
    int idx;
    if(users == NULL || admin == NULL || acc == NULL) return 0u;
    if(!admin->deleted && strcmp(acc, admin->account) == 0) {
        if(!admin->has_fp) return 0u;
        if(page_id_out != NULL) *page_id_out = admin->fp_page_id_1;
        return 1u;
    }
    idx = user_auth_find_index_by_acc(users, user_count, acc);
    if(idx < 0 || !users[idx].has_fp) return 0u;
    if(page_id_out != NULL) *page_id_out = users[idx].fp_page_id_1;
    return 1u;
}

void user_auth_clear_fp_by_acc(user_cred_t *users, uint8_t user_count,
                               user_auth_admin_t *admin,
                               const char *acc, uint8_t clear_hw,
                               uint8_t (*fp_delete_cb)(uint16_t page_id))
{
    int idx;
    uint16_t page1 = 0xFFFFu;
    uint16_t page2 = 0xFFFFu;
    uint8_t has_fp = 0u;
    if(users == NULL || admin == NULL || acc == NULL) return;
    if(!admin->deleted && strcmp(acc, admin->account) == 0) {
        if(admin->has_fp) {
            has_fp = 1u;
            page1 = admin->fp_page_id_1;
            page2 = admin->fp_page_id_2;
        }
        admin->has_fp = 0u;
        admin->fp_page_id_1 = 0xFFFFu;
        admin->fp_page_id_2 = 0xFFFFu;
        if(clear_hw && has_fp && fp_delete_cb != NULL) {
            if(page1 != 0xFFFFu) (void)fp_delete_cb(page1);
            if(page2 != 0xFFFFu && page2 != page1) (void)fp_delete_cb(page2);
        }
        return;
    }
    idx = user_auth_find_index_by_acc(users, user_count, acc);
    if(idx < 0) return;
    if(users[idx].has_fp) {
        has_fp = 1u;
        page1 = users[idx].fp_page_id_1;
        page2 = users[idx].fp_page_id_2;
    }
    users[idx].has_fp = 0u;
    users[idx].fp_page_id_1 = 0xFFFFu;
    users[idx].fp_page_id_2 = 0xFFFFu;
    if(clear_hw && has_fp && fp_delete_cb != NULL) {
        if(page1 != 0xFFFFu) (void)fp_delete_cb(page1);
        if(page2 != 0xFFFFu && page2 != page1) (void)fp_delete_cb(page2);
    }
}

void user_auth_clear_nfc_by_acc(user_cred_t *users, uint8_t user_count,
                                user_auth_admin_t *admin,
                                const char *acc)
{
    int idx;
    if(users == NULL || admin == NULL || acc == NULL) return;
    if(!admin->deleted && strcmp(acc, admin->account) == 0) {
        admin->has_nfc = 0u;
        memset(admin->nfc_uid, 0, sizeof(admin->nfc_uid));
        return;
    }
    idx = user_auth_find_index_by_acc(users, user_count, acc);
    if(idx < 0) return;
    users[idx].has_nfc = 0u;
    memset(users[idx].nfc_uid, 0, sizeof(users[idx].nfc_uid));
}
