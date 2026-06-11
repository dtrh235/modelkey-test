/**
 * @file test_unlock_diag.c
 * @brief 从 W25Q 读取用户表，复现 Host unlock/admin 校验，判断 1/1111 失败是数据还是 UI。
 */

#include "test_unlock_diag.h"
#include "test_rs485_log.h"
#include "app_config.h"
#include "user_auth_portable.h"

#include "../../../Host/Drivers/BSP/W25Q16/bsp_w25q16.h"
#include <string.h>

#define USER_STORE_MAGIC        (0x55535231u) /* "USR1" */
#define USER_STORE_VERSION      (2u)
#define USER_STORE_EXT_ADDR     ((uint32_t)0x00000000u)
#define APP_USER_MAX            64u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t user_count;
    uint8_t default_admin_deleted;
    uint8_t default_admin_is_admin_role;
    uint8_t default_admin_has_nfc;
    uint8_t default_admin_nfc_uid[4];
    uint8_t default_admin_has_fp;
    uint8_t reserved0;
    uint16_t default_admin_fp_page_id_1;
    uint16_t default_admin_fp_page_id_2;
    char default_admin_account[13];
    char default_admin_password[11];
    user_cred_t users[32];
    uint32_t tail_marker;
} app_user_store_v1_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t user_count;
    uint8_t default_admin_deleted;
    uint8_t default_admin_is_admin_role;
    uint8_t default_admin_has_nfc;
    uint8_t default_admin_nfc_uid[4];
    uint8_t default_admin_has_fp;
    uint8_t reserved0;
    uint16_t default_admin_fp_page_id_1;
    uint16_t default_admin_fp_page_id_2;
    char default_admin_account[13];
    char default_admin_password[11];
    user_cred_t users[APP_USER_MAX];
    uint32_t tail_marker;
} app_user_store_t;

typedef struct {
    uint8_t user_count;
    uint8_t default_admin_deleted;
    uint8_t default_admin_is_admin_role;
    char default_admin_account[13];
    char default_admin_password[11];
    user_cred_t users[APP_USER_MAX];
    uint8_t loaded;
    uint8_t store_valid;
    uint8_t store_version;
} test_unlock_ctx_t;

static test_unlock_ctx_t s_ctx;

static uint8_t test_store_tail_ok(uint32_t tail)
{
    return (tail == (USER_STORE_MAGIC ^ 0xA5A55A5Au)) ? 1u : 0u;
}

static uint8_t test_store_valid_v2(const app_user_store_t *st)
{
    if(st->magic != USER_STORE_MAGIC) {
        return 0u;
    }
    if(st->version != USER_STORE_VERSION) {
        return 0u;
    }
    if(st->user_count > APP_USER_MAX) {
        return 0u;
    }
    if(!test_store_tail_ok(st->tail_marker)) {
        return 0u;
    }
    return 1u;
}

static uint8_t test_store_valid_v1(const app_user_store_v1_t *st)
{
    if(st->magic != USER_STORE_MAGIC) {
        return 0u;
    }
    if(st->version != 1u) {
        return 0u;
    }
    if(st->user_count > 32u) {
        return 0u;
    }
    if(!test_store_tail_ok(st->tail_marker)) {
        return 0u;
    }
    return 1u;
}

static void test_unlock_apply_v2(const app_user_store_t *st)
{
    uint8_t i;

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.user_count = st->user_count;
    s_ctx.default_admin_deleted = st->default_admin_deleted;
    s_ctx.default_admin_is_admin_role = st->default_admin_is_admin_role;
    strncpy(s_ctx.default_admin_account, st->default_admin_account, sizeof(s_ctx.default_admin_account) - 1u);
    strncpy(s_ctx.default_admin_password, st->default_admin_password, sizeof(s_ctx.default_admin_password) - 1u);
    memcpy(s_ctx.users, st->users, sizeof(s_ctx.users));
    for(i = s_ctx.user_count; i < APP_USER_MAX; i++) {
        memset(&s_ctx.users[i], 0, sizeof(s_ctx.users[i]));
    }
    s_ctx.loaded = 1u;
    s_ctx.store_valid = 1u;
    s_ctx.store_version = 2u;
}

