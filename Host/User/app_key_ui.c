#include "app_key_ui.h"

#include <stdbool.h>
#include <string.h>

#include "lvgl.h"
#include "gui_guider.h"
#include "user_auth_portable.h"
#include "app_screen.h"
#include "app_user_ops.h"
#include "app_screen6_types.h"
#include "app_nav_entries.h"
#include "app_ui_nav.h"
#include "app_screen5_flow.h"
#include "app_screen_wifi_flow.h"
#include "app_wifi_diag.h"
#include "app_ui_nav.h"
#include "ui_menu_popup_utils.h"
#include "app_screen3_menu.h"
#include "app_screen_pair_flow.h"
#include "app_screen4_table.h"
#include "app_screen6_dialog_flow.h"
#include "app_screen6_commit.h"
#include "app_screen6_info.h"
#include "app_screen7_flow.h"
#include "app_screen8_focus.h"
#include "app_screen8_flow.h"
#include "app_screen8_enroll.h"
#include "app_screen8_popup.h"
#include "app_screen9_10_flow.h"
#include "app_screen2_login.h"
#include "app_screen_auth.h"
#include "app_screen1_unlock.h"
#include "app_unlock_event.h"
#include "app_state.h"
#include "app_home_nav_btns.h"
#include "cloud_ota_service.h"

#include "./BSP/KEY/key.h"

#define APP_SCREEN8_N_FOCUS 6u

extern uint32_t HAL_GetTick(void);

