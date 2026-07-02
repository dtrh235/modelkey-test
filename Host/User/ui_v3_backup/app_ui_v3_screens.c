#include "app_ui_v3_screens.h"
#include "app_ui_v3_widgets.h"
#include "app_ui_v3_theme.h"
#include "app_ui_v3_anim.h"
#include "app_ui_v3_gesture.h"
#include "app_ui_v3_users.h"
#include "app_ui_v3.h"
#include "app_ui_v3_diag.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const struct {
    const char *icon;
    const char *title;
    const char *sub;
    ui3_scr_id_t id;
} s_menu[] = {
    { "增", "增加用户",       "录入账号与凭证", UI3_SCR_ADD_USER },
    { "删", "删除用户",       "按学号移除",     UI3_SCR_DEL_USER },
    { "查", "查找(更改)用户", "搜索并编辑",     UI3_SCR_SEARCH },
    { "显", "显示所有用户",   "完整名单",       UI3_SCR_USER_LIST },
    { "网", "设置无线网络",   "网络与云端",     UI3_SCR_WIFI },
    { "配", "设备配对",       "JoyfulZone",     UI3_SCR_PAIR },
};
#define UI3_MENU_COUNT 6u

static const struct {
    const char *ssid;
    uint8_t rssi;
    uint8_t connected;
} s_wifi[] = {
    { "办公室-5G", 4, 1 },
    { "访客网络",  3, 0 },
    { "物联网桥接", 2, 0 },
    { "家里WiFi",  3, 0 },
};

static void cb_nav_back(lv_event_t *e)
{
    (void)e;
    ui3_nav_back();
}

static bool unlock_is_locked(ui3_state_t *st)
{
    return (st->lockout_until_ms > HAL_GetTick());
}

static void ui3_prepare_unlock(ui3_state_t *st)
{
    st->unlock_acc[0] = '\0';
    st->unlock_pwd[0] = '\0';
    st->unlock_field = 0u;
    st->unlock_show_err = 0u;
    st->unlock_errors = 0u;
}

static void ui3_prepare_admin(ui3_state_t *st)
{
    st->admin_acc[0] = '\0';
    st->admin_pwd[0] = '\0';
    st->admin_field = 0u;
    st->admin_show_err = 0u;
}

static void cb_home_footer(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    uint8_t btn = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    if(st->scr != UI3_SCR_HOME) {
        return;
    }
    if(btn != st->home_nav_sel) {
        st->home_nav_sel = btn;
        ui3_home_footer_sel(st);
        return;
    }
    if(btn == 0u) {
        ui3_prepare_admin(st);
        ui3_nav_to(UI3_SCR_ADMIN, true);
    } else {
        ui3_prepare_unlock(st);
        ui3_nav_to(UI3_SCR_UNLOCK, true);
    }
    (void)e;
}

static void cb_go_admin(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    ui3_prepare_admin(st);
    ui3_nav_to(UI3_SCR_ADMIN, true);
}

static void cb_menu_enter(lv_event_t *e)
{
    ui3_state_t *st = (ui3_state_t *)lv_event_get_user_data(e);
    ui3_scr_id_t id = s_menu[st->menu_index].id;

    if(id == UI3_SCR_ADD_USER) {
        ui3_users_prepare_add_screen(st);
    }
    ui3_nav_to(id, true);
}

static void cb_menu_pick(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    st->menu_index = idx;
    if(st->menu_track && lv_obj_is_valid(st->menu_track)) {
        uint32_t n = lv_obj_get_child_cnt(st->menu_track);
        for(uint32_t i = 0; i < n; i++) {
            ui3_card_set_sel(lv_obj_get_child(st->menu_track, (int32_t)i), (i == idx), true);
        }
    }
}

static void cb_home_lock(lv_event_t *e)
{
    static uint32_t last_ms;
    ui3_state_t *st = ui3_state();
    uint32_t now = HAL_GetTick();

    if((now - last_ms) < 450u) {
        st->home_sensing = 1u;
        ui3_show_unlock_ripple(lv_obj_get_screen(lv_event_get_target(e)));
        st->home_sensing = 0u;
        last_ms = 0;
    } else {
        last_ms = now;
    }
}

static void cb_form_field(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    if(st->scr == UI3_SCR_UNLOCK) {
        if(unlock_is_locked(st)) {
            return;
        }
        st->unlock_field = idx;
        st->unlock_show_err = 0u;
    } else if(st->scr == UI3_SCR_ADMIN) {
        st->admin_field = idx;
        st->admin_show_err = 0u;
    } else {
        return;
    }
    if(st->unlock_show_err != 0u || st->admin_show_err != 0u || !ui3_inputs_live_refresh(st)) {
        ui3_reload_current();
    }
}

