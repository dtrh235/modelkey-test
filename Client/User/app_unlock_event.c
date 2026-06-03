#include "app_unlock_event.h"

#include <string.h>

#include "app_home_unlock.h"
#include "app_screen1_unlock.h"
#include "app_unlock_uart4.h"
#include "cloud_ota_service.h"
#include "app_config.h"
#include "./SYSTEM/sys/sys.h"
#include "app_slave_diag.h"

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
#include "app_rs485_proto.h"
#include "app_slave_host_time.h"
#include "app_slave_unlock_queue.h"
#endif

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
static uint8_t unlock_method_to_id(const char *method)
{
    if(method == NULL) {
        return 1u;
    }
    if(strcmp(method, "nfc") == 0) {
        return 2u;
    }
    if(strcmp(method, "fingerprint") == 0) {
        return 3u;
    }
    return 1u;
}
#endif

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE && (APP_USE_FREERTOS == 1)
static volatile uint8_t s_unlock_popup_pending;
static app_unlock_popup_t s_unlock_popup_type = APP_UNLOCK_POPUP_SCREEN1;
#endif

#if (APP_USE_FREERTOS == 1) && !APP_RS485_IS_SLAVE
static uint8_t app_unlock_event_can_lock_gui(void)
{
    if(xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return 0u;
    }
    if((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0u) {
        return 0u;
    }
    return 1u;
}
#endif

void app_unlock_event_gui_poll(void)
{
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE && (APP_USE_FREERTOS == 1)
    if(s_unlock_popup_pending == 0u) {
        return;
    }
    s_unlock_popup_pending = 0u;
    if(s_unlock_popup_type == APP_UNLOCK_POPUP_HOME) {
        app_home_show_unlock_popup();
    } else {
        screen1_show_unlock_popup();
    }
#else
    (void)0;
#endif
}

void app_unlock_event_handle_success(app_unlock_popup_t popup_type,
                                     const char *account,
                                     const char *method)
{
    if(account == NULL) account = "";
    if(method == NULL) method = "";

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
    /* 硬件开锁先走；RS485 上报等主机侧校时窗口后再发（与主机开锁记录策略一致） */
    app_unlock_uart4_on_unlock_ok(account, method);
    if(app_slave_host_time_ready() != 0u) {
        SLAVE_UNLOCK_CLOUD_LOG("[SLV][UNLOCK] acc=%s mtd=%s -> rs485 now",
                               account, method);
        app_rs485_slave_unlock_notify_async(account, unlock_method_to_id(method));
    } else {
        SLAVE_UNLOCK_CLOUD_LOG("[SLV][UNLOCK] acc=%s mtd=%s -> rs485 queue wait_ms=%lu",
                               account, method,
                               (unsigned long)app_slave_host_time_ms_until_ready());
        app_slave_unlock_queue_push(account, unlock_method_to_id(method));
    }
#if (APP_USE_FREERTOS == 1)
    s_unlock_popup_type = popup_type;
    s_unlock_popup_pending = 1u;
#else
    if(popup_type == APP_UNLOCK_POPUP_HOME) {
        app_home_show_unlock_popup();
    } else {
        screen1_show_unlock_popup();
    }
#endif
#else
    {
        uint8_t gui_locked = 0u;

        if(app_unlock_event_can_lock_gui() != 0u) {
            vTaskSuspendAll();
            gui_locked = 1u;
        }

        if(popup_type == APP_UNLOCK_POPUP_HOME) {
            app_home_show_unlock_popup();
        } else {
            screen1_show_unlock_popup();
        }

        if(gui_locked != 0u) {
            (void)xTaskResumeAll();
        }
    }

    app_unlock_uart4_on_unlock_ok(account, method);
    cloud_ota_service_report_event(CLOUD_EVT_UNLOCK_OK, account);
    cloud_ota_service_report_unlock_record(account, method, HAL_GetTick());
#endif
}
