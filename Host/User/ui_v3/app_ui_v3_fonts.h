#ifndef APP_UI_V3_FONTS_H
#define APP_UI_V3_FONTS_H

#include "lvgl.h"

LV_FONT_DECLARE(lv_font_ui_v3_13);
LV_FONT_DECLARE(lv_font_ui_v3_16);
LV_FONT_DECLARE(lv_font_ui_v3_20);
LV_FONT_DECLARE(lv_font_ui_v3_num_36);
/* LVGL 内置图标字体：仅用于 LV_SYMBOL_*，中文仍用 lv_font_ui_v3_* */
LV_FONT_DECLARE(lv_font_montserrat_14);

#define UI3_FONT_BODY   (&lv_font_ui_v3_13)
#define UI3_FONT_HEAD   (&lv_font_ui_v3_16)
#define UI3_FONT_TITLE  (&lv_font_ui_v3_20)
#define UI3_FONT_MENU   (&lv_font_ui_v3_20)
#define UI3_FONT_WIFI   (&lv_font_ui_v3_16)
#define UI3_FONT_WIFI_TAG (&lv_font_ui_v3_13)
#define UI3_FONT_CLOCK  (&lv_font_ui_v3_num_36)
#define UI3_FONT_SYMBOL (&lv_font_montserrat_14)

#endif
