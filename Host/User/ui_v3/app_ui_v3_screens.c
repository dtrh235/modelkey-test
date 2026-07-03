#include "app_ui_v3_screens.h"
#include "app_ui_v3_widgets.h"
#include "app_ui_v3_theme.h"
#include "app_ui_v3_anim.h"
#include "app_ui_v3_gesture.h"
#include "app_ui_v3_users.h"
#include "app_ui_v3_services.h"
#include "app_ui_v3.h"
#include "app_wifi_scan.h"
#include "cloud_aliyun_at.h"
#include "app_state.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const struct {
    ui3_menu_icon_t icon;
    const char *title;
    ui3_scr_id_t id;
} s_menu[] = {
    { UI3_MENU_ICON_USER,   "增加用户",       UI3_SCR_ADD_USER },
    { UI3_MENU_ICON_TRASH,  "删除用户",       UI3_SCR_DEL_USER },
    { UI3_MENU_ICON_SEARCH, "查找(更改)用户", UI3_SCR_SEARCH },
    { UI3_MENU_ICON_LIST,   "显示所有用户",   UI3_SCR_USER_LIST },
    { UI3_MENU_ICON_WIFI,   "设置无线网络",   UI3_SCR_WIFI },
    { UI3_MENU_ICON_LINK,   "设备配对",       UI3_SCR_PAIR },
};

static void cb_nav_back(lv_event_t *e)
{
    (void)e;
    ui3_nav_back();
}

static void cb_menu_back_home(lv_event_t *e)
{
    (void)e;
    ui3_nav_home();
}

static void menu_enter_impl(ui3_state_t *st)
{
    ui3_scr_id_t id;

    if(st == NULL) {
        return;
    }
    id = s_menu[st->menu_index].id;
    st->menu_armed = 0u;
    if(id == UI3_SCR_ADD_USER) {
        ui3_users_prepare_add_screen(st);
        st->scroll_px = 0;
    } else if(id == UI3_SCR_SEARCH) {
        st->search_acc[0] = '\0';
        st->scroll_px = 0;
    } else if(id == UI3_SCR_DEL_USER) {
        st->del_acc[0] = '\0';
        st->scroll_px = 0;
    } else if(id == UI3_SCR_USER_LIST) {
        st->scroll_px = 0;
    } else if(id == UI3_SCR_PAIR) {
        ui3_pair_on_enter(st);
    }
    ui3_nav_to(id, true);
}

void ui3_menu_enter_selected(void)
{
    menu_enter_impl(ui3_state());
}

void ui3_menu_refresh_sel(void)
{
    ui3_state_t *st = ui3_state();
    uint32_t n;
    uint32_t i;

    if(st == NULL || st->menu_track == NULL || !lv_obj_is_valid(st->menu_track)) {
        return;
    }
    n = lv_obj_get_child_cnt(st->menu_track);
    for(i = 0u; i < n; i++) {
        ui3_card_set_sel(lv_obj_get_child(st->menu_track, (int32_t)i), (i == st->menu_index), true);
    }
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
    (void)e;
    menu_enter_impl(ui3_state());
}

static void cb_menu_pick(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    if(idx != st->menu_index) {
        st->menu_index = idx;
        st->menu_armed = 0u;
        ui3_menu_refresh_sel();
        return;
    }
    if(st->menu_armed == 0u) {
        st->menu_armed = 1u;
        ui3_menu_refresh_sel();
        return;
    }
    menu_enter_impl(st);
}

static void cb_home_lock(lv_event_t *e)
{
    static uint32_t last_ms;
    ui3_state_t *st = ui3_state();
    uint32_t now = HAL_GetTick();

    if((now - last_ms) < 450u) {
        lv_obj_t *mask;
        st->home_sensing = 1u;
        mask = ui3_show_unlock_welcome(lv_obj_get_screen(lv_event_get_target(e)));
        ui3_welcome_schedule_close(mask, 2400u);
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
        ui3_prepare_unlock(st);
        st->pending_welcome = 1u;
        ui3_nav_home();
        (void)scr;
    } else {
        ui3_reload_current();
    }
}

