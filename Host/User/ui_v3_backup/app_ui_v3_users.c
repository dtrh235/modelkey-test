#include "app_ui_v3_users.h"

#if (APP_UI_V3_ENABLE == 1)

#include <string.h>
#include <stdio.h>

#include "app_config.h"
#include "app_state.h"
#include "app_ui_v3_theme.h"
#include "app_ui_v3.h"
#include "app_user_ops.h"
#include "app_temp_password.h"
#include "app_unlock_uart4.h"
#include "cloud_ota_service.h"
#include "app_cloud_bind_cmd.h"
#include "app_screen8_enroll.h"
#include "app_ui_v3_anim.h"
#include "stm32f4xx_hal.h"

#define UI3_LIST_MAX (APP_USER_MAX + 1u)

static ui3_user_row_t s_list_rows[UI3_LIST_MAX];
static uint8_t s_list_count;
static uint8_t s_unlock_fail_streak;
static char s_unlock_last_fail_acc[13];

static uint8_t s_enroll_kind;
static lv_obj_t *s_enroll_mask;

static int ui3_cmp_acc_str(const char *a, const char *b)
{
    size_t la;
    size_t lb;

    if(a == NULL) {
        a = "";
    }
    if(b == NULL) {
        b = "";
    }
    la = strlen(a);
    lb = strlen(b);
    if(la != lb) {
        return (la < lb) ? -1 : 1;
    }
    return strcmp(a, b);
}

static void ui3_list_rebuild(void)
{
    uint8_t i;
    uint8_t n = 0u;

    if(!g_default_admin_deleted && g_default_admin_account[0] != '\0') {
        (void)strncpy(s_list_rows[n].acc, g_default_admin_account, sizeof(s_list_rows[n].acc) - 1u);
        s_list_rows[n].acc[sizeof(s_list_rows[n].acc) - 1u] = '\0';
        s_list_rows[n].role_admin = g_default_admin_is_admin_role;
        s_list_rows[n].has_fp = g_default_admin_has_fp;
        s_list_rows[n].has_nfc = g_default_admin_has_nfc;
        n++;
    }
    for(i = 0u; i < g_user_count; i++) {
        if(n >= UI3_LIST_MAX) {
            break;
        }
        if(g_users[i].active == 0u || g_users[i].acc[0] == '\0') {
            continue;
        }
        (void)strncpy(s_list_rows[n].acc, g_users[i].acc, sizeof(s_list_rows[n].acc) - 1u);
        s_list_rows[n].acc[sizeof(s_list_rows[n].acc) - 1u] = '\0';
        s_list_rows[n].role_admin = g_users[i].is_admin;
        s_list_rows[n].has_fp = g_users[i].has_fp;
        s_list_rows[n].has_nfc = g_users[i].has_nfc;
        n++;
    }
    for(i = 1u; i < n; i++) {
        ui3_user_row_t key = s_list_rows[i];
        uint8_t j = i;
        while(j > 0u && ui3_cmp_acc_str(s_list_rows[j - 1u].acc, key.acc) > 0) {
            s_list_rows[j] = s_list_rows[j - 1u];
            j--;
        }
        s_list_rows[j] = key;
    }
    s_list_count = n;
}

uint8_t ui3_users_list_count(void)
{
    ui3_list_rebuild();
    return s_list_count;
}

bool ui3_users_list_row(uint8_t idx, ui3_user_row_t *out)
{
    ui3_list_rebuild();
    if(out == NULL || idx >= s_list_count) {
        return false;
    }
    *out = s_list_rows[idx];
    return true;
}

