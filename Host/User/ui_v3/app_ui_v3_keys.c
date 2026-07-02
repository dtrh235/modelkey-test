#include "app_ui_v3_keys.h"

#if (APP_UI_V3_ENABLE == 1)

#include <string.h>
#include <stdio.h>
#include "app_ui_v3.h"
#include "app_ui_v3_widgets.h"
#include "app_ui_v3_anim.h"
#include "app_ui_v3_users.h"
#include "app_ui_v3_services.h"
#include "ui_common_utils.h"
#include "stm32f4xx_hal.h"

static void append_char(char *buf, size_t sz, char c, uint16_t max_len)
{
    size_t n = strlen(buf);
    if(c == '\0' || n + 1u >= sz || n >= max_len) {
        return;
    }
    buf[n] = c;
    buf[n + 1u] = '\0';
}

static void backspace(char *buf)
{
    size_t n = strlen(buf);
    if(n > 0u) {
        buf[n - 1u] = '\0';
    }
}

static char *active_text_buf(ui3_state_t *st)
{
    switch(st->scr) {
    case UI3_SCR_UNLOCK:
        return (st->unlock_field == 0u) ? st->unlock_acc : st->unlock_pwd;
    case UI3_SCR_ADMIN:
        return (st->admin_field == 0u) ? st->admin_acc : st->admin_pwd;
    case UI3_SCR_SEARCH:
        return st->search_acc;
    case UI3_SCR_DEL_USER:
        return st->del_acc;
    case UI3_SCR_ADD_USER:
        if(st->add_field == 0u) {
            return st->add_acc;
        }
        if(st->add_field == 1u) {
            return st->add_pwd;
        }
        return NULL;
    case UI3_SCR_EDIT_USER:
        if(st->edit_pwd_editing == 0u) {
            return NULL;
        }
        return st->edit_pwd;
    case UI3_SCR_WIFI:
        if(st->wifi_modal != 0u) {
            return st->wifi_pwd;
        }
        return NULL;
    default:
        return NULL;
    }
}

static uint16_t active_field_max_len(ui3_state_t *st)
{
    switch(st->scr) {
    case UI3_SCR_UNLOCK:
        return (st->unlock_field == 0u) ? 12u : 10u;
    case UI3_SCR_ADMIN:
        return (st->admin_field == 0u) ? 12u : 10u;
    case UI3_SCR_SEARCH:
    case UI3_SCR_DEL_USER:
        return 12u;
    case UI3_SCR_ADD_USER:
        return (st->add_field == 0u) ? 12u : 10u;
    case UI3_SCR_EDIT_USER:
        return 10u;
    case UI3_SCR_WIFI:
        return 63u;
    default:
        return 12u;
    }
}

static void clear_input_error(ui3_state_t *st)
{
    if(st->scr == UI3_SCR_UNLOCK) {
        st->unlock_show_err = 0u;
    } else if(st->scr == UI3_SCR_ADMIN) {
        st->admin_show_err = 0u;
    }
}

static void reload_screen(ui3_state_t *st)
{
    if(st->unlock_show_err == 0u && st->admin_show_err == 0u &&
       !(st->scr == UI3_SCR_ADD_USER && st->add_field >= 2u) &&
       !(st->scr == UI3_SCR_EDIT_USER && (st->edit_pwd_editing == 0u || st->edit_field != 1u)) &&
       ui3_inputs_live_refresh(st)) {
        return;
    }
    ui3_nav_to(st->scr, false);
}

