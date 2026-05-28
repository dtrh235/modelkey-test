#ifndef APP_USER_OPS_H
#define APP_USER_OPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_config.h"
#include "user_auth_portable.h"

/* 前 32 个用户槽位仅写本机 W25Q16；第 33 人起需 RS485 从机在线并同步镜像到从机（主机 Flash 仍保存全表）。 */
#define APP_USER_MAX 64u
#define APP_USER_HOST_PRIMARY_SLOTS 32u

int users_find_index_by_acc(const char *acc);
int users_find_index_by_nfc_uid(const uint8_t uid[4]);
bool users_bind_nfc_by_acc(const char *acc, const uint8_t uid[4]);
bool users_fp_page_owned_by_other(const char *acc, uint16_t page_id);
bool users_bind_fp_by_acc(const char *acc, uint16_t page_id);
uint8_t users_has_fp_by_acc(const char *acc, uint16_t *page_id_out);
uint8_t users_match_fp_page(uint16_t page_id);
const char *users_get_acc_by_fp_page(uint16_t page_id);
void users_clear_fp_by_acc(const char *acc, uint8_t clear_hw);
void users_clear_nfc_by_acc(const char *acc);
bool admin_credentials_match(const char *acc, const char *pwd);
bool unlock_credentials_match(const char *acc, const char *pwd);
bool users_try_register(const char *acc, const char *pwd, bool is_admin);
bool users_try_delete_by_acc(const char *acc);
bool default_admin_deleted(void);
bool admin_credentials_match_with_delete(const char *acc, const char *pwd);
bool unlock_credentials_match_with_delete(const char *acc, const char *pwd);
bool users_storage_load(void);
bool users_storage_save(void);
size_t users_storage_blob_size(void);
bool users_storage_export_blob(uint8_t *out, size_t out_sz, size_t *out_len, uint32_t *out_hash);
bool users_storage_import_blob(const uint8_t *blob, size_t blob_len, uint32_t *out_hash);
uint32_t users_storage_snapshot_hash(void);
void users_storage_task_register_current(void);
void users_storage_task_wait_and_flush(uint32_t wait_ms);
void users_storage_mark_boot_save_pending(void);
void users_storage_boot_commit_pending(void);
void users_storage_request_save(void);

uint8_t fp_delete_template_by_page(uint16_t page_id);

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
bool users_slave_mirror_apply_user(const user_cred_t *u);
bool users_slave_mirror_delete_by_acc(const char *acc);
void users_slave_mirror_save_tick(void);
#endif

#endif
