#include "app_screen2_login.h"

#include "lvgl.h"
#include "gui_guider.h"
#include "cloud_ota_service.h"
#include "app_user_ops.h"

extern lv_ui guider_ui;
extern void enter_screen_3(void);
extern void screen2_show_error_label(void);

void try_screen2_admin_login(void)
{
    const char *acc;
    const char *pwd;

    if(!lv_obj_is_valid(guider_ui.screen_2_ta_2) || !lv_obj_is_valid(guider_ui.screen_2_ta_1)) return;

    acc = lv_textarea_get_text(guider_ui.screen_2_ta_2);
    pwd = lv_textarea_get_text(guider_ui.screen_2_ta_1);
    if(admin_credentials_match_with_delete(acc, pwd)) {
        cloud_ota_service_report_event(CLOUD_EVT_ADMIN_LOGIN_OK, acc);
        enter_screen_3();
    } else {
        cloud_ota_service_report_event(CLOUD_EVT_ADMIN_LOGIN_FAIL, acc);
        screen2_show_error_label();
    }
}
