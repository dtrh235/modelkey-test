#include "app_unlock_event.h"

#include <string.h>

#include "app_board_gpio.h"
#include "app_home_unlock.h"
#include "app_unlock_uart4.h"
#include "cloud_ota_service.h"
#include "app_config.h"
#include "app_host_diag.h"
#if (APP_LEGACY_UI_ENABLE != 0)
#include "app_screen1_unlock.h"
#endif
#include "./SYSTEM/sys/sys.h"

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

static uint8_t app_unlock_event_can_lock_gui(void)
{
#if (APP_USE_FREERTOS == 1)
    if(xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return 0u;
    }
    if((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0u) {
        return 0u;
    }
    return 1u;
#else
    return 0u;
#endif
}

static volatile uint8_t s_unlock_popup_pending = 0u;
static volatile uint8_t s_unlock_popup_type = (uint8_t)APP_UNLOCK_POPUP_HOME;
static char s_unlock_popup_account[13];
static char s_unlock_popup_method[16];

void app_unlock_event_handle_success(app_unlock_popup_t popup_type,
                                     const char *account,
                                     const char *method)
{
    if(account == NULL) account = "";
    if(method == NULL) method = "";

    (void)app_unlock_event_can_lock_gui();

    /* Do not touch LVGL from non-GuiTask context (FpTask/NfcTask). */
    strncpy(s_unlock_popup_account, account, sizeof(s_unlock_popup_account) - 1u);
    s_unlock_popup_account[sizeof(s_unlock_popup_account) - 1u] = '\0';
    strncpy(s_unlock_popup_method, method, sizeof(s_unlock_popup_method) - 1u);
    s_unlock_popup_method[sizeof(s_unlock_popup_method) - 1u] = '\0';
    s_unlock_popup_type = (uint8_t)popup_type;
    s_unlock_popup_pending = 1u;

#if (APP_HOST_NAV_DIAG != 0)
    NAV_LOG("[UNLOCK] queued popup=%u acc=%s by=%s\r\n",
            (unsigned)popup_type, s_unlock_popup_account, s_unlock_popup_method);
#endif
    board_relay_unlock_pulse();
    app_unlock_uart4_on_unlock_ok(account, method);
#if (APP_HOST_NAV_DIAG != 0)
    NAV_LOG("[UNLOCK] event acc=%s mtd=%s\r\n", account, method);
#endif
    cloud_ota_service_report_event(CLOUD_EVT_UNLOCK_OK, account);
    if(strcmp(method, "remote") == 0) {
        cloud_ota_service_queue_unlock_report_ex("0", "phone", HAL_GetTick(),
                                                 CLOUD_UNLOCK_DEVICE_PHONE);
    } else if(strcmp(method, "temporary-password") == 0 || strcmp(method, "temporary") == 0) {
        cloud_ota_service_queue_unlock_report_ex("temporary account", "temporary-password",
                                                   HAL_GetTick(), CLOUD_UNLOCK_DEVICE_MASTER);
    } else {
        cloud_ota_service_queue_unlock_report_ex(account, method, HAL_GetTick(),
                                                 CLOUD_UNLOCK_DEVICE_MASTER);
    }
}

void app_unlock_event_gui_pump(void)
{
    if(s_unlock_popup_pending == 0u) {
        return;
    }
    s_unlock_popup_pending = 0u;

    if(s_unlock_popup_type == (uint8_t)APP_UNLOCK_POPUP_HOME) {
        app_home_show_unlock_popup();
    } else {
#if (APP_LEGACY_UI_ENABLE != 0)
        screen1_show_unlock_popup();
#else
        app_home_show_unlock_popup();
#endif
    }
#if (APP_HOST_NAV_DIAG != 0)
    NAV_LOG("[UNLOCK] popup=%u acc=%s by=%s\r\n",
            (unsigned)s_unlock_popup_type, s_unlock_popup_account, s_unlock_popup_method);
#endif
}