static void cb_unlock_ok(lv_event_t *e)
{
    (void)e;
    ui3_state_t *st = ui3_state();
    lv_obj_t *scr = lv_scr_act();
    uint8_t rc;

    if(unlock_is_locked(st)) {
        return;
    }
    rc = ui3_auth_try_unlock(st, st->unlock_acc, st->unlock_pwd);
    if(rc == 1u) {
        ui3_show_unlock_ripple(scr);
    } else {
        ui3_reload_current();
    }
}

static void cb_admin_ok(lv_event_t *e)
{
    (void)e;
    ui3_state_t *st = ui3_state();
    if(ui3_auth_try_admin(st, st->admin_acc, st->admin_pwd)) {
        ui3_nav_to(UI3_SCR_MENU, true);
    } else {
        ui3_reload_current();
    }
}

static void cb_search_ok(lv_event_t *e)
{
    (void)e;
    ui3_state_t *st = ui3_state();
    ui3_user_row_t row;

    if(!ui3_users_lookup(st->search_acc, &row)) {
        ui3_show_modal_result(lv_scr_act(), "未找到该用户", NULL, false);
        return;
    }
    (void)strncpy(st->edit_acc, row.acc, sizeof(st->edit_acc) - 1u);
    st->edit_acc[sizeof(st->edit_acc) - 1u] = '\0';
    st->edit_is_admin = row.role_admin;
    st->edit_has_fp = row.has_fp;
    st->edit_has_nfc = row.has_nfc;
    ui3_users_set_edit_target(st->edit_acc);
    ui3_nav_to(UI3_SCR_EDIT_USER, true);
}

static void cb_del_ok(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    if(ui3_users_try_delete(st->del_acc)) {
        ui3_show_modal_result(lv_scr_act(), "删除成功", st->del_acc, true);
        st->del_acc[0] = '\0';
    } else {
        ui3_show_modal_result(lv_scr_act(), "删除失败", NULL, false);
    }
}

static void cb_save_user(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    if(ui3_users_try_register(st->add_acc, st->add_pwd, st->add_admin != 0u)) {
        ui3_show_modal_result(lv_scr_act(), "保存成功", st->add_acc, true);
    } else {
        ui3_show_modal_result(lv_scr_act(), "保存失败", "账号重复或格式错误", false);
    }
}

static void cb_fp_enroll(lv_event_t *e)
{
    (void)e;
    ui3_users_fp_start_add(lv_scr_act());
}

static void cb_nfc_enroll(lv_event_t *e)
{
    (void)e;
    ui3_users_nfc_start_add(lv_scr_act());
}

static void cb_go_fp_mgmt(lv_event_t *e)
{
    (void)e;
    ui3_nav_to(UI3_SCR_FP_MGMT, true);
}

static void cb_go_nfc_mgmt(lv_event_t *e)
{
    (void)e;
    ui3_nav_to(UI3_SCR_NFC_MGMT, true);
}

static void cb_wifi_pick(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    st->wifi_index = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
}

static void cb_wifi_scan(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    st->wifi_scanning = 1u;
    ui3_show_scan_modal(lv_scr_act(), "扫描中", "正在搜索附近热点…");
    st->wifi_scanning = 0u;
}

static void cb_wifi_connect(lv_event_t *e)
{
    (void)e;
    ui3_state_t *st = ui3_state();
    if(s_wifi[st->wifi_index].connected) {
        ui3_show_toast(lv_scr_act(), "已连接");
    } else {
        ui3_show_wifi_pwd_modal(lv_scr_act(), s_wifi[st->wifi_index].ssid);
    }
}

static void cb_pair_regen(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    for(uint8_t i = 0; i < 6u; i++) {
        st->pair_code[i] = (char)('0' + (rand() % 10));
    }
    st->pair_code[6] = '\0';
    ui3_nav_to(UI3_SCR_PAIR, false);
}

static void cb_nfc_change(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    ui3_users_nfc_start_replace(lv_scr_act(), st->edit_acc);
}

static void cb_nfc_remove(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    if(ui3_users_clear_nfc(st->edit_acc)) {
        st->edit_has_nfc = 0u;
        ui3_show_modal_result(lv_scr_act(), "已移除 NFC", NULL, true);
    } else {
        ui3_show_modal_result(lv_scr_act(), "移除失败", NULL, false);
    }
}

static void cb_fp_change(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    ui3_users_fp_start_replace(lv_scr_act(), st->edit_acc);
}

