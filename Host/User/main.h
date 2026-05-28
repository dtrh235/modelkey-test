#ifndef MAIN_H
#define MAIN_H

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/KEY/key.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/TIMER/btim.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "gui_guider.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "app_config.h"
/* Do not #define stubs for real API names here: they break declarations in
 * cloud_ota_service.h / cloud_legacy_adapter.h when APP_CLOUD_ENABLE==0. */

#include "events_init.h"
#include "cloud_ota_service.h"
#include "cloud_legacy_adapter.h"
#include "cloud_aliyun_at.h"
#include "app_wifi_cfg.h"
#include "user_auth_portable.h"
#include "ui_common_utils.h"
#include "ui_auth_input.h"
#include "ui_menu_popup_utils.h"
#include "ui_screen8_utils.h"
#include "ui_flow_panels.h"
#include "app_screen.h"
#include "ui_nav_flow.h"
#include "app_user_ops.h"
#include "app_nfc_hw.h"
#include "app_home_unlock.h"
#include "app_home_fp_poll.h"
#include "app_screen_auth.h"
#include "app_screen7_flow.h"
#include "app_screen5_flow.h"
#include "app_screen_wifi_flow.h"
#include "app_screen3_menu.h"
#include "app_screen4_table.h"
#include "app_screen8_focus.h"
#include "app_screen6_info.h"
#include "app_screen6_commit.h"
#include "app_screen6_dialog_base.h"
#include "app_screen6_types.h"
#include "app_screen6_dialog_flow.h"
#include "app_screen1_unlock.h"
#include "app_screen2_login.h"
#include "app_screen8_popup.h"
#include "app_screen8_enroll.h"
#include "app_screen8_flow.h"
#include "app_screen9_10_flow.h"
#include "app_nav_entries.h"
#include "app_key_ui.h"
#include "app_touch_ui.h"
#include "app_board_gpio.h"
#include "app_state.h"
#include "app_home_nav_btns.h"

LV_FONT_DECLARE(lv_font_simsun_16_cjk);

#endif