uint8_t app_ui_v3_key_dispatch(KeyValue_t key)
{
    ui3_state_t *st = ui3_state();
    char *txt;

    if(key == KEY_NONE) {
        return 1u;
    }

    if(ui3_modal_blocks_input() != 0u) {
        return 1u;
    }

    if(st->scr == UI3_SCR_HOME) {
        if(key == KEY_LEFT) {
            st->home_nav_sel = 0u;
            ui3_home_footer_sel(st);
            return 1u;
        }
        if(key == KEY_RIGHT) {
            st->home_nav_sel = 1u;
            ui3_home_footer_sel(st);
            return 1u;
        }
        if(key == KEY_OK) {
            if(st->home_nav_sel == 0u) {
                st->admin_acc[0] = '\0';
                st->admin_pwd[0] = '\0';
                st->admin_field = 0u;
                st->admin_show_err = 0u;
                ui3_nav_to(UI3_SCR_ADMIN, true);
            } else if(st->home_nav_sel == 1u) {
                st->unlock_acc[0] = '\0';
                st->unlock_pwd[0] = '\0';
                st->unlock_field = 0u;
                st->unlock_show_err = 0u;
                ui3_nav_to(UI3_SCR_UNLOCK, true);
            }
            return 1u;
        }
        return 1u;
    }

    if(st->scr == UI3_SCR_MENU) {
        if(key == KEY_UP && st->menu_index > 0u) {
            st->menu_index--;
            st->menu_armed = 0u;
            reload_screen(st);
        } else if(key == KEY_DOWN && st->menu_index + 1u < UI3_MENU_COUNT) {
            st->menu_index++;
            st->menu_armed = 0u;
            reload_screen(st);
        } else if(key == KEY_OK) {
            if(st->menu_armed == 0u) {
                st->menu_armed = 1u;
                ui3_menu_refresh_sel();
            } else {
                ui3_menu_enter_selected();
            }
        } else if(key == KEY_LEFT && st->footer_btn_back != NULL) {
            ui3_footer_on_key(UI3_FOOTER_SEL_BACK);
        } else if(key == KEY_RIGHT && st->footer_btn_ok != NULL) {
            ui3_footer_on_key(UI3_FOOTER_SEL_OK);
        }
        return 1u;
    }

    if(st->scr == UI3_SCR_WIFI) {
        if(st->wifi_modal != 0u) {
            if(key == KEY_OK) {
                ui3_wifi_modal_confirm(st);
                st->wifi_modal = 0u;
                ui3_wifi_pwd_modal_close();
                ui3_reload_current();
                return 1u;
            }
            if(key == KEY_ESC) {
                st->wifi_modal = 0u;
                st->wifi_pwd[0] = '\0';
                ui3_wifi_pwd_modal_close();
                ui3_reload_current();
                return 1u;
            }
        } else {
            if(key == KEY_UP && st->wifi_index > 0u) {
                st->wifi_index--;
                reload_screen(st);
                return 1u;
            }
            if(key == KEY_DOWN) {
                uint8_t n = ui3_wifi_row_count();
                if(st->wifi_index + 1u < n) {
                    st->wifi_index++;
                    reload_screen(st);
                }
                return 1u;
            }
        }
    }

    if(st->scr == UI3_SCR_ADD_USER) {
        if(key == KEY_LEFT && st->add_field == 2u) {
            st->add_admin = 0u;
            reload_screen(st);
            return 1u;
        }
        if(key == KEY_RIGHT && st->add_field == 2u) {
            st->add_admin = 1u;
            reload_screen(st);
            return 1u;
        }
        if(key == KEY_UP) {
            if(st->scroll_px > 0u && st->add_field <= st->scroll_px) {
                st->scroll_px--;
                reload_screen(st);
                return 1u;
            }
            if(st->add_field > 0u) {
                st->add_field--;
                reload_screen(st);
            }
            return 1u;
        }
        if(key == KEY_DOWN) {
            if(st->add_field < 2u) {
                st->add_field++;
                reload_screen(st);
                return 1u;
            }
            if(st->scroll_px < st->scroll_max) {
                st->scroll_px++;
                reload_screen(st);
            }
            return 1u;
        }
    }

    if(st->scr == UI3_SCR_EDIT_USER) {
        if(key == KEY_UP || key == KEY_DOWN) {
            return 1u;
        }
        if(st->edit_pwd_editing != 0u) {
            st->edit_field = 1u;
        }
    }

    if(st->scr == UI3_SCR_UNLOCK) {
        if(st->lockout_until_ms > HAL_GetTick()) {
            if(key == KEY_ESC) {
                ui3_nav_back();
            }
            return 1u;
        }
        if(key == KEY_UP && st->unlock_field > 0u) {
            st->unlock_field--;
            reload_screen(st);
            return 1u;
        }
        if(key == KEY_DOWN && st->unlock_field < 1u) {
            st->unlock_field++;
            reload_screen(st);
            return 1u;
        }
    }

    if(st->scr == UI3_SCR_ADMIN) {
        if(key == KEY_UP && st->admin_field > 0u) {
            st->admin_field--;
            reload_screen(st);
            return 1u;
        }
        if(key == KEY_DOWN && st->admin_field < 1u) {
            st->admin_field++;
            reload_screen(st);
            return 1u;
        }
    }

    txt = active_text_buf(st);
    if(txt != NULL) {
        int digit = ui_key_to_digit(key);
        if(digit >= 0) {
            clear_input_error(st);
            append_char(txt, 64, (char)('0' + digit), active_field_max_len(st));
            if(st->scr == UI3_SCR_WIFI && st->wifi_modal != 0u) {
                ui3_wifi_pwd_modal_refresh(st->wifi_pwd);
                return 1u;
            }
            reload_screen(st);
            return 1u;
        }
        if(key == KEY_LEFT) {
            clear_input_error(st);
            backspace(txt);
            if(st->scr == UI3_SCR_WIFI && st->wifi_modal != 0u) {
                ui3_wifi_pwd_modal_refresh(st->wifi_pwd);
                return 1u;
            }
            reload_screen(st);
            return 1u;
        }
    }

    if(key == KEY_LEFT && st->footer_btn_back != NULL) {
        ui3_footer_on_key(UI3_FOOTER_SEL_BACK);
        return 1u;
    }
    if(key == KEY_RIGHT && st->footer_btn_ok != NULL) {
        ui3_footer_on_key(UI3_FOOTER_SEL_OK);
        return 1u;
    }

    if(key == KEY_ESC) {
        if(txt != NULL && txt[0] != '\0') {
            clear_input_error(st);
            backspace(txt);
            reload_screen(st);
        } else if(st->footer_btn_back != NULL) {
            ui3_footer_on_key(UI3_FOOTER_SEL_BACK);
        } else {
            ui3_nav_back();
        }
        return 1u;
    }

    if(key == KEY_OK && st->footer_btn_ok != NULL) {
        ui3_footer_on_key(UI3_FOOTER_SEL_OK);
        return 1u;
    }

    return 1u;
}

#endif