void app_key_ui_dispatch(KeyValue_t key)
{
    if(key == KEY_NONE) return;

    if(g_app_scr == APP_SCR_HOME) {
        if(key == KEY_RIGHT) {
            lock_btn_set_selected(1);
        } else if(key == KEY_LEFT) {
            menu_btn_set_selected(1);
        } else if(key == KEY_OK && g_lock_btn_selected) {
            enter_screen_1();
        } else if(key == KEY_OK && g_menu_btn_selected) {
            enter_screen_2();
        }
        return;
    }

    if(g_app_scr == APP_SCR_6 && g_screen6_dlg != SCREEN6_DLG_NONE) {
        screen6_dlg_handle_key(key);
        return;
    }

    /* 密码自锁倒计时期间禁止离开开锁界面 */
    if(g_app_scr == APP_SCR_1 && screen1_is_lockout_active() != 0u) {
        return;
    }

    /* ESC：screen_3 直接回首页，其余页面默认返回上一级。
     * 但 screen_6/8/9/10 有录入弹窗时，优先交给对应页面分支处理，避免切屏与弹窗状态机冲突。 */
    if(key == KEY_ESC) {
        if((g_app_scr == APP_SCR_6 || g_app_scr == APP_SCR_8 || g_app_scr == APP_SCR_9 || g_app_scr == APP_SCR_10) &&
           (g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup))) {
            /* handled in APP_SCR_8/APP_SCR_9 branches below */
        } else
        if(g_app_scr == APP_SCR_3 && screen_pair_is_open()) {
            screen_pair_exit_to_menu();
            screen3_set_menu_selected(g_screen3_pending_index);
            return;
        }
        if(g_app_scr == APP_SCR_3) {
            app_ui_nav_begin((uint8_t)APP_SCR_3);
            ui_load_scr_animation(&guider_ui, &guider_ui.screen, guider_ui.screen_del, &guider_ui.screen_3_del,
                                  setup_scr_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false, true);
            g_app_scr = APP_SCR_HOME;
            app_ui_nav_end((uint8_t)APP_SCR_HOME);
            g_screen3_menu_index = 0u;
            lock_btn_set_selected(0);
            menu_btn_set_selected(0);
            return;
        }
        else {
            if(g_app_scr == APP_SCR_11) {
                if(screen_wifi_popup_is_active()) {
                    screen_wifi_popup_cancel();
                } else {
                    screen_wifi_exit_to_menu();
                }
                return;
            }
            go_back_prev_screen();
            return;
        }
    }

    if(g_app_scr == APP_SCR_1) {
        if(g_screen1_unlock_popup != NULL && lv_obj_is_valid(g_screen1_unlock_popup)) {
            return;
        }
        if(screen1_is_lockout_active() != 0u) {
            return;
        }
        if(key == KEY_OK) {
            (void)screen1_try_password_unlock();
        } else if(key == KEY_UP && g_screen1_field_index > 0u) {
            screen1_set_field_selected(g_screen1_field_index - 1u);
        } else if(key == KEY_DOWN && g_screen1_field_index < 1u) {
            screen1_set_field_selected(g_screen1_field_index + 1u);
        } else {
            screen1_handle_input_key(key);
        }
    } else if(g_app_scr == APP_SCR_2) {
        if(key == KEY_OK) {
            try_screen2_admin_login();
        } else if(key == KEY_UP && g_screen2_field_index > 0u) {
            screen2_set_field_selected(g_screen2_field_index - 1u);
        } else if(key == KEY_DOWN && g_screen2_field_index < 1u) {
            screen2_set_field_selected(g_screen2_field_index + 1u);
        } else {
            screen2_handle_input_key(key);
        }
    } else if(g_app_scr == APP_SCR_3 && screen_pair_is_open()) {
        if(key == KEY_OK) {
            screen_pair_on_regen();
        }
    } else if(g_app_scr == APP_SCR_3) {
        if(key == KEY_UP) {
            if(g_screen3_menu_index == 0u) {
                screen3_set_menu_selected(SCREEN3_MENU_ITEM_COUNT - 1u);
            } else {
                screen3_set_menu_selected(g_screen3_menu_index - 1u);
            }
        } else if(key == KEY_DOWN) {
            if(g_screen3_menu_index >= (SCREEN3_MENU_ITEM_COUNT - 1u)) {
                screen3_set_menu_selected(0u);
            } else {
                screen3_set_menu_selected(g_screen3_menu_index + 1u);
            }
        } else if(key == KEY_OK && g_screen3_menu_index == 0u) {
            enter_screen_8();
        } else if(key == KEY_OK && g_screen3_menu_index == 1u) {
            enter_screen_7();
        } else if(key == KEY_OK && g_screen3_menu_index == 2u) {
            enter_screen_5();
        } else if(key == KEY_OK && g_screen3_menu_index == 3u) {
            enter_screen_4();
        } else if(key == KEY_OK && g_screen3_menu_index == 4u) {
            enter_screen_wifi();
        } else if(key == KEY_OK && g_screen3_menu_index == 5u) {
            enter_screen_pair();
        }
    } else if(g_app_scr == APP_SCR_5) {
        if(key == KEY_OK) {
            if(screen5_try_lookup_acc()) {
                screen5_hide_error_label();
                enter_screen_6();
            } else {
                screen5_show_error_label();
            }
        } else {
            screen5_handle_input_key(key);
        }
    } else if(g_app_scr == APP_SCR_11) {
        if(screen_wifi_popup_is_active()) {
            if(key == KEY_OK) {
                screen_wifi_popup_confirm();
            } else {
                screen_wifi_handle_popup_input(key);
            }
        } else {
            if(key == KEY_OK) {
                screen_wifi_list_ok();
            } else if(key == KEY_UP) {
                screen_wifi_list_move(-1);
            } else if(key == KEY_DOWN) {
                screen_wifi_list_move(1);
            }
        }
        screen_wifi_gui_wake();
    } else if(g_app_scr == APP_SCR_6) {
        if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
            if(key == KEY_OK || key == KEY_ESC) {
                if(g_nfc_enroll_state == 1u) {
                    if(key == KEY_ESC) {
                        g_nfc_enroll_state = 2u;
                        g_nfc_enroll_result = 0u;
                        screen8_show_enroll_popup("NFC FAIL");
                    }
                    return;
                }
                if(g_screen8_fp_enroll_state == 1u) {
                    if(key == KEY_ESC) {
                        g_screen8_fp_enroll_state = 2u;
                        g_screen8_fp_enroll_result = 0u;
                        screen8_show_enroll_popup("Fingerprint FAIL");
                    }
                    return;
                }
                if(g_nfc_enroll_state == 2u) g_nfc_enroll_state = 0u;
                if(g_screen8_fp_enroll_state == 2u) g_screen8_fp_enroll_state = 0u;
                screen8_popup_close_only();
            }
            return;
        }

        lv_obj_t *dd = guider_ui.screen_6_ddlist_1;
        if(!lv_obj_is_valid(dd)) return;

        const bool dd_open = lv_dropdown_is_open(dd) ? true : false;

        if(dd_open) {
            /* 身份下拉框打开时：UP/DOWN选择，OK确认保存 */
            if(g_screen6_focus == 1u) {
                if(key == KEY_UP) {
                    char k = LV_KEY_UP;
                    lv_event_send(dd, LV_EVENT_KEY, &k);
                    return;
                } else if(key == KEY_DOWN) {
                    char k = LV_KEY_DOWN;
                    lv_event_send(dd, LV_EVENT_KEY, &k);
                    return;
                } else if(key == KEY_OK) {
                    /* 键盘 UP/DOWN 只更新 sel_opt_id；get_selected_str 仍读 sel_opt_id_orig，须用 get_selected 并 set_selected 同步显示 */
                    uint16_t sel = lv_dropdown_get_selected(dd);
                    screen6_save_identity(sel == 1u);
                    if(lv_dropdown_get_option_cnt(dd) > 1u) {
                        lv_dropdown_set_selected(dd, sel == 0u ? 1u : 0u);
                    }
                    lv_dropdown_set_selected(dd, sel);
                    lv_dropdown_close(dd);
                    screen6_set_focus(1u);
                    return;
                } else if(key == KEY_ESC) {
                    char k = LV_KEY_ESC;
                    lv_event_send(dd, LV_EVENT_KEY, &k);
                    screen6_set_focus(1u);
                    return;
                }
            }
            /* 下拉框打开但当前不在身份选定上：不做焦点切换 */
            return;
        }

        /* 下拉框关闭：UP/DOWN按界面从上到下顺序切换（不选中 ESC，且循环回绕） */
        if(key == KEY_UP) {
            if(g_screen6_focus == 0u) g_screen6_focus = 3u;
            else g_screen6_focus--;
            screen6_set_focus(g_screen6_focus);
        } else if(key == KEY_DOWN) {
            if(g_screen6_focus == 3u) g_screen6_focus = 0u;
            else g_screen6_focus++;
            screen6_set_focus(g_screen6_focus);
        } else if(key == KEY_OK) {
            if(g_screen6_focus == 0u) {
                screen6_open_edit_password_dialog();
            } else if(g_screen6_focus == 1u) {
                lv_dropdown_open(dd);
            } else if(g_screen6_focus == 2u) {
                enter_screen_10();
            } else if(g_screen6_focus == 3u) {
                enter_screen_9();
            }
        }
    } else if(g_app_scr == APP_SCR_7) {
        if(g_screen7_msgbox_state == 1u) {
            if(key == KEY_RIGHT) {
                screen7_set_msgbox1_choice(0u);
            } else if(key == KEY_LEFT) {
                screen7_set_msgbox1_choice(1u);
            } else if(key == KEY_OK) {
                if(g_screen7_choice_yes) {
                    const char *acc = lv_textarea_get_text(guider_ui.screen_7_ta_1);
                    int idx = users_find_index_by_acc(acc);
                    uint8_t was_admin = 0u;
                    if(idx >= 0 && g_users[idx].is_admin != 0u) {
                        was_admin = 1u;
                    } else if(!g_default_admin_deleted &&
                              strcmp(acc, g_default_admin_account) == 0 &&
                              g_default_admin_is_admin_role != 0u) {
                        was_admin = 1u;
                    }
                    if(users_try_delete_by_acc(acc)) {
                        screen7_show_delete_result(1u, was_admin);
                    } else {
                        screen7_show_delete_result(0u, was_admin);
                    }
                } else {
                    screen7_show_delete_result(0u, 0u);
                }
            }
        } else if(g_screen7_msgbox_state == 2u || g_screen7_msgbox_state == 3u) {
            if(key == KEY_OK || key == KEY_ESC) {
                screen7_dismiss_result_popup();
            }
        } else {
            if(key == KEY_OK) {
                screen7_show_msgbox1();
            } else {
                screen7_handle_input_key(key);
            }
        }
    } else if(g_app_scr == APP_SCR_8) {
        if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
            if(key == KEY_OK || key == KEY_ESC) {
                if(g_nfc_enroll_state == 1u) {
                    /* ESC on "NFC Working..." aborts enroll and shows fail result. */
                    if(key == KEY_ESC) {
                        g_nfc_enroll_state = 2u;
                        g_nfc_enroll_result = 0u;
                        screen8_show_enroll_popup("NFC FAIL");
                    }
                    return; /* keep blocking close while running */
                }
                if(g_screen8_fp_enroll_state == 1u) {
                    if(key == KEY_ESC) {
                        g_screen8_fp_enroll_state = 2u;
                        g_screen8_fp_enroll_result = 0u;
                        screen8_show_enroll_popup("Fingerprint FAIL");
                    }
                    return; /* 录入进行中，禁止提前关闭弹窗 */
                }
                if(g_nfc_enroll_state == 2u) {
                    g_nfc_enroll_state = 0u;
                }
                if(g_screen8_fp_enroll_state == 2u) {
                    g_screen8_fp_enroll_state = 0u;
                }
                if(screen8_add_user_success_popup_active() != 0u) {
                    screen8_add_user_success_popup_clear();
                    screen8_popup_close_and_back();
                } else {
                    screen8_popup_close_only();
                }
            }
            return;
        }
        if(key == KEY_UP) {
            if(g_screen8_focus == 0u) {
                screen8_set_focus(APP_SCREEN8_N_FOCUS - 1u);
            } else {
                screen8_set_focus(g_screen8_focus - 1u);
            }
        } else if(key == KEY_DOWN) {
            if(g_screen8_focus >= APP_SCREEN8_N_FOCUS - 1u) {
                screen8_set_focus(0u);
            } else {
                screen8_set_focus(g_screen8_focus + 1u);
            }
        } else if(key == KEY_OK) {
            if(g_screen8_focus == 2u) {
                if(lv_obj_has_state(guider_ui.screen_8_cb_1, LV_STATE_CHECKED)) {
                    lv_obj_clear_state(guider_ui.screen_8_cb_1, LV_STATE_CHECKED);
                } else {
                    lv_obj_add_state(guider_ui.screen_8_cb_1, LV_STATE_CHECKED);
                }
            } else if(g_screen8_focus == 3u) {
                if(screen8_has_valid_acc_pwd_input()) {
                    screen8_show_fp_enroll_popup();
                } else {
                    screen8_show_enroll_popup("Need Account/PWD");
                }
            } else if(g_screen8_focus == 4u) {
                if(screen8_has_valid_acc_pwd_input()) {
                    g_nfc_op = NFC_OP_ENROLL_SCREEN8;
                    screen8_show_nfc_enroll_popup();
                } else {
                    screen8_show_enroll_popup("Need Account/PWD");
                }
            } else if(g_screen8_focus == 5u) {
                screen8_attempt_commit();
            }
            /* focus 0/1：OK 不提交；focus 3/4：录入；focus 5：提交 */
        } else if(g_screen8_focus <= 1u) {
            screen8_handle_ta_key(key);
        }
    } else if(g_app_scr == APP_SCR_9) {
        if(g_screen9_msgbox_state == 1u) {
            if(key == KEY_RIGHT) {
                screen9_set_msgbox_choice(0u);
            } else if(key == KEY_LEFT) {
                screen9_set_msgbox_choice(1u);
            } else if(key == KEY_OK) {
                if(g_screen9_choice_yes) {
                    bool can_delete = false;
                    int idx = users_find_index_by_acc(g_screen5_found_acc);
                    if(!g_default_admin_deleted && strcmp(g_screen5_found_acc, g_default_admin_account) == 0) {
                        can_delete = g_default_admin_has_nfc ? true : false;
                    } else if(idx >= 0 && g_users[idx].has_nfc) {
                        can_delete = true;
                    }
                    if(can_delete) {
                        users_clear_nfc_by_acc(g_screen5_found_acc);
                        cloud_ota_service_report_event(CLOUD_EVT_NFC_DELETE_OK, g_screen5_found_acc);
                        screen9_show_delete_result_popup(1u);
                    } else {
                        cloud_ota_service_report_event(CLOUD_EVT_NFC_DELETE_FAIL, g_screen5_found_acc);
                        screen9_show_delete_result_popup(0u);
                    }
                } else {
                    screen9_hide_all_msgbox();
                }
            }
        } else if(g_screen9_msgbox_state == 2u || g_screen9_msgbox_state == 3u) {
            if(key == KEY_OK || key == KEY_ESC) {
                screen9_hide_all_msgbox();
            }
        } else if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
            if(key == KEY_OK || key == KEY_ESC) {
                if(g_nfc_enroll_state == 1u) {
                    /* ESC on "NFC Working..." aborts enroll and shows fail result. */
                    if(key == KEY_ESC) {
                        g_nfc_enroll_state = 2u;
                        g_nfc_enroll_result = 0u;
                        screen8_show_enroll_popup("NFC FAIL");
                    }
                    return; /* keep blocking close while running */
                }
                if(g_screen8_fp_enroll_state == 1u) {
                    if(key == KEY_ESC) {
                        g_screen8_fp_enroll_state = 2u;
                        g_screen8_fp_enroll_result = 0u;
                        screen8_show_enroll_popup("Fingerprint FAIL");
                    }
                    return; /* 录入进行中，禁止提前关闭弹窗 */
                }
                if(g_nfc_enroll_state == 2u) g_nfc_enroll_state = 0u;
                if(g_screen8_fp_enroll_state == 2u) g_screen8_fp_enroll_state = 0u;
                screen8_popup_close_only();
                g_nfc_op = NFC_OP_NONE;
            }
            return;
        } else if(key == KEY_UP) {
            if(g_screen9_focus == 0u) g_screen9_focus = 1u;
            else g_screen9_focus--;
            screen9_set_focus(g_screen9_focus);
        } else if(key == KEY_DOWN) {
            if(g_screen9_focus == 1u) g_screen9_focus = 0u;
            else g_screen9_focus++;
            screen9_set_focus(g_screen9_focus);
        } else if(key == KEY_OK) {
            if(g_screen9_focus == 0u) {
                screen9_start_nfc_replace();
            } else {
                screen9_show_delete_confirm_popup();
            }
        }
    } else if(g_app_scr == APP_SCR_10) {
        if(g_screen9_msgbox_state == 1u) {
            if(key == KEY_RIGHT) {
                screen9_set_msgbox_choice(0u);
            } else if(key == KEY_LEFT) {
                screen9_set_msgbox_choice(1u);
            } else if(key == KEY_OK) {
                if(g_screen9_choice_yes) {
                    uint8_t ok = screen10_try_delete_fp();
                    screen10_show_delete_result_popup(ok);
                } else {
                    screen9_hide_all_msgbox();
                }
            }
            return;
        }
        if(g_screen9_msgbox_state == 2u || g_screen9_msgbox_state == 3u) {
            if(key == KEY_OK || key == KEY_ESC) {
                screen9_hide_all_msgbox();
            }
            return;
        }

        if(g_screen8_popup != NULL && lv_obj_is_valid(g_screen8_popup)) {
            if(key == KEY_OK || key == KEY_ESC) {
                if(g_screen8_fp_enroll_state == 1u) {
                    if(key == KEY_ESC) {
                        g_screen8_fp_enroll_state = 2u;
                        g_screen8_fp_enroll_result = 0u;
                        screen8_show_enroll_popup("Fingerprint FAIL");
                    }
                    return;
                }
                if(g_screen8_fp_enroll_state == 2u) {
                    g_screen8_fp_enroll_state = 0u;
                }
                screen8_popup_close_only();
            }
            return;
        }

        if(key == KEY_UP) {
            if(g_screen10_focus == 0u) g_screen10_focus = 2u;
            else g_screen10_focus--;
            screen10_set_focus(g_screen10_focus);
        } else if(key == KEY_DOWN) {
            if(g_screen10_focus >= 2u) g_screen10_focus = 0u;
            else g_screen10_focus++;
            screen10_set_focus(g_screen10_focus);
        } else if(key == KEY_OK) {
            if(g_screen10_focus == 0u) {
                screen8_show_fp_enroll_popup();
            } else if(g_screen10_focus == 1u) {
                screen10_show_delete_confirm_popup();
            } else {
                go_back_prev_screen();
            }
        }
    }

    if(g_app_scr == APP_SCR_4) {
        screen4_handle_table_key(key);
    }
}

void app_key_ui_handle(void)
{
    app_key_ui_dispatch(KEY_Scan_WithDebounce(20));
}