bool ui3_users_lookup(const char *acc, ui3_user_row_t *out)
{
    int idx;
    uint8_t i;

    if(acc == NULL || acc[0] == '\0' || out == NULL) {
        return false;
    }
    ui3_list_rebuild();
    for(i = 0u; i < s_list_count; i++) {
        if(strcmp(s_list_rows[i].acc, acc) == 0) {
            *out = s_list_rows[i];
            return true;
        }
    }
    idx = users_find_index_by_acc(acc);
    if(idx >= 0) {
        (void)strncpy(out->acc, g_users[idx].acc, sizeof(out->acc) - 1u);
        out->acc[sizeof(out->acc) - 1u] = '\0';
        out->role_admin = g_users[idx].is_admin;
        out->has_fp = g_users[idx].has_fp;
        out->has_nfc = g_users[idx].has_nfc;
        return true;
    }
    if(!g_default_admin_deleted && strcmp(acc, g_default_admin_account) == 0) {
        (void)strncpy(out->acc, g_default_admin_account, sizeof(out->acc) - 1u);
        out->acc[sizeof(out->acc) - 1u] = '\0';
        out->role_admin = g_default_admin_is_admin_role;
        out->has_fp = g_default_admin_has_fp;
        out->has_nfc = g_default_admin_has_nfc;
        return true;
    }
    return false;
}

void ui3_users_reset_add_pending(void)
{
    g_nfc_pending_uid_valid = 0u;
    memset(g_nfc_pending_uid, 0, sizeof(g_nfc_pending_uid));
    g_fp_pending_page_valid = 0u;
    g_fp_pending_page_id = 0xFFFFu;
    g_fp_pending_page2_valid = 0u;
    g_fp_pending_page_id_2 = 0xFFFFu;
    g_nfc_op = NFC_OP_NONE;
    g_screen8_fp_enroll_state = 0u;
    g_screen8_fp_enroll_result = 0u;
    g_nfc_enroll_state = 0u;
    g_nfc_enroll_result = 0u;
    g_enroll_target_acc[0] = '\0';
    g_enroll_ui_v3_mode = 0u;
    g_screen8_chip_read_ok = 0u;
    s_enroll_kind = 0u;
    if(s_enroll_mask != NULL && lv_obj_is_valid(s_enroll_mask)) {
        lv_obj_del(s_enroll_mask);
        s_enroll_mask = NULL;
    }
}

void ui3_users_prepare_add_screen(ui3_state_t *st)
{
    if(st == NULL) {
        return;
    }
    st->add_acc[0] = '\0';
    st->add_pwd[0] = '\0';
    st->add_admin = 0u;
    st->add_field = 0u;
    ui3_users_reset_add_pending();
}

void ui3_users_set_edit_target(const char *acc)
{
    if(acc == NULL) {
        g_screen5_found_acc[0] = '\0';
        g_screen5_found_is_admin = 0u;
        return;
    }
    (void)strncpy(g_screen5_found_acc, acc, sizeof(g_screen5_found_acc) - 1u);
    g_screen5_found_acc[sizeof(g_screen5_found_acc) - 1u] = '\0';
    {
        ui3_user_row_t row;
        if(ui3_users_lookup(acc, &row)) {
            g_screen5_found_is_admin = row.role_admin;
        } else {
            g_screen5_found_is_admin = 0u;
        }
    }
}

bool ui3_users_try_register(const char *acc, const char *pwd, bool is_admin)
{
    size_t la;
    size_t lp;
    bool ok = true;

    if(acc == NULL || pwd == NULL) {
        return false;
    }
    la = strlen(acc);
    lp = strlen(pwd);
    if(la < 1u || la > 12u || lp < 4u || lp > 10u) {
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
        ui3_users_reset_add_pending();
    }
    return ok;
}

bool ui3_users_try_delete(const char *acc)
{
    size_t len;

    if(acc == NULL) {
        return false;
    }
    len = strlen(acc);
    if(len < 1u || len > 12u) {
        return false;
    }
    if(!ui3_users_lookup(acc, NULL)) {
        return false;
    }
    return users_try_delete_by_acc(acc);
}

void ui3_auth_unlock_success(const char *acc, const char *method)
{
    const char *m = (method != NULL) ? method : "password";
    const char *a = (acc != NULL) ? acc : "";

    app_unlock_uart4_on_unlock_ok(a, m);
    cloud_ota_service_report_event(CLOUD_EVT_UNLOCK_OK, a);
    cloud_ota_service_queue_unlock_report(a, m, HAL_GetTick());
}

