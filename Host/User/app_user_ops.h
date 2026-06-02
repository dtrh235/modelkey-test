#ifndef APP_USER_OPS_H
#define APP_USER_OPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 前 32 个用户槽位仅写本机 W25Q16；第 33 人起需 RS485 从机在线并同步镜像到从机（主机 Flash 仍保存全表）。 */
#define APP_USER_MAX 64u
#define APP_USER_HOST_PRIMARY_SLOTS 32u

int users_find_index_by_acc(const char *acc);
int users_find_index_by_nfc_uid(const uint8_t uid[4]);
bool users_bind_nfc_by_acc(const char *acc, const uint8_t uid[4]);
bool users_fp_page_owned_by_other(const char *acc, uint16_t page_id);
uint8_t users_fp_page_in_use(uint16_t page_id);
uint8_t users_fp_alloc_enroll_pages(uint16_t *page1_out, uint16_t *page2_out);
bool users_bind_fp_by_acc(const char *acc, uint16_t page_id);
bool users_bind_fp_by_acc_ex(const char *acc, uint16_t page_id, uint8_t schedule_mirror);
uint8_t users_has_fp_by_acc(const char *acc, uint16_t *page_id_out);
uint8_t users_match_fp_page(uint16_t page_id);
const char *users_get_acc_by_fp_page(uint16_t page_id);
void users_clear_fp_by_acc(const char *acc, uint8_t clear_hw);
void users_clear_nfc_by_acc(const char *acc);
bool admin_credentials_match(const char *acc, const char *pwd);
bool unlock_credentials_match(const char *acc, const char *pwd);
bool users_try_register(const char *acc, const char *pwd, bool is_admin);
bool users_try_delete_by_acc(const char *acc);
uint8_t users_admin_count(void);
uint8_t users_migrate_default_admin_to_users(void);
/* 1=存在至少一个可登录的管理员（Flash 用户表或旧版虚拟默认槽） */
uint8_t users_has_loginable_admin(void);
/* 仅当没有任何可登录管理员时，写入出厂账号 1/1111（不清空已有用户） */
void users_ensure_default_admin_if_none(void);
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
void users_storage_kick_flush(void);
uint8_t users_storage_try_commit_scr3_from_gui(void);
void users_storage_scr3_idle_service(void);

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
bool users_fp_template_cache_save(uint16_t page_id, const uint8_t *tpl, uint16_t tpl_len);
bool users_fp_template_cache_load(uint16_t page_id, uint8_t *tpl, uint16_t tpl_len);
bool users_fp_template_cache_valid(uint16_t page_id);
void users_fp_template_cache_delete(uint16_t page_id);
#endif

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
/* 从机上线或定期拉齐：把主机 RAM 中的用户（含 NFC）下发从机，避免从机单独重启后刷卡无匹配。 */
void users_mirror_to_slave_on_link(void);
/* 在 CloudTask 中执行，避免 NfcTask/GuiTask 里同步 RS485 全量镜像卡死 LVGL */
void users_mirror_run_pending(void);
void users_mirror_schedule_full(void);
void users_mirror_schedule_acc(const char *acc);
void users_mirror_schedule_delete(const char *acc);
#endif

uint8_t fp_delete_template_by_page(uint16_t page_id);

#endif
