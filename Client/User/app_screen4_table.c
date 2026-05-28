#include "app_screen4_table.h"

#include "lvgl.h"
#include "gui_guider.h"
#include "app_user_ops.h"
#include "user_auth_portable.h"

extern lv_ui guider_ui;
extern user_cred_t g_users[APP_USER_MAX];
extern uint8_t g_user_count;
extern uint8_t g_default_admin_deleted;
extern uint8_t g_default_admin_is_admin_role;
extern char g_default_admin_account[13];

void screen4_refresh_table(void)
{
    lv_obj_t *table = guider_ui.screen_4_table_1;
    if(!lv_obj_is_valid(table)) return;

    const uint16_t header_row = 0u;
    uint16_t user_cnt = 0u;
    uint16_t max_rows = lv_table_get_row_cnt(table);
    uint16_t max_data_rows = (max_rows > 1u) ? (max_rows - 1u) : 0u;
    uint16_t row;
    uint16_t placed;
    uint16_t target;
    uint8_t i;

    if(!g_default_admin_deleted) user_cnt++;
    for(i = 0u; i < g_user_count; i++) {
        if(g_users[i].active) user_cnt++;
    }

    lv_table_set_col_cnt(table, 2u);
    lv_table_set_col_width(table, 0u, 120);
    lv_table_set_col_width(table, 1u, 120);

    lv_table_set_cell_value(table, header_row, 0u, "账号");
    lv_table_set_cell_value(table, header_row, 1u, "身份");

    if(max_data_rows == 0u) return;

    if(user_cnt == 0u) {
        lv_table_set_cell_value(table, 1u, 0u, "无用户");
        lv_table_set_cell_value(table, 1u, 1u, "");
        for(row = 2u; row < max_rows; row++) {
            lv_table_set_cell_value(table, row, 0u, "");
            lv_table_set_cell_value(table, row, 1u, "");
        }
        return;
    }

    target = user_cnt;
    if(target > max_data_rows) target = max_data_rows;

    row = 1u;
    placed = 0u;
    if(!g_default_admin_deleted) {
        lv_table_set_cell_value(table, row, 0u, g_default_admin_account);
        lv_table_set_cell_value(table, row, 1u, g_default_admin_is_admin_role ? "管理员" : "用户");
        row++;
        placed++;
    }

    for(i = 0u; i < g_user_count && placed < target; i++) {
        if(!g_users[i].active) continue;
        lv_table_set_cell_value(table, row, 0u, g_users[i].acc);
        lv_table_set_cell_value(table, row, 1u, g_users[i].is_admin ? "管理员" : "用户");
        row++;
        placed++;
    }

    for(; row < max_rows; row++) {
        lv_table_set_cell_value(table, row, 0u, "");
        lv_table_set_cell_value(table, row, 1u, "");
    }
}

void screen4_handle_table_key(KeyValue_t key)
{
    lv_obj_t *table = guider_ui.screen_4_table_1;
    if(!lv_obj_is_valid(table)) return;
    if(key == KEY_UP) {
        int32_t k = LV_KEY_UP;
        lv_event_send(table, LV_EVENT_KEY, &k);
    } else if(key == KEY_DOWN) {
        int32_t k = LV_KEY_DOWN;
        lv_event_send(table, LV_EVENT_KEY, &k);
    }
}