uint8_t ui3_auth_try_unlock(ui3_state_t *st, const char *acc, const char *pwd)
{
    uint32_t now;

    if(st == NULL) {
        return 0u;
    }
    if(st->lockout_until_ms > HAL_GetTick()) {
        return 2u;
    }
    if(app_temp_password_try_unlock(acc, pwd) != 0u) {
        s_unlock_fail_streak = 0u;
        st->unlock_errors = 0u;
        st->unlock_show_err = 0u;
        st->lockout_until_ms = 0u;
        cloud_ota_service_set_unlock_guest(acc);
        ui3_auth_unlock_success("temporary account", "temporary-password");
        app_cloud_queue_temp_password_used(acc);
        return 1u;
    }
    if(unlock_credentials_match_with_delete(acc, pwd)) {
        s_unlock_fail_streak = 0u;
        st->unlock_errors = 0u;
        st->unlock_show_err = 0u;
        st->lockout_until_ms = 0u;
        ui3_auth_unlock_success(acc, "password");
        return 1u;
    }

    now = HAL_GetTick();
    if(acc != NULL) {
        (void)strncpy(s_unlock_last_fail_acc, acc, sizeof(s_unlock_last_fail_acc) - 1u);
        s_unlock_last_fail_acc[sizeof(s_unlock_last_fail_acc) - 1u] = '\0';
    }
    s_unlock_fail_streak++;
    st->unlock_show_err = 1u;
    if(s_unlock_fail_streak >= UI3_UNLOCK_FAIL_LIMIT) {
        s_unlock_fail_streak = 0u;
        st->unlock_errors = 0u;
        st->lockout_until_ms = now + UI3_UNLOCK_LOCKOUT_MS;
        return 2u;
    }
    st->unlock_errors = s_unlock_fail_streak;
    return 0u;
}

bool ui3_auth_try_admin(ui3_state_t *st, const char *acc, const char *pwd)
{
    if(st == NULL) {
        return false;
    }
    if(admin_credentials_match_with_delete(acc, pwd)) {
        st->admin_show_err = 0u;
        cloud_ota_service_report_event(CLOUD_EVT_ADMIN_LOGIN_OK, acc);
        return true;
    }
    st->admin_show_err = 1u;
    cloud_ota_service_report_event(CLOUD_EVT_ADMIN_LOGIN_FAIL, acc);
    return false;
}

uint8_t ui3_users_enroll_active(void)
{
    return (g_nfc_enroll_state == 1u || g_screen8_fp_enroll_state == 1u) ? 1u : 0u;
}

static void ui3_enroll_show_busy(lv_obj_t *parent, const char *title, const char *sub)
{
    lv_obj_t *mask;
    lv_obj_t *sheet;

    if(s_enroll_mask != NULL && lv_obj_is_valid(s_enroll_mask)) {
        lv_obj_del(s_enroll_mask);
    }
    s_enroll_mask = NULL;
    if(parent == NULL || !lv_obj_is_valid(parent)) {
        return;
    }

    mask = lv_obj_create(parent);
    s_enroll_mask = mask;
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, UI3_W, UI3_H);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(mask, LV_OPA_40, 0);

    sheet = lv_obj_create(mask);
    lv_obj_set_size(sheet, 200, 130);
    lv_obj_center(sheet);
    lv_obj_set_style_radius(sheet, 18, 0);
    lv_obj_set_style_bg_color(sheet, UI3_COL_SURFACE, 0);

    lv_obj_t *tit = lv_label_create(sheet);
    lv_label_set_text(tit, title ? title : "录入中");
    lv_obj_add_style(tit, ui3_style_title(), 0);
    lv_obj_align(tit, LV_ALIGN_CENTER, 0, -8);
    lv_obj_t *hint = lv_label_create(sheet);
    lv_label_set_text(hint, sub ? sub : "请稍候");
    lv_obj_add_style(hint, ui3_style_label(), 0);
    lv_obj_set_style_text_color(hint, UI3_COL_INK_FAINT, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
}

static void ui3_enroll_begin(uint8_t kind, lv_obj_t *parent, const char *title, const char *sub)
{
    s_enroll_kind = kind;
    g_enroll_ui_v3_mode = 1u;
    ui3_enroll_show_busy(parent, title, sub);
}

