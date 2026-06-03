#ifndef APP_SLAVE_UI_FIXUP_H
#define APP_SLAVE_UI_FIXUP_H

#include "app_config.h"

#if APP_RS485_IS_SLAVE
void app_slave_ui_hide_corner_ok_esc(void);
/* 禁用 screen_1 文本框 LVGL 点击/键盘回调，避免 g_kb_top_layer 未创建时 ta_event_cb 硬故障。 */
void app_slave_ui_sanitize_screen1_textareas(void);
#endif

#endif /* APP_SLAVE_UI_FIXUP_H */
