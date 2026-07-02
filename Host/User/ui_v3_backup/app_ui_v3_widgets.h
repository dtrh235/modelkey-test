#ifndef APP_UI_V3_WIDGETS_H
#define APP_UI_V3_WIDGETS_H

#include "lvgl.h"
#include "app_ui_v3_types.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *top_time;
    lv_obj_t *cloud;
    lv_obj_t *body;
    lv_obj_t *footer;
    lv_obj_t *scroll_track;
} ui3_layout_t;

lv_obj_t *ui3_screen_create(void);
void ui3_topbar(ui3_layout_t *lo, ui3_state_t *st);
void ui3_page_head(ui3_layout_t *lo, const char *title, const char *sub, bool tight);
lv_obj_t *ui3_body_fill(ui3_layout_t *lo, lv_coord_t top_y, lv_coord_t bottom_h);
lv_obj_t *ui3_scroll_viewport(ui3_layout_t *lo, int16_t h);
lv_obj_t *ui3_scroll_dots(ui3_layout_t *lo, ui3_state_t *st, lv_coord_t y);
lv_obj_t *ui3_footer_dock(ui3_layout_t *lo, const char *ok, bool show_esc,
                          lv_event_cb_t on_ok, lv_event_cb_t on_esc, void *user,
                          bool primary_fill);
lv_obj_t *ui3_footer_back(ui3_layout_t *lo, lv_event_cb_t on_esc, void *user);
lv_obj_t *ui3_footer_pair(ui3_layout_t *lo, lv_event_cb_t on_esc, lv_event_cb_t on_regen, void *user);
lv_obj_t *ui3_field_block(lv_obj_t *parent, const char *label, const char *val, bool focus,
                          bool disabled, lv_event_cb_t on_tap, void *user);
lv_obj_t *ui3_role_picker(lv_obj_t *parent, bool is_admin, bool focus);
void ui3_role_picker_set(lv_obj_t *row, bool is_admin);
lv_obj_t *ui3_list_card(lv_obj_t *parent, const char *title, const char *sub, bool sel);
lv_obj_t *ui3_menu_card(lv_obj_t *parent, const char *icon, const char *title,
                        const char *sub, bool sel, lv_event_cb_t on_click, void *user);
void ui3_card_set_sel(lv_obj_t *card, bool sel, bool menu_style);
lv_obj_t *ui3_dock_btn(lv_obj_t *parent, const char *txt, bool fill, lv_coord_t w);
void ui3_dock_btn_set_sel(lv_obj_t *btn, bool fill, bool sel);

lv_obj_t *ui3_signal_dots(lv_obj_t *parent, uint8_t level);
void ui3_home_build(ui3_layout_t *lo, ui3_state_t *st,
                    lv_event_cb_t on_menu, lv_event_cb_t on_unlock, lv_event_cb_t on_lock);
void ui3_home_footer_sel(ui3_state_t *st);
lv_obj_t *ui3_detail_card(lv_obj_t *parent, const char *k, const char *v,
                          bool show_chg, lv_event_cb_t on_chg, void *user);
lv_obj_t *ui3_user_row(lv_obj_t *parent, const char *acc, bool admin, bool sel);
lv_obj_t *ui3_wifi_banner(lv_obj_t *parent, const char *ssid, const char *sub, uint8_t rssi);
lv_obj_t *ui3_wifi_scan_link(lv_obj_t *parent, bool scanning, lv_event_cb_t cb, void *user);
lv_obj_t *ui3_wifi_item(lv_obj_t *parent, const char *ssid, uint8_t rssi,
                        bool connected, bool sel, lv_event_cb_t cb, void *user);
void ui3_pair_zone(ui3_layout_t *lo, ui3_state_t *st);
lv_obj_t *ui3_bio_block(lv_obj_t *parent, lv_event_cb_t on_fp, lv_event_cb_t on_nfc, void *user);
lv_obj_t *ui3_action_stack(lv_obj_t *parent, const char *primary, const char *danger,
                           lv_event_cb_t on_pri, lv_event_cb_t on_dan, void *user);
lv_obj_t *ui3_form_zone(ui3_layout_t *lo, ui3_state_t *st,
                        const char *l0, const char *v0, uint8_t f0,
                        const char *l1, const char *v1, uint8_t f1,
                        const char *err, const char *lock_hint,
                        bool fields_locked, lv_event_cb_t on_field);
void ui3_inputs_reset(ui3_state_t *st);
void ui3_inputs_track(ui3_state_t *st, uint8_t idx, lv_obj_t *wrap);
void ui3_pwd_mask_fill(char *buf, size_t sz, const char *pwd);
bool ui3_inputs_live_refresh(ui3_state_t *st);
void ui3_update_clock_label(lv_obj_t *time_lbl);
void ui3_update_date_label(lv_obj_t *date_lbl);

#endif
