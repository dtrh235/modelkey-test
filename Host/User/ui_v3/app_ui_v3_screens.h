#ifndef APP_UI_V3_SCREENS_H
#define APP_UI_V3_SCREENS_H

#include "lvgl.h"
#include "app_ui_v3_types.h"

lv_obj_t *ui3_build_screen(ui3_state_t *st);
/** WiFi 页：仅刷新热点列表，不整屏 rebuild */
void ui3_wifi_refresh_list(ui3_state_t *st);
/** 仅更新右上角扫描/刷新标签，不重建列表 */
void ui3_wifi_refresh_scan_lbl(ui3_state_t *st);
/** 编辑用户页：仅刷新指纹/NFC/密码行文案 */
void ui3_edit_user_refresh_creds(ui3_state_t *st);

#endif
