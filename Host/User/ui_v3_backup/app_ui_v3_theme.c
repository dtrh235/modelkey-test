#include "app_ui_v3_theme.h"
#include "app_ui_v3_fonts.h"
#include "../../Drivers/BSP/LCD/lcd.h"

static lv_style_t s_paper, s_title, s_label, s_field, s_field_focus;
static lv_style_t s_btn_ghost, s_btn_fill, s_card, s_card_sel;

void ui3_theme_init(void)
{
    lv_style_init(&s_paper);
    lv_style_set_bg_color(&s_paper, UI3_COL_PAPER);
    lv_style_set_bg_opa(&s_paper, LV_OPA_COVER);
    lv_style_set_border_width(&s_paper, 0);
    lv_style_set_pad_all(&s_paper, 0);
    lv_style_set_radius(&s_paper, 0);

    lv_style_init(&s_title);
    lv_style_set_text_font(&s_title, UI3_FONT_TITLE);
    lv_style_set_text_color(&s_title, UI3_COL_INK);

    lv_style_init(&s_label);
    lv_style_set_text_font(&s_label, UI3_FONT_HEAD);
    lv_style_set_text_color(&s_label, UI3_COL_INK);

    lv_style_init(&s_field);
    lv_style_set_bg_color(&s_field, UI3_COL_SURFACE);
    lv_style_set_bg_opa(&s_field, LV_OPA_COVER);
    lv_style_set_border_color(&s_field, UI3_COL_LINE_STRONG);
    lv_style_set_border_width(&s_field, 1);
    lv_style_set_radius(&s_field, 12);
    lv_style_set_pad_hor(&s_field, 10);
    lv_style_set_pad_ver(&s_field, 6);
    lv_style_set_text_font(&s_field, UI3_FONT_HEAD);
    lv_style_set_text_color(&s_field, UI3_COL_INK);

    lv_style_init(&s_field_focus);
    lv_style_set_border_color(&s_field_focus, UI3_COL_TERRA);
    lv_style_set_border_width(&s_field_focus, 1);
    lv_style_set_shadow_color(&s_field_focus, UI3_COL_TERRA_SOFT);
    lv_style_set_shadow_width(&s_field_focus, 8);
    lv_style_set_shadow_spread(&s_field_focus, 2);

    lv_style_init(&s_btn_ghost);
    lv_style_set_bg_color(&s_btn_ghost, UI3_COL_SURFACE);
    lv_style_set_bg_opa(&s_btn_ghost, LV_OPA_COVER);
    lv_style_set_radius(&s_btn_ghost, UI3_DOCK_BTN_RADIUS);
    lv_style_set_border_color(&s_btn_ghost, UI3_COL_LINE_STRONG);
    lv_style_set_border_width(&s_btn_ghost, 2);
    lv_style_set_text_color(&s_btn_ghost, UI3_COL_INK_SOFT);
    lv_style_set_text_font(&s_btn_ghost, UI3_FONT_BODY);
    lv_style_set_shadow_width(&s_btn_ghost, 0);

    lv_style_init(&s_btn_fill);
    lv_style_set_bg_color(&s_btn_fill, UI3_COL_INK);
    lv_style_set_bg_opa(&s_btn_fill, LV_OPA_COVER);
    lv_style_set_radius(&s_btn_fill, UI3_DOCK_BTN_RADIUS);
    lv_style_set_border_width(&s_btn_fill, 0);
    lv_style_set_text_color(&s_btn_fill, UI3_COL_SURFACE);
    lv_style_set_text_font(&s_btn_fill, UI3_FONT_BODY);
    lv_style_set_shadow_width(&s_btn_fill, 0);

    lv_style_init(&s_card);
    lv_style_set_bg_color(&s_card, UI3_COL_SURFACE);
    lv_style_set_bg_opa(&s_card, LV_OPA_COVER);
    lv_style_set_border_color(&s_card, UI3_COL_LINE_STRONG);
    lv_style_set_border_width(&s_card, 1);
    lv_style_set_radius(&s_card, 14);
    lv_style_set_pad_all(&s_card, 10);
    lv_style_set_shadow_width(&s_card, 0);

    lv_style_init(&s_card_sel);
    lv_style_set_bg_color(&s_card_sel, UI3_COL_CARD_SEL_BG);
    lv_style_set_bg_opa(&s_card_sel, LV_OPA_COVER);
    lv_style_set_border_color(&s_card_sel, UI3_COL_TERRA);
    lv_style_set_border_width(&s_card_sel, 1);
    lv_style_set_radius(&s_card_sel, 14);
    lv_style_set_shadow_width(&s_card_sel, 0);

    ui3_theme_apply_disp_bg();
}

void ui3_theme_apply_disp_bg(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    uint16_t c565 = lv_color_to16(UI3_COL_PAPER);

    g_back_color = c565;
    lcd_fill(0, 0, (uint16_t)(UI3_W - 1), (uint16_t)(UI3_H - 1), c565);

    if(disp != NULL) {
        lv_disp_set_bg_color(disp, UI3_COL_PAPER);
        lv_disp_set_bg_opa(disp, LV_OPA_COVER);
    }
}

void ui3_paint_paper_bg(lv_obj_t *scr)
{
    if(scr == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(scr, UI3_COL_PAPER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(scr, 0, LV_PART_MAIN);
}

void ui3_style_obj_paper(lv_obj_t *obj)
{
    if(obj == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(obj, UI3_COL_PAPER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
}

void ui3_style_obj_transparent(lv_obj_t *obj)
{
    if(obj == NULL) {
        return;
    }
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
}

lv_style_t *ui3_style_paper(void)       { return &s_paper; }
lv_style_t *ui3_style_title(void)        { return &s_title; }
lv_style_t *ui3_style_label(void)        { return &s_label; }
lv_style_t *ui3_style_field(void)        { return &s_field; }
lv_style_t *ui3_style_field_focus(void)  { return &s_field_focus; }
lv_style_t *ui3_style_btn_ghost(void)    { return &s_btn_ghost; }
lv_style_t *ui3_style_btn_fill(void)     { return &s_btn_fill; }
lv_style_t *ui3_style_card(void)         { return &s_card; }
lv_style_t *ui3_style_card_sel(void)     { return &s_card_sel; }
