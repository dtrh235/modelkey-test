#ifndef APP_UI_V3_THEME_H
#define APP_UI_V3_THEME_H

#include "lvgl.h"

#define UI3_W  240
#define UI3_H  320
#define UI3_TOPBAR_H       34
#define UI3_FOOTER_PAD_H   14
#define UI3_FOOTER_GAP     10
#define UI3_DOCK_BTN_H     36
#define UI3_DOCK_HOME_BTN_W ((UI3_W - (UI3_FOOTER_PAD_H * 2) - UI3_FOOTER_GAP) / 2)
#define UI3_DOCK_BTN_RADIUS (UI3_DOCK_BTN_H / 2)

/* preview.css v0.3 — paper RGB565 tuned for ILI9341 */
#define UI3_COL_PAPER       lv_color_hex(0xDAD2C6)
#define UI3_COL_PAPER_DEEP  lv_color_hex(0xDDD5C8)
#define UI3_COL_SURFACE     lv_color_hex(0xF0EBE3)
#define UI3_COL_INK         lv_color_hex(0x2A231C)
#define UI3_COL_INK_SOFT    lv_color_hex(0x5E564C)
#define UI3_COL_INK_FAINT   lv_color_hex(0x9A9084)
#define UI3_COL_LINE        lv_color_hex(0xE0D8CC)
#define UI3_COL_LINE_STRONG lv_color_hex(0xD0C8BC)
#define UI3_COL_TERRA       lv_color_hex(0xC4613A)
#define UI3_COL_TERRA_SOFT  lv_color_hex(0xF3E0D8)
#define UI3_COL_CARD_SEL_BG lv_color_hex(0xEAE2DA)
#define UI3_COL_GOLD        lv_color_hex(0xB8924A)
#define UI3_COL_GOLD_SOFT   lv_color_hex(0xF0E6D0)
#define UI3_COL_DANGER      lv_color_hex(0xB84343)
#define UI3_COL_SAGE        lv_color_hex(0x4A5C3E)
#define UI3_COL_SAGE_MID    lv_color_hex(0x3D4F32)
#define UI3_COL_CLOUD_BG    lv_color_hex(0xC8D3BC)
#define UI3_COL_SHADOW      lv_color_hex(0x1A1814)

void ui3_theme_init(void);
void ui3_theme_apply_disp_bg(void);
void ui3_paint_paper_bg(lv_obj_t *scr);
void ui3_style_obj_paper(lv_obj_t *obj);
void ui3_style_obj_transparent(lv_obj_t *obj);
lv_style_t *ui3_style_paper(void);
lv_style_t *ui3_style_title(void);
lv_style_t *ui3_style_label(void);
lv_style_t *ui3_style_field(void);
lv_style_t *ui3_style_field_focus(void);
lv_style_t *ui3_style_btn_ghost(void);
lv_style_t *ui3_style_btn_fill(void);
lv_style_t *ui3_style_card(void);
lv_style_t *ui3_style_card_sel(void);

#endif