static void test_unlock_apply_v1(const app_user_store_v1_t *st)
{
    uint8_t i;

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.user_count = st->user_count;
    s_ctx.default_admin_deleted = st->default_admin_deleted;
    s_ctx.default_admin_is_admin_role = st->default_admin_is_admin_role;
    strncpy(s_ctx.default_admin_account, st->default_admin_account, sizeof(s_ctx.default_admin_account) - 1u);
    strncpy(s_ctx.default_admin_password, st->default_admin_password, sizeof(s_ctx.default_admin_password) - 1u);
    memcpy(s_ctx.users, st->users, (size_t)st->user_count * sizeof(user_cred_t));
    for(i = s_ctx.user_count; i < APP_USER_MAX; i++) {
        memset(&s_ctx.users[i], 0, sizeof(s_ctx.users[i]));
    }
    s_ctx.loaded = 1u;
    s_ctx.store_valid = 1u;
    s_ctx.store_version = 1u;
}

static uint8_t test_unlock_match_with_delete(const char *acc, const char *pwd)
{
    int idx;

    if(acc == NULL || pwd == NULL || acc[0] == '\0') {
        return 0u;
    }

    idx = user_auth_find_index_by_acc(s_ctx.users, s_ctx.user_count, acc);
    if(idx >= 0 && strcmp(pwd, s_ctx.users[idx].pwd) == 0) {
        return 1u;
    }

    if(s_ctx.default_admin_deleted == 0u &&
       s_ctx.default_admin_account[0] != '\0' &&
       strcmp(acc, s_ctx.default_admin_account) == 0 &&
       strcmp(pwd, s_ctx.default_admin_password) == 0) {
        return 1u;
    }

    return 0u;
}

static uint8_t test_admin_match(const char *acc, const char *pwd)
{
    return user_auth_admin_credentials_match(s_ctx.users, s_ctx.user_count,
                                             s_ctx.default_admin_account,
                                             s_ctx.default_admin_password,
                                             s_ctx.default_admin_is_admin_role,
                                             acc, pwd) ? 1u : 0u;
}

static void test_unlock_dump_users(void)
{
    uint8_t i;
    int idx1;

    test_rs485_logf("[AUTH] admin_del=%u admin_role=%u def_acc='%s' def_pwd='%s'\r\n",
                    (unsigned)s_ctx.default_admin_deleted,
                    (unsigned)s_ctx.default_admin_is_admin_role,
                    s_ctx.default_admin_account,
                    s_ctx.default_admin_password);

    for(i = 0u; i < s_ctx.user_count; i++) {
        test_rs485_logf("[AUTH] u[%u] act=%u adm=%u acc='%s' pwd='%s'\r\n",
                        (unsigned)i,
                        (unsigned)s_ctx.users[i].active,
                        (unsigned)s_ctx.users[i].is_admin,
                        s_ctx.users[i].acc,
                        s_ctx.users[i].pwd);
    }

    idx1 = user_auth_find_index_by_acc(s_ctx.users, s_ctx.user_count, ADMIN_DEFAULT_ACCOUNT);
    if(idx1 < 0) {
        test_rs485_log_str("[AUTH] account '1' NOT in active user list\r\n");
    } else {
        test_rs485_logf("[AUTH] account '1' idx=%d pwd='%s' (expect '%s')\r\n",
                        idx1,
                        s_ctx.users[idx1].pwd,
                        ADMIN_DEFAULT_PASSWORD);
    }
}

