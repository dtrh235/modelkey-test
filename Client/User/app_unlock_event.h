#ifndef APP_UNLOCK_EVENT_H
#define APP_UNLOCK_EVENT_H

typedef enum {
    APP_UNLOCK_POPUP_HOME = 0,
    APP_UNLOCK_POPUP_SCREEN1 = 1
} app_unlock_popup_t;

void app_unlock_event_handle_success(app_unlock_popup_t popup_type,
                                     const char *account,
                                     const char *method);

/* 从机：在 GuiTask 里调，勿在 NfcTask 里直接 lv_obj_* */
void app_unlock_event_gui_poll(void);

#endif