static void cb_admin_ok(lv_event_t *e)
{
    (void)e;
    ui3_state_t *st = ui3_state();
    if(ui3_auth_try_admin(st, st->admin_acc, st->admin_pwd)) {
        st->menu_index = 0u;
        st->scroll_px = 0u;
        st->menu_armed = 0u;
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
    st->edit_role_saved = row.role_admin;
    st->edit_role_dirty = 0u;
    st->edit_has_fp = row.has_fp;
    st->edit_has_nfc = row.has_nfc;
    st->edit_pwd[0] = '\0';
    st->edit_pwd_editing = 0u;
    st->edit_field = 0u;
    ui3_users_set_edit_target(st->edit_acc);
    ui3_nav_to(UI3_SCR_EDIT_USER, true);
}

static void cb_del_ok(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    const char *msg = NULL;
    ui3_del_result_t rc;

    (void)e;
    rc = ui3_users_try_delete_msg(st->del_acc, &msg);
    if(rc == UI3_DEL_OK) {
        ui3_show_modal_result(lv_scr_act(), "删除成功", st->del_acc, true);
        st->del_acc[0] = '\0';
    } else {
        ui3_show_modal_result(lv_scr_act(), "删除失败", msg != NULL ? msg : NULL, false);
    }
}

static void cb_save_user(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    char saved_acc[13];
    (void)e;

    (void)strncpy(saved_acc, st->add_acc, sizeof(saved_acc) - 1u);
    saved_acc[sizeof(saved_acc) - 1u] = '\0';
    if(ui3_users_try_register(st->add_acc, st->add_pwd, st->add_admin != 0u)) {
        ui3_users_prepare_add_screen(st);
        ui3_show_modal_result(lv_scr_act(), "保存成功", saved_acc, true);
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

static void cb_role_user(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    st->add_admin = 0u;
    st->add_field = 2u;
    ui3_reload_current();
}

static void cb_role_admin(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    st->add_admin = 1u;
    st->add_field = 2u;
    ui3_reload_current();
}

static void cb_add_field(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    st->add_field = idx;
    if(!ui3_inputs_live_refresh(st)) {
        ui3_reload_current();
    }
}

static void cb_edit_pwd_chg(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    st->edit_pwd_editing = 1u;
    st->edit_pwd[0] = '\0';
    st->edit_field = 1u;
    ui3_reload_current();
}

static void cb_edit_role_user(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    lv_obj_t *seg = lv_event_get_target(e);
    lv_obj_t *wrap = lv_obj_get_parent(lv_obj_get_parent(seg));

    (void)e;
    st->edit_is_admin = 0u;
    st->edit_role_dirty = 1u;
    ui3_role_picker_set(wrap, false, false);
}

static void cb_edit_role_admin(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    lv_obj_t *seg = lv_event_get_target(e);
    lv_obj_t *wrap = lv_obj_get_parent(lv_obj_get_parent(seg));

    (void)e;
    st->edit_is_admin = 1u;
    st->edit_role_dirty = 1u;
    ui3_role_picker_set(wrap, true, false);
}

static void cb_edit_back_menu(lv_event_t *e)
{
    (void)e;
    ui3_nav_back_to_menu();
}

static void cb_edit_save(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;

    if(st->edit_is_admin != st->edit_role_saved) {
        if(!ui3_users_set_role(st->edit_acc, st->edit_is_admin != 0u)) {
            ui3_show_modal_result(lv_scr_act(), "无法更改身份", "须保留至少一名管理员", false);
            return;
        }
        st->edit_role_saved = st->edit_is_admin;
        st->edit_role_dirty = 0u;
    }

    if(st->edit_pwd_editing != 0u && st->edit_pwd[0] != '\0') {
        if(!ui3_users_change_password(st->edit_acc, st->edit_pwd)) {
            ui3_show_modal_result(lv_scr_act(), "更新失败", "密码须为4-10位", false);
            return;
        }
        st->edit_pwd[0] = '\0';
        st->edit_pwd_editing = 0u;
    } else if(st->edit_pwd_editing != 0u) {
        st->edit_pwd_editing = 0u;
    }

    ui3_post_feedback_toast("已保存");
    ui3_nav_back_to_menu();
}

static void cb_edit_field(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    st->edit_field = 1u;
    if(!ui3_inputs_live_refresh(st)) {
        ui3_reload_current();
    }
}

static void cb_edit_fp_chg(lv_event_t *e)
{
    (void)e;
    ui3_nav_to(UI3_SCR_FP_MGMT, true);
}

static void cb_edit_nfc_chg(lv_event_t *e)
{
    (void)e;
    ui3_nav_to(UI3_SCR_NFC_MGMT, true);
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
    st->wifi_modal = 0u;
    st->wifi_pwd[0] = '\0';
    st->wifi_pending_ssid[0] = '\0';
    ui3_wifi_pwd_modal_close();
    ui3_wifi_refresh_list(st);
}

static void cb_wifi_scan(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    if(st->wifi_scanning != 0u || app_wifi_scan_busy() != 0u) {
        return;
    }
    if(ui3_wifi_scan_ui_active(st) != 0u) {
        return;
    }
    ui3_wifi_request_scan(st);
    ui3_wifi_refresh_list(st);
}

static void cb_wifi_connect(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    ui3_wifi_try_connect(st);
    if(st->wifi_modal != 0u && st->wifi_pending_ssid[0] != '\0') {
        ui3_show_wifi_pwd_modal(lv_scr_act(), st->wifi_pending_ssid);
        ui3_wifi_pwd_modal_refresh(st->wifi_pwd);
    }
}

static void cb_pair_regen(lv_event_t *e)
{
    ui3_state_t *st = ui3_state();
    (void)e;
    ui3_pair_regen(st);
    ui3_pair_sync(st);
    ui3_reload_current();
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
        ui3_edit_user_refresh_creds(st);
        ui3_show_modal_result(lv_scr_act(), "已移除 NFC", NULL, true);
    } else {
        ui3_show_modal_result(lv_scr_act(), "移除失败", "未绑定 NFC", false);
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
        ui3_show_modal_result(lv_scr_act(), "清除失败", "未录入指纹", false);
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
    ui3_page_head_menu(lo, "账号管理");

    st->scroll_row_h = (uint8_t)(UI3_MENU_CARD_H + 8);
    st->scroll_visible = (uint8_t)(UI3_MENU_VIEWPORT_H / st->scroll_row_h);
    if(st->scroll_visible < 1u) {
        st->scroll_visible = 1u;
    }
    st->scroll_max = (UI3_MENU_COUNT > st->scroll_visible) ?
                     (uint8_t)(UI3_MENU_COUNT - st->scroll_visible) : 0u;
    if(st->menu_index < st->scroll_px) {
        st->scroll_px = st->menu_index;
    }
    if(st->menu_index >= st->scroll_px + st->scroll_visible) {
        st->scroll_px = (uint8_t)(st->menu_index - st->scroll_visible + 1u);
    }
    if(st->scroll_px > st->scroll_max) {
        st->scroll_px = st->scroll_max;
    }

    lv_obj_t *track = ui3_scroll_viewport(lo, UI3_MENU_VIEWPORT_H);
    st->menu_track = track;
    for(uint8_t i = 0; i < UI3_MENU_COUNT; i++) {
        ui3_menu_card(track, s_menu[i].icon, s_menu[i].title,
                      i == st->menu_index, cb_menu_pick, (void *)(uintptr_t)i);
    }
    ui3_scroll_apply(st, track, false);
    ui3_gesture_bind(st, track);
    ui3_footer_dock(lo, "进入", true, cb_menu_enter, cb_menu_back_home, st, true);
}

static void build_add_user(ui3_state_t *st, ui3_layout_t *lo)
{
    char pwd_mask[32];
    uint8_t want_row;

    ui3_pwd_mask_fill(pwd_mask, sizeof(pwd_mask), st->add_pwd);
    ui3_topbar(lo, st);
    ui3_page_head(lo, "新用户", NULL, true);

    st->scroll_row_h = UI3_ADD_SCROLL_STEP;
    st->scroll_visible = 2u;
    st->scroll_max = UI3_ADD_SCROLL_MAX;
    want_row = st->add_field;
    if(want_row > st->scroll_max) {
        want_row = st->scroll_max;
    }
    if(want_row < st->scroll_px) {
        st->scroll_px = want_row;
    } else if(want_row >= st->scroll_px + st->scroll_visible) {
        st->scroll_px = (uint8_t)(want_row - st->scroll_visible + 1u);
    }
    if(st->scroll_px > st->scroll_max) {
        st->scroll_px = st->scroll_max;
    }

    lv_obj_t *track = ui3_scroll_viewport(lo, UI3_ADD_VIEWPORT_H);
    ui3_inputs_track(st, 0u, ui3_field_block(track, "学号 / 工号", st->add_acc, st->add_field == 0,
                                               false, cb_add_field, (void *)(uintptr_t)0u));
    ui3_inputs_track(st, 1u, ui3_field_block(track, "密码", pwd_mask, st->add_field == 1,
                                               false, cb_add_field, (void *)(uintptr_t)1u));
    ui3_role_picker(track, st->add_admin != 0, st->add_field == 2,
                    cb_role_user, cb_role_admin, st);
    ui3_bio_block(track, cb_fp_enroll, cb_nfc_enroll, st,
                  g_fp_pending_page_valid, g_nfc_pending_uid_valid);

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

    st->scroll_row_h = (uint8_t)((UI3_LIST_ROW_H - 6) + 8);
    st->scroll_visible = (uint8_t)(UI3_LIST_VIEWPORT_H / st->scroll_row_h);
    if(st->scroll_visible < 1u) {
        st->scroll_visible = 1u;
    }
    if(count > st->scroll_visible) {
        st->scroll_max = (uint8_t)(count - st->scroll_visible);
    } else {
        st->scroll_max = 0u;
    }
    if(st->scroll_px > st->scroll_max) {
        st->scroll_px = st->scroll_max;
    }
    lv_obj_t *track = ui3_scroll_viewport(lo, UI3_LIST_VIEWPORT_H);
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

static void wifi_update_scroll_params(ui3_state_t *st, uint8_t count)
{
    st->scroll_row_h = (uint8_t)((UI3_LIST_ROW_H - 6) + 8);
    st->scroll_visible = (uint8_t)(UI3_WIFI_VIEWPORT_H / st->scroll_row_h);
    if(st->scroll_visible < 1u) {
        st->scroll_visible = 1u;
    }
    if(count > st->scroll_visible) {
        st->scroll_max = (uint8_t)(count - st->scroll_visible);
    } else {
        st->scroll_max = 0u;
    }
    if(st->wifi_index >= count && count > 0u) {
        st->wifi_index = (uint8_t)(count - 1u);
    }
    if(st->scroll_px > st->scroll_max) {
        st->scroll_px = st->scroll_max;
    }
}

static void wifi_populate_track(ui3_state_t *st, lv_obj_t *track)
{
    uint8_t count = ui3_wifi_row_count();
    uint8_t i;
    ui3_wifi_row_t row;

    lv_obj_clean(track);
    if(count == 0u) {
        lv_obj_t *hint = lv_label_create(track);
        if(ui3_wifi_scan_ui_active(st) != 0u) {
            lv_label_set_text(hint, "正在搜索附近热点…");
        } else {
            lv_label_set_text(hint, "点击刷新列表搜索热点");
        }
        lv_obj_add_style(hint, ui3_style_label(), 0);
        lv_obj_set_style_text_color(hint, UI3_COL_INK_FAINT, 0);
    } else {
        for(i = 0u; i < count; i++) {
            if(ui3_wifi_row_at(i, &row)) {
                ui3_wifi_item(track, row.ssid, ui3_wifi_rssi_level(row.rssi), row.connected != 0u,
                              i == st->wifi_index, cb_wifi_pick, (void *)(uintptr_t)i);
            }
        }
    }
    ui3_scroll_apply(st, track, false);
}

void ui3_wifi_refresh_list(ui3_state_t *st)
{
    uint8_t count;

    if(st == NULL || st->scr != UI3_SCR_WIFI) {
        return;
    }
    ui3_wifi_refresh_scan_lbl(st);
    if(st->wifi_list_track == NULL || !lv_obj_is_valid(st->wifi_list_track)) {
        return;
    }
    count = ui3_wifi_row_count();
    wifi_update_scroll_params(st, count);
    wifi_populate_track(st, st->wifi_list_track);
}

void ui3_wifi_refresh_scan_lbl(ui3_state_t *st)
{
    if(st == NULL || st->scr != UI3_SCR_WIFI) {
        return;
    }
    if(st->wifi_scan_lbl != NULL && lv_obj_is_valid(st->wifi_scan_lbl)) {
        if(cloud_aliyun_at_wifi_link_ready() != 0u && app_wifi_scan_busy() == 0u &&
           cloud_aliyun_at_cwlap_scan_async_active() == 0u) {
            st->wifi_scanning = 0u;
            lv_label_set_text(st->wifi_scan_lbl, "刷新列表");
        } else {
            lv_label_set_text(st->wifi_scan_lbl,
                              ui3_wifi_scan_ui_active(st) != 0u ? "扫描中…" : "刷新列表");
        }
    }
}

static void build_wifi(ui3_state_t *st, ui3_layout_t *lo)
{
    uint8_t count;

    ui3_topbar(lo, st);
    ui3_page_head(lo, "无线网络", NULL, true);

    st->wifi_scan_lbl = ui3_wifi_scan_link(lo->root, ui3_wifi_scan_ui_active(st) != 0u, cb_wifi_scan, st);
    lv_obj_align(st->wifi_scan_lbl, LV_ALIGN_TOP_RIGHT, -12, 36);

    count = ui3_wifi_row_count();
    wifi_update_scroll_params(st, count);

    st->wifi_list_track = ui3_scroll_viewport(lo, UI3_WIFI_VIEWPORT_H);
    lv_obj_align(lv_obj_get_parent(st->wifi_list_track), LV_ALIGN_TOP_MID, 0, 68);
    wifi_populate_track(st, st->wifi_list_track);
    ui3_gesture_bind(st, st->wifi_list_track);
    ui3_footer_dock(lo, "连接", true, cb_wifi_connect, cb_nav_back, st, true);
    if(st->wifi_modal != 0u && st->wifi_pending_ssid[0] != '\0') {
        ui3_show_wifi_pwd_modal(lo->root, st->wifi_pending_ssid);
        ui3_wifi_pwd_modal_refresh(st->wifi_pwd);
    }
}

static void build_pair(ui3_state_t *st, ui3_layout_t *lo)
{
    ui3_topbar(lo, st);
    ui3_page_head(lo, "设备配对", NULL, true);
    ui3_pair_sync(st);
    ui3_pair_zone(lo, st, ui3_pair_status_line());
    st->scroll_max = 0;
    ui3_gesture_bind(st, NULL);
    ui3_footer_pair(lo, cb_nav_back, cb_pair_regen, st);
}

static void build_edit_user(ui3_state_t *st, ui3_layout_t *lo)
{
    ui3_user_row_t row;
    char pwd_mask[32];
    char cur_pwd[16];
    const uint8_t edit_rows = 5u;

    st->edit_lbl_fp = NULL;
    st->edit_lbl_nfc = NULL;
    st->edit_lbl_pwd = NULL;

    if(st->edit_acc[0] != '\0' && ui3_users_lookup(st->edit_acc, &row)) {
        if(st->edit_role_dirty == 0u) {
            st->edit_is_admin = row.role_admin;
            st->edit_role_saved = row.role_admin;
        }
        st->edit_has_fp = row.has_fp;
        st->edit_has_nfc = row.has_nfc;
    }
    ui3_users_get_password(st->edit_acc, cur_pwd, sizeof(cur_pwd));

    ui3_topbar(lo, st);
    ui3_page_head(lo, st->edit_acc[0] ? st->edit_acc : "用户", "更改密码与凭证", true);

    st->scroll_row_h = (uint8_t)((UI3_LIST_ROW_H - 6) + 8);
    st->scroll_visible = (uint8_t)(UI3_LIST_VIEWPORT_H / st->scroll_row_h);
    if(st->scroll_visible < 1u) {
        st->scroll_visible = 1u;
    }
    if(edit_rows > st->scroll_visible) {
        st->scroll_max = (uint8_t)(edit_rows - st->scroll_visible);
    } else {
        st->scroll_max = 0u;
    }
    if(st->scroll_px > st->scroll_max) {
        st->scroll_px = st->scroll_max;
    }

    lv_obj_t *track = ui3_scroll_viewport(lo, UI3_LIST_VIEWPORT_H);
    ui3_detail_card(track, "账号", st->edit_acc, false, NULL, NULL, NULL);
    if(st->edit_pwd_editing != 0u) {
        ui3_pwd_mask_fill(pwd_mask, sizeof(pwd_mask), st->edit_pwd);
        ui3_inputs_track(st, 0u, ui3_field_block(track, "新密码", pwd_mask, st->edit_field == 1u,
                                                   false, cb_edit_field, st));
    } else {
        ui3_inputs_reset(st);
        ui3_detail_card(track, "密码", cur_pwd[0] ? cur_pwd : "未设置", true, cb_edit_pwd_chg, st,
                        &st->edit_lbl_pwd);
    }
    ui3_role_picker(track, st->edit_is_admin != 0u, false,
                    cb_edit_role_user, cb_edit_role_admin, st);
    ui3_detail_card(track, "指纹", st->edit_has_fp ? "已录入" : "未录入", true, cb_edit_fp_chg, st,
                    &st->edit_lbl_fp);
    ui3_detail_card(track, "NFC 卡", st->edit_has_nfc ? "已绑定" : "未绑定", true, cb_edit_nfc_chg, st,
                    &st->edit_lbl_nfc);
    ui3_scroll_apply(st, track, false);
    ui3_gesture_bind(st, track);
    ui3_footer_dock(lo, "保存", true, cb_edit_save, cb_edit_back_menu, st, true);
}

void ui3_edit_user_refresh_creds(ui3_state_t *st)
{
    ui3_user_row_t row;
    char cur_pwd[16];

    if(st == NULL || st->scr != UI3_SCR_EDIT_USER) {
        return;
    }
    if(st->edit_acc[0] != '\0' && ui3_users_lookup(st->edit_acc, &row)) {
        st->edit_has_fp = row.has_fp;
        st->edit_has_nfc = row.has_nfc;
    }
    if(st->edit_lbl_fp != NULL && lv_obj_is_valid(st->edit_lbl_fp)) {
        lv_label_set_text(st->edit_lbl_fp, st->edit_has_fp ? "已录入" : "未录入");
    }
    if(st->edit_lbl_nfc != NULL && lv_obj_is_valid(st->edit_lbl_nfc)) {
        lv_label_set_text(st->edit_lbl_nfc, st->edit_has_nfc ? "已绑定" : "未绑定");
    }
    if(st->edit_pwd_editing == 0u && st->edit_lbl_pwd != NULL && lv_obj_is_valid(st->edit_lbl_pwd) &&
       ui3_users_get_password(st->edit_acc, cur_pwd, sizeof(cur_pwd))) {
        lv_label_set_text(st->edit_lbl_pwd, cur_pwd[0] ? cur_pwd : "未设置");
    }
}

static void build_unlock(ui3_state_t *st, ui3_layout_t *lo)
{
    char pwd_mask[32];
    char lock_hint[40];
    bool locked = unlock_is_locked(st);
    const char *err = (st->unlock_show_err != 0u && !locked) ? "密码错误" : NULL;

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

    st->lbl_top_time = NULL;
    st->lbl_clock_big = NULL;
    st->lbl_date = NULL;
    st->cloud_badge = NULL;
    st->cloud_dot = NULL;
    st->cloud_label = NULL;
    st->btn_home_menu = NULL;
    st->btn_home_unlock = NULL;
    st->footer_btn_back = NULL;
    st->footer_btn_ok = NULL;
    st->wifi_list_track = NULL;
    st->wifi_scan_lbl = NULL;
    st->edit_lbl_fp = NULL;
    st->edit_lbl_nfc = NULL;
    st->edit_lbl_pwd = NULL;
    st->footer_sel = UI3_FOOTER_SEL_NONE;
    if(st->scr != UI3_SCR_MENU) {
        st->menu_armed = 0u;
    }
    st->menu_track = NULL;
    st->active_caret = NULL;
    ui3_inputs_reset(st);

    lo.root = ui3_screen_create();
    if(lo.root == NULL) {
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
    return lo.root;
}