void ui3_users_fp_start_add(lv_obj_t *parent)
{
    g_nfc_op = NFC_OP_NONE;
    g_enroll_target_acc[0] = '\0';
    if(screen8_start_fp_enroll_hw() == 0u) {
        ui3_show_modal_result(parent, "指纹录入失败", "请重试", false);
        return;
    }
    ui3_enroll_begin(1u, parent, "指纹录入中", "请按压手指 3 次");
}

void ui3_users_nfc_start_add(lv_obj_t *parent)
{
    g_nfc_op = NFC_OP_ENROLL_SCREEN8;
    g_enroll_target_acc[0] = '\0';
    if(screen8_start_nfc_enroll_hw() == 0u) {
        ui3_show_modal_result(parent, "NFC 录入失败", "请重试", false);
        return;
    }
    ui3_enroll_begin(2u, parent, "NFC 录入中", "请将卡片靠近读卡区");
}

void ui3_users_fp_start_replace(lv_obj_t *parent, const char *acc)
{
    if(acc == NULL || acc[0] == '\0') {
        return;
    }
    ui3_users_set_edit_target(acc);
    (void)strncpy(g_enroll_target_acc, acc, sizeof(g_enroll_target_acc) - 1u);
    g_enroll_target_acc[sizeof(g_enroll_target_acc) - 1u] = '\0';
    g_nfc_op = NFC_OP_NONE;
    if(screen8_start_fp_enroll_hw() == 0u) {
        g_enroll_target_acc[0] = '\0';
        ui3_show_modal_result(parent, "指纹录入失败", "请重试", false);
        return;
    }
    ui3_enroll_begin(3u, parent, "指纹录入中", "请按压手指 3 次");
}

void ui3_users_nfc_start_replace(lv_obj_t *parent, const char *acc)
{
    if(acc == NULL || acc[0] == '\0') {
        return;
    }
    ui3_users_set_edit_target(acc);
    g_nfc_op = NFC_OP_REPLACE_SCREEN9;
    if(screen8_start_nfc_enroll_hw() == 0u) {
        ui3_show_modal_result(parent, "NFC 录入失败", "请重试", false);
        return;
    }
    ui3_enroll_begin(4u, parent, "NFC 录入中", "请将卡片靠近读卡区");
}

bool ui3_users_clear_fp(const char *acc)
{
    if(acc == NULL || acc[0] == '\0') {
        return false;
    }
    users_clear_fp_by_acc(acc, 1u);
    return true;
}

bool ui3_users_clear_nfc(const char *acc)
{
    if(acc == NULL || acc[0] == '\0') {
        return false;
    }
    users_clear_nfc_by_acc(acc);
    cloud_ota_service_report_event(CLOUD_EVT_NFC_DELETE_OK, acc);
    return true;
}

void ui3_users_enroll_poll(void)
{
    lv_obj_t *parent;
    uint8_t ok;
    const char *title;

    if(s_enroll_kind == 0u) {
        return;
    }
    if(ui3_users_enroll_active() != 0u) {
        return;
    }

    parent = lv_scr_act();
    if(s_enroll_kind == 1u || s_enroll_kind == 3u) {
        ok = (g_screen8_fp_enroll_result != 0u) ? 1u : 0u;
        title = ok ? "指纹录入完成" : "指纹录入失败";
    } else {
        ok = (g_nfc_enroll_result != 0u) ? 1u : 0u;
        title = ok ? "NFC 绑定完成" : "NFC 绑定失败";
    }

    if(s_enroll_mask != NULL && lv_obj_is_valid(s_enroll_mask)) {
        lv_obj_del(s_enroll_mask);
        s_enroll_mask = NULL;
    }
    g_enroll_target_acc[0] = '\0';
    g_nfc_op = NFC_OP_NONE;
    g_enroll_ui_v3_mode = 0u;
    s_enroll_kind = 0u;
    {
        ui3_state_t *st = ui3_state();
        ui3_user_row_t row;
        if(st != NULL && st->edit_acc[0] != '\0' && ui3_users_lookup(st->edit_acc, &row)) {
            st->edit_has_fp = row.has_fp;
            st->edit_has_nfc = row.has_nfc;
        }
    }
    ui3_show_modal_result(parent, title, ok ? NULL : "请重试", ok != 0u);
}

#endif
