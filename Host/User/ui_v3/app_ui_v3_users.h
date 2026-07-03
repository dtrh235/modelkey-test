#ifndef APP_UI_V3_USERS_H
#define APP_UI_V3_USERS_H

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

#if (APP_UI_V3_ENABLE == 1)

#include "lvgl.h"
#include "app_ui_v3_types.h"

#define UI3_UNLOCK_FAIL_LIMIT      5u
#define UI3_UNLOCK_LOCKOUT_MS      60000u

typedef struct {
    char acc[13];
    uint8_t role_admin;
    uint8_t has_fp;
    uint8_t has_nfc;
} ui3_user_row_t;

typedef enum {
    UI3_DEL_OK = 0,
    UI3_DEL_FAIL_NOT_FOUND,
    UI3_DEL_FAIL_LAST_ADMIN,
    UI3_DEL_FAIL_OTHER
} ui3_del_result_t;

uint8_t ui3_users_list_count(void);
bool ui3_users_list_row(uint8_t idx, ui3_user_row_t *out);
bool ui3_users_lookup(const char *acc, ui3_user_row_t *out);
bool ui3_users_get_password(const char *acc, char *out, size_t out_sz);

void ui3_users_reset_add_pending(void);
void ui3_users_prepare_add_screen(ui3_state_t *st);
void ui3_users_set_edit_target(const char *acc);

bool ui3_users_try_register(const char *acc, const char *pwd, bool is_admin);
bool ui3_users_try_delete(const char *acc);
ui3_del_result_t ui3_users_try_delete_msg(const char *acc, const char **out_msg);
bool ui3_users_set_role(const char *acc, bool is_admin);
bool ui3_users_change_password(const char *acc, const char *pwd);

/* 0=失败 1=成功 2=锁定 */
uint8_t ui3_auth_try_unlock(ui3_state_t *st, const char *acc, const char *pwd);
bool ui3_auth_try_admin(ui3_state_t *st, const char *acc, const char *pwd);
void ui3_auth_unlock_success(const char *acc, const char *method);

uint8_t ui3_users_enroll_active(void);
void ui3_users_fp_start_add(lv_obj_t *parent);
void ui3_users_nfc_start_add(lv_obj_t *parent);
void ui3_users_fp_start_replace(lv_obj_t *parent, const char *acc);
void ui3_users_nfc_start_replace(lv_obj_t *parent, const char *acc);
bool ui3_users_clear_fp(const char *acc);
bool ui3_users_clear_nfc(const char *acc);
void ui3_users_enroll_poll(void);
void ui3_users_on_screen_changing(void);

#else

static inline uint8_t ui3_users_enroll_active(void) { return 0u; }
static inline void ui3_users_enroll_poll(void) {}

#endif

#endif
