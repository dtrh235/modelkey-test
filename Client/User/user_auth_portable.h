#ifndef USER_AUTH_PORTABLE_H
#define USER_AUTH_PORTABLE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char acc[13];
    char pwd[11];
    uint8_t is_admin;
    uint8_t active;
    uint8_t has_nfc;
    uint8_t nfc_uid[4];
    uint8_t has_fp;
    uint16_t fp_page_id_1;
    uint16_t fp_page_id_2;
} user_cred_t;

typedef struct {
    uint8_t deleted;
    uint8_t is_admin_role;
    char account[13];
    char password[11];
    uint8_t has_nfc;
    uint8_t nfc_uid[4];
    uint8_t has_fp;
    uint16_t fp_page_id_1;
    uint16_t fp_page_id_2;
} user_auth_admin_t;

int user_auth_find_index_by_acc(const user_cred_t *users, uint8_t user_count, const char *acc);
int user_auth_find_index_by_nfc_uid(const user_cred_t *users, uint8_t user_count, const uint8_t uid[4]);
uint8_t user_auth_match_fp_page(const user_cred_t *users, uint8_t user_count,
                                uint16_t page_id,
                                uint8_t default_admin_deleted,
                                uint8_t default_admin_has_fp,
                                uint16_t default_admin_fp_page_id_1,
                                uint16_t default_admin_fp_page_id_2);
const char *user_auth_get_acc_by_fp_page(const user_cred_t *users, uint8_t user_count,
                                         uint16_t page_id,
                                         uint8_t default_admin_deleted,
                                         uint8_t default_admin_has_fp,
                                         uint16_t default_admin_fp_page_id_1,
                                         uint16_t default_admin_fp_page_id_2,
                                         const char *default_admin_account);
bool user_auth_admin_credentials_match(const user_cred_t *users, uint8_t user_count,
                                       const char *default_admin_account,
                                       const char *default_admin_password,
                                       uint8_t default_admin_is_admin_role,
                                       const char *acc, const char *pwd);
bool user_auth_unlock_credentials_match(const user_cred_t *users, uint8_t user_count,
                                        const char *default_admin_account,
                                        const char *default_admin_password,
                                        const char *acc, const char *pwd);
bool user_auth_bind_nfc_by_acc(user_cred_t *users, uint8_t user_count,
                               user_auth_admin_t *admin,
                               const char *acc, const uint8_t uid[4]);
bool user_auth_bind_fp_by_acc(user_cred_t *users, uint8_t user_count,
                              user_auth_admin_t *admin,
                              const char *acc, uint16_t page_id);
uint8_t user_auth_has_fp_by_acc(const user_cred_t *users, uint8_t user_count,
                                const user_auth_admin_t *admin,
                                const char *acc, uint16_t *page_id_out);
void user_auth_clear_fp_by_acc(user_cred_t *users, uint8_t user_count,
                               user_auth_admin_t *admin,
                               const char *acc, uint8_t clear_hw,
                               uint8_t (*fp_delete_cb)(uint16_t page_id));
void user_auth_clear_nfc_by_acc(user_cred_t *users, uint8_t user_count,
                                user_auth_admin_t *admin,
                                const char *acc);

#endif