static void test_unlock_report_verdict(void)
{
    const char *acc = ADMIN_DEFAULT_ACCOUNT;
    const char *pwd = ADMIN_DEFAULT_PASSWORD;
    uint8_t unlock_ok;
    uint8_t admin_ok;

    unlock_ok = test_unlock_match_with_delete(acc, pwd);
    admin_ok = test_admin_match(acc, pwd);

    test_rs485_logf("[AUTH] simulate unlock_credentials_match_with_delete('%s','%s') => %s\r\n",
                    acc, pwd, unlock_ok ? "PASS" : "FAIL");
    test_rs485_logf("[AUTH] simulate admin_credentials_match('%s','%s') => %s\r\n",
                    acc, pwd, admin_ok ? "PASS" : "FAIL");

    if(unlock_ok != 0u) {
        test_rs485_log_str("[AUTH] PASS software-data: Flash 中 1/1111 校验通过\r\n");
        test_rs485_log_str("[AUTH] => 主程序进不了页：优先查触屏/双击进开锁页/UI 队列，非账号数据问题\r\n");
        test_rs485_log_str("[AUTH]    Host 首页须双击「开锁」钮；screen1 点 OK 才提交密码\r\n");
        return;
    }

    test_rs485_log_str("[AUTH] FAIL software-data: Flash 中 1/1111 无法通过校验\r\n");

    if(s_ctx.default_admin_deleted == 0u &&
       strcmp(s_ctx.default_admin_account, acc) == 0 &&
       strcmp(s_ctx.default_admin_password, pwd) != 0) {
        test_rs485_logf("[AUTH] cause: 默认管理员密码已改为 '%s' 而非 1111\r\n",
                        s_ctx.default_admin_password);
    } else if(s_ctx.default_admin_deleted != 0u) {
        test_rs485_log_str("[AUTH] cause: default_admin 已迁移/删除，仅 users[] 有效\r\n");
        if(user_auth_find_index_by_acc(s_ctx.users, s_ctx.user_count, acc) < 0) {
            test_rs485_log_str("[AUTH] cause: users[] 中无活跃账号 '1'\r\n");
        }
    }

    if(user_auth_find_index_by_acc(s_ctx.users, s_ctx.user_count, acc) >= 0 &&
       strcmp(s_ctx.users[user_auth_find_index_by_acc(s_ctx.users, s_ctx.user_count, acc)].pwd, pwd) != 0) {
        test_rs485_log_str("[AUTH] cause: users[] 中账号 1 的密码不是 1111\r\n");
    }
}

void test_unlock_diag_run(void)
{
    app_user_store_t st;
    app_user_store_v1_t st1;
    uint32_t jedec;

    memset(&s_ctx, 0, sizeof(s_ctx));

    test_rs485_log_str("\r\n[AUTH] ===== unlock account diag (W25Q user store) =====\r\n");
    test_rs485_log_str("[AUTH] simulate Host screen1 unlock + screen2 admin check\r\n");

    if(!bsp_w25q16_init()) {
        test_rs485_log_str("[AUTH] FAIL: W25Q init fail, cannot read user store, fix Flash HW first\r\n");
        bsp_w25q16_end_session();
        return;
    }

    jedec = bsp_w25q16_read_jedec_id();
    test_rs485_logf("[AUTH] W25Q jedec=0x%06lX\r\n", (unsigned long)jedec);

    if(!bsp_w25q16_read(USER_STORE_EXT_ADDR, (uint8_t *)&st, sizeof(st))) {
        test_rs485_log_str("[AUTH] FAIL: read user store @0 fail\r\n");
        bsp_w25q16_end_session();
        return;
    }
    bsp_w25q16_end_session();

    test_rs485_logf("[AUTH] raw magic=0x%08lX ver=%lu cnt=%u tail=0x%08lX\r\n",
                    (unsigned long)st.magic,
                    (unsigned long)st.version,
                    (unsigned)st.user_count,
                    (unsigned long)st.tail_marker);

    if(test_store_valid_v2(&st) != 0u) {
        test_unlock_apply_v2(&st);
        test_rs485_log_str("[AUTH] store v2 valid\r\n");
    } else {
        memcpy(&st1, &st, sizeof(st1));
        if(test_store_valid_v1(&st1) != 0u) {
            test_unlock_apply_v1(&st1);
            test_rs485_log_str("[AUTH] store v1 valid\r\n");
        } else {
            test_rs485_log_str("[AUTH] store INVALID (主程序会写出厂 1/1111 到 RAM，但 Flash 可能旧数据)\r\n");
            test_rs485_log_str("[AUTH] 空 Flash 时 Host users_storage_load 失败后会 users_ensure_default_admin\r\n");
            strncpy(s_ctx.default_admin_account, ADMIN_DEFAULT_ACCOUNT, sizeof(s_ctx.default_admin_account) - 1u);
            strncpy(s_ctx.default_admin_password, ADMIN_DEFAULT_PASSWORD, sizeof(s_ctx.default_admin_password) - 1u);
            s_ctx.default_admin_is_admin_role = 1u;
            s_ctx.loaded = 1u;
        }
    }

    if(s_ctx.loaded != 0u) {
        test_unlock_dump_users();
        test_unlock_report_verdict();
    }
}
