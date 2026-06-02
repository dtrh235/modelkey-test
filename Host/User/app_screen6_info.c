#include "app_screen6_info.h"

#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "cloud_ota_service.h"
#include "ui_screen8_utils.h"
#include "app_user_ops.h"
#include "user_auth_portable.h"

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20);

extern lv_ui guider_ui;
extern uint8_t g_screen6_focus;
extern lv_obj_t *g_screen6_acc_val_label;
extern lv_obj_t *g_screen6_pwd_val_label;

/* 这两个 label 是动态 child of screen_6_cont_1。当 screen_6 被卸载（再次进入 screen_6
 * 或从 screen_6 退出后再回来）时，cont_1 连同 child 一起被 LVGL 释放——但静态指针
 * 仍指向已释放/被复用的内存，下一次 lv_label_set_text 会写到错误 widget，造成屏幕崩花。
 * 解法：给 label 挂 LV_EVENT_DELETE 回调，被销毁时主动把静态指针清空。 */
static void screen6_acc_val_label_deleted_cb(lv_event_t *e)
{
    (void)e;
    g_screen6_acc_val_label = NULL;
}

static void screen6_pwd_val_label_deleted_cb(lv_event_t *e)
{
    (void)e;
    g_screen6_pwd_val_label = NULL;
}

extern char g_screen5_found_acc[13];
extern user_cred_t g_users[APP_USER_MAX];
extern uint8_t g_default_admin_deleted;
extern uint8_t g_default_admin_is_admin_role;
extern char g_default_admin_account[13];
extern char g_default_admin_password[11];

static void screen6_get_pwd_for_found_acc(char *out_pwd, size_t out_sz)
{
    int idx;
    if(out_pwd == NULL || out_sz == 0u) return;

    idx = users_find_index_by_acc(g_screen5_found_acc);
    if(idx >= 0) {
        strncpy(out_pwd, g_users[idx].pwd, out_sz - 1u);
        out_pwd[out_sz - 1u] = '\0';
        return;
    }

    if(!g_default_admin_deleted && strcmp(g_screen5_found_acc, g_default_admin_account) == 0) {
        strncpy(out_pwd, g_default_admin_password, out_sz - 1u);
        out_pwd[out_sz - 1u] = '\0';
        return;
    }

    out_pwd[0] = '\0';
}

void screen6_set_focus(uint8_t focus)
{
    ui_screen6_set_focus_style(focus, &g_screen6_focus,
                               NULL, guider_ui.screen_6_btn_4,
                               guider_ui.screen_6_btn_6, guider_ui.screen_6_btn_7,
                               guider_ui.screen_6_ddlist_1);
}

void screen6_update_labels_from_found(void)
{
    char pwd_buf[11];
    lv_obj_t *parent;

    if(!lv_obj_is_valid(guider_ui.screen_6_cont_1)) return;
    parent = guider_ui.screen_6_cont_1;

    if(!lv_obj_is_valid(guider_ui.screen_6_label_2) || !lv_obj_is_valid(guider_ui.screen_6_label_3)) return;

    screen6_get_pwd_for_found_acc(pwd_buf, sizeof(pwd_buf));

    lv_label_set_text(guider_ui.screen_6_label_2, "账号：");
    lv_label_set_text(guider_ui.screen_6_label_3, "密码：");

    /* 双保险：除了 LV_EVENT_DELETE 回调把指针清 NULL 之外，
     * 这里再校验一次"指针存在 && 仍是当前 cont_1 的 child"，
     * 避免极端情况下指针看似有效但其实指向被复用的内存。 */
    if(g_screen6_acc_val_label != NULL) {
        if(!lv_obj_is_valid(g_screen6_acc_val_label) ||
           lv_obj_get_parent(g_screen6_acc_val_label) != parent) {
            g_screen6_acc_val_label = NULL;
        }
    }
    if(g_screen6_pwd_val_label != NULL) {
        if(!lv_obj_is_valid(g_screen6_pwd_val_label) ||
           lv_obj_get_parent(g_screen6_pwd_val_label) != parent) {
            g_screen6_pwd_val_label = NULL;
        }
    }

    if(g_screen6_acc_val_label == NULL) {
        g_screen6_acc_val_label = lv_label_create(parent);
        lv_obj_set_style_text_font(g_screen6_acc_val_label, &lv_font_SourceHanSerifSC_Regular_20,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(g_screen6_acc_val_label, lv_color_hex(0x000000),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(g_screen6_acc_val_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(g_screen6_acc_val_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(g_screen6_acc_val_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(g_screen6_acc_val_label, screen6_acc_val_label_deleted_cb, LV_EVENT_DELETE, NULL);
    }
    if(g_screen6_pwd_val_label == NULL) {
        g_screen6_pwd_val_label = lv_label_create(parent);
        lv_obj_set_style_text_font(g_screen6_pwd_val_label, &lv_font_SourceHanSerifSC_Regular_20,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(g_screen6_pwd_val_label, lv_color_hex(0x000000),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(g_screen6_pwd_val_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(g_screen6_pwd_val_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(g_screen6_pwd_val_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(g_screen6_pwd_val_label, screen6_pwd_val_label_deleted_cb, LV_EVENT_DELETE, NULL);
    }

    lv_label_set_text(g_screen6_acc_val_label, g_screen5_found_acc);
    lv_obj_set_pos(g_screen6_acc_val_label, 85, 71);
    lv_obj_set_size(g_screen6_acc_val_label, 90, 24);

    lv_label_set_text(g_screen6_pwd_val_label, pwd_buf);
    lv_obj_set_pos(g_screen6_pwd_val_label, 70, 111);
    lv_obj_set_size(g_screen6_pwd_val_label, 100, 24);
}