static void cb_fp_clear(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    if(ui3_users_clear_fp(st->edit_acc)) {
        st->edit_has_fp = 0u;
        ui3_show_modal_result(lv_scr_act(), "已清除指纹", NULL, true);
    } else {
        ui3_show_modal_result(lv_scr_act(), "清除失败", NULL, false);
    }
}

static void build_home(ui3_state_t *st, ui3_layout_t *lo)
{
    st->home_nav_sel = UI3_HOME_NAV_NONE;
    ui3_topbar(lo, st);
    if(lo->cloud) {
        lv_obj_add_flag(lo->cloud, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(lo->cloud, cb_go_admin, LV_EVENT_CLICKED, NULL);
    }
    ui3_home_build(lo, st, cb_home_footer, cb_home_footer, cb_home_lock);
}

static void build_menu(ui3_state_t *st, ui3_layout_t *lo)
{
    ui3_topbar(lo, st);
    ui3_page_head(lo, "账号管理", NULL, true);

    st->scroll_row_h = UI3_LIST_ROW_H;
    st->scroll_visible = UI3_LIST_VISIBLE;
    st->scroll_max = (UI3_MENU_COUNT > UI3_LIST_VISIBLE) ? (UI3_MENU_COUNT - UI3_LIST_VISIBLE) : 0;
    if(st->scroll_px > st->scroll_max) {
        st->scroll_px = st->scroll_max;
    }

    lv_obj_t *track = ui3_scroll_viewport(lo, 170);
    st->menu_track = track;
    for(uint8_t i = 0; i < UI3_MENU_COUNT; i++) {
        ui3_menu_card(track, s_menu[i].icon, s_menu[i].title, s_menu[i].sub,
                      i == st->menu_index, cb_menu_pick, (void *)(uintptr_t)i);
    }
    ui3_scroll_apply(st, track, false);
    ui3_scroll_dots(lo, st, 252);
    ui3_gesture_bind(st, track);
    ui3_footer_dock(lo, "进入", true, cb_menu_enter, cb_nav_back, st, true);
}

static void build_add_user(ui3_state_t *st, ui3_layout_t *lo)
{
    char pwd_mask[32];

    ui3_pwd_mask_fill(pwd_mask, sizeof(pwd_mask), st->add_pwd);
    ui3_topbar(lo, st);
    ui3_page_head(lo, "新用户", NULL, true);

    st->scroll_row_h = UI3_ADD_SCROLL_STEP;
    st->scroll_visible = 1;
    st->scroll_max = UI3_ADD_SCROLL_MAX;
    if(st->scroll_px > st->scroll_max) {
        st->scroll_px = st->scroll_max;
    }

    lv_obj_t *track = ui3_scroll_viewport(lo, 168);
    ui3_inputs_track(st, 0u, ui3_field_block(track, "学号 / 工号", st->add_acc, st->add_field == 0,
                                               false, NULL, NULL));
    ui3_inputs_track(st, 1u, ui3_field_block(track, "密码", pwd_mask, st->add_field == 1,
                                               false, NULL, NULL));
    ui3_role_picker(track, st->add_admin != 0, st->add_field == 2);
    ui3_bio_block(track, cb_fp_enroll, cb_nfc_enroll, st);

    ui3_scroll_apply(st, track, false);
    ui3_gesture_bind(st, track);
    ui3_footer_dock(lo, "保存", true, cb_save_user, cb_nav_back, st, true);
}

static void build_user_list(ui3_state_t *st, ui3_layout_t *lo)
{
    char sub[20];
    uint8_t count = ui3_users_list_count();

    snprintf(sub, sizeof(sub), "%u 位已注册", (unsigned)count);
    ui3_topbar(lo, st);
    ui3_page_head(lo, "全部用户", sub, false);

    st->scroll_row_h = UI3_LIST_ROW_H;
    st->scroll_visible = 5;
    st->scroll_max = (count > 5u) ? (count - 5u) : 0;
    if(st->scroll_px > st->scroll_max) {
        st->scroll_px = st->scroll_max;
    }
    lv_obj_t *track = ui3_scroll_viewport(lo, 168);
    for(uint8_t i = 0; i < count; i++) {
        ui3_user_row_t row;
        if(ui3_users_list_row(i, &row)) {
            ui3_user_row(track, row.acc, row.role_admin != 0, false);
        }
    }
    ui3_scroll_apply(st, track, false);
    ui3_gesture_bind(st, track);
    ui3_footer_back(lo, cb_nav_back, st);
}

static void build_wifi(ui3_state_t *st, ui3_layout_t *lo)
{
    ui3_topbar(lo, st);
    ui3_page_head(lo, "无线网络", NULL, true);

    lv_obj_t *banner = ui3_wifi_banner(lo->root, s_wifi[0].connected ? s_wifi[0].ssid : "未连接",
                                       s_wifi[0].connected ? "云端通道就绪" : "请选择网络",
                                       s_wifi[0].connected ? 4u : 1u);
    lv_obj_align(banner, LV_ALIGN_TOP_MID, 0, 72);

    lv_obj_t *scan = ui3_wifi_scan_link(lo->root, st->wifi_scanning != 0u, cb_wifi_scan, st);
    lv_obj_align(scan, LV_ALIGN_TOP_MID, 0, 120);

    st->scroll_row_h = UI3_LIST_ROW_H;
    st->scroll_max = 1;
    lv_obj_t *track = ui3_scroll_viewport(lo, 130);
    lv_obj_align(lv_obj_get_parent(track), LV_ALIGN_TOP_MID, 0, 136);
    for(uint8_t i = 0; i < 4u; i++) {
        ui3_wifi_item(track, s_wifi[i].ssid, s_wifi[i].rssi, s_wifi[i].connected != 0u,
                      i == st->wifi_index, cb_wifi_pick, (void *)(uintptr_t)i);
    }
    ui3_scroll_apply(st, track, false);
    ui3_gesture_bind(st, track);
    ui3_footer_dock(lo, "连接", true, cb_wifi_connect, cb_nav_back, st, true);
}

static void build_pair(ui3_state_t *st, ui3_layout_t *lo)
{
    ui3_topbar(lo, st);
    ui3_page_head(lo, "设备配对", "JoyfulZone 智慧门禁", false);
    ui3_pair_zone(lo, st);
    st->scroll_max = 0;
    ui3_gesture_bind(st, NULL);
    ui3_footer_pair(lo, cb_nav_back, cb_pair_regen, st);
}

static void build_edit_user(ui3_state_t *st, ui3_layout_t *lo)
{
    ui3_user_row_t row;

    if(st->edit_acc[0] != '\0' && ui3_users_lookup(st->edit_acc, &row)) {
        st->edit_is_admin = row.role_admin;
        st->edit_has_fp = row.has_fp;
        st->edit_has_nfc = row.has_nfc;
    }

    ui3_topbar(lo, st);
    ui3_page_head(lo, st->edit_acc[0] ? st->edit_acc : "用户", "编辑用户资料", true);

    st->scroll_row_h = UI3_LIST_ROW_H;
    st->scroll_visible = 4;
    st->scroll_max = 1;
    lv_obj_t *track = ui3_scroll_viewport(lo, 168);
    ui3_detail_card(track, "账号", st->edit_acc, false, NULL, NULL);
    ui3_detail_card(track, "密码", "······", false, NULL, NULL);
    ui3_detail_card(track, "指纹", st->edit_has_fp ? "已录入" : "未录入", true, cb_go_fp_mgmt, st);
    ui3_detail_card(track, "身份", st->edit_is_admin ? "管理员" : "用户", false, NULL, NULL);
    ui3_detail_card(track, "NFC 卡", st->edit_has_nfc ? "已绑定" : "未绑定", true, cb_go_nfc_mgmt, st);
    ui3_scroll_apply(st, track, false);
    ui3_gesture_bind(st, track);
    ui3_footer_back(lo, cb_nav_back, st);
}

static void build_unlock(ui3_state_t *st, ui3_layout_t *lo)
{
    char pwd_mask[32];
    char lock_hint[40];
    bool locked = unlock_is_locked(st);
    const char *err = (st->unlock_show_err != 0u && !locked) ? "凭证不正确" : NULL;

    ui3_pwd_mask_fill(pwd_mask, sizeof(pwd_mask), st->unlock_pwd);
    lock_hint[0] = '\0';
    if(locked) {
        uint32_t rem = (st->lockout_until_ms - HAL_GetTick() + 999u) / 1000u;
        snprintf(lock_hint, sizeof(lock_hint), "%lu\n秒后可再次尝试", (unsigned long)rem);
    }
    ui3_topbar(lo, st);
    ui3_page_head(lo, "验证身份", "学号 / 工号与密码", false);
    ui3_form_zone(lo, st, "账号", st->unlock_acc, st->unlock_field == 0,
                  "密码", pwd_mask, st->unlock_field == 1,
                  err, locked ? lock_hint : NULL, locked, cb_form_field);
    st->scroll_max = 0;
    ui3_gesture_bind(st, NULL);
    ui3_footer_dock(lo, "开锁", true, locked ? NULL : cb_unlock_ok, cb_nav_back, st, !locked);
}

static void build_admin(ui3_state_t *st, ui3_layout_t *lo)
{
    char pwd_mask[32];
    const char *err = (st->admin_show_err != 0u) ? "验证失败" : NULL;

    ui3_pwd_mask_fill(pwd_mask, sizeof(pwd_mask), st->admin_pwd);
    ui3_topbar(lo, st);
    ui3_page_head(lo, "管理入口", "需要管理员权限", false);
    ui3_form_zone(lo, st, "管理员账号", st->admin_acc, st->admin_field == 0,
                  "密码", pwd_mask, st->admin_field == 1,
                  err, NULL, false, cb_form_field);
    st->scroll_max = 0;
    ui3_gesture_bind(st, NULL);
    ui3_footer_dock(lo, "进入", true, cb_admin_ok, cb_nav_back, st, true);
}

lv_obj_t *ui3_build_screen(ui3_state_t *st)
{
    ui3_layout_t lo;

    ui3_diag_nav("build", st->scr, 0u);

    st->lbl_top_time = NULL;
    st->lbl_clock_big = NULL;
    st->lbl_date = NULL;
    st->btn_home_menu = NULL;
    st->btn_home_unlock = NULL;
    st->menu_track = NULL;
    ui3_inputs_reset(st);

    lo.root = ui3_screen_create();
    if(lo.root == NULL) {
        ui3_diag_fmt("build", "ui3_screen_create NULL");
        ui3_diag_mem("build_root_fail");
        return NULL;
    }
    lo.body = NULL;
    lo.footer = NULL;
    lo.scroll_track = NULL;
    lo.top_time = NULL;
    lo.cloud = NULL;

    switch(st->scr) {
    case UI3_SCR_HOME:
        build_home(st, &lo);
        break;
    case UI3_SCR_UNLOCK:
        build_unlock(st, &lo);
        break;
    case UI3_SCR_ADMIN:
        build_admin(st, &lo);
        break;
    case UI3_SCR_MENU:
        build_menu(st, &lo);
        break;
    case UI3_SCR_ADD_USER:
        build_add_user(st, &lo);
        break;
    case UI3_SCR_DEL_USER:
        ui3_topbar(&lo, st);
        ui3_page_head(&lo, "删除用户", "此操作不可撤销", false);
        ui3_form_zone(&lo, st, "学号 / 工号", st->del_acc, 1, NULL, NULL, 0, NULL, NULL, false, NULL);
        st->scroll_max = 0;
        ui3_gesture_bind(st, NULL);
        ui3_footer_dock(&lo, "删除", true, cb_del_ok, cb_nav_back, st, true);
        break;
    case UI3_SCR_SEARCH:
        ui3_topbar(&lo, st);
        ui3_page_head(&lo, "查找用户", "输入学号 / 工号", false);
        ui3_form_zone(&lo, st, "学号 / 工号", st->search_acc, 1, NULL, NULL, 0, NULL, NULL, false, NULL);
        st->scroll_max = 0;
        ui3_gesture_bind(st, NULL);
        ui3_footer_dock(&lo, "搜索", true, cb_search_ok, cb_nav_back, st, true);
        break;
    case UI3_SCR_EDIT_USER:
        build_edit_user(st, &lo);
        break;
    case UI3_SCR_USER_LIST:
        build_user_list(st, &lo);
        break;
    case UI3_SCR_WIFI:
        build_wifi(st, &lo);
        break;
    case UI3_SCR_PAIR:
        build_pair(st, &lo);
        break;
    case UI3_SCR_NFC_MGMT:
        ui3_topbar(&lo, st);
        ui3_page_head(&lo, "NFC 卡", "更改或移除卡片", false);
        ui3_action_stack(lo.root, "更换卡片", "移除 NFC", cb_nfc_change, cb_nfc_remove, st);
        st->scroll_max = 0;
        ui3_gesture_bind(st, NULL);
        ui3_footer_back(&lo, cb_nav_back, st);
        break;
    case UI3_SCR_FP_MGMT:
        ui3_topbar(&lo, st);
        ui3_page_head(&lo, "指纹", "更改或清除模板", false);
        ui3_action_stack(lo.root, "重新录入", "清除指纹", cb_fp_change, cb_fp_clear, st);
        st->scroll_max = 0;
        ui3_gesture_bind(st, NULL);
        ui3_footer_back(&lo, cb_nav_back, st);
        break;
    default:
        break;
    }

    ui3_anim_screen_in(lo.root);
    ui3_diag_scr("build_ok", st->scr, lo.root);
    return lo.root;
}
