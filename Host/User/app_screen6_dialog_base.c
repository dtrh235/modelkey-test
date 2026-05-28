#include "app_screen6_dialog_base.h"

#include "ui_auth_input.h"

LV_FONT_DECLARE(lv_font_montserratMedium_16);

extern lv_obj_t *g_screen6_dlg_root;
extern lv_obj_t *g_screen6_dlg_btn_close;
extern lv_obj_t *g_screen6_dlg_btn_ok;
extern lv_obj_t *g_screen6_dlg_btn_esc;
extern lv_obj_t *g_screen6_dlg_ta;
extern lv_obj_t *g_screen6_dlg_cursor;
extern uint32_t g_screen6_dlg_cursor_ms;
extern uint8_t g_screen6_dlg_cursor_vis;

void screen6_close_dialog(void)
{
    if(g_screen6_dlg_root != NULL && lv_obj_is_valid(g_screen6_dlg_root)) {
        lv_obj_del(g_screen6_dlg_root);
    }
    g_screen6_dlg_root = NULL;
    g_screen6_dlg_btn_close = NULL;
    g_screen6_dlg_btn_ok = NULL;
    g_screen6_dlg_btn_esc = NULL;
    g_screen6_dlg_ta = NULL;
    g_screen6_dlg_cursor = NULL;
}

void screen6_dlg_update_cursor_pos(void)
{
    lv_obj_t *ta = g_screen6_dlg_ta;
    ui_auth_update_cursor_pos(g_screen6_dlg_cursor, ta);
}

void screen6_dlg_setup_cursor_on_ta(lv_obj_t *ta)
{
    if(!lv_obj_is_valid(ta)) return;
    ui_cursor_attach_bar(&g_screen6_dlg_cursor, ta);
    lv_obj_set_style_text_font(g_screen6_dlg_cursor, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    screen6_dlg_update_cursor_pos();
    ui_cursor_activate(g_screen6_dlg_cursor, &g_screen6_dlg_cursor_vis, &g_screen6_dlg_cursor_ms);
}

void screen6_dlg_style_ta_like_screen5(lv_obj_t *ta, uint16_t max_len)
{
    lv_textarea_set_text(ta, "");
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, false);
    lv_textarea_set_max_length(ta, max_len);
    lv_obj_set_style_text_color(ta, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ta, &lv_font_montserratMedium_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ta, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ta, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ta, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ta, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ta, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ta, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ta, lv_color_hex(0xe6e6e6), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ta, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ta, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ta, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ta, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(ta, 0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_width(ta, 0, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ta, 0, LV_PART_CURSOR | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ta, 0, LV_PART_CURSOR | LV_STATE_DEFAULT);
}
