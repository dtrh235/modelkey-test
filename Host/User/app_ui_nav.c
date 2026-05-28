#include "app_ui_nav.h"

#include "app_config.h"
#include "app_user_ops.h"
#include "app_screen.h"
#include "stm32f4xx_hal.h"
#include "app_host_diag.h"

static volatile uint8_t s_ui_nav_busy;

void app_ui_nav_begin(uint8_t from_scr)
{
    (void)from_scr;
    s_ui_nav_busy = 1u;
    NAV_LOG("[NAV] begin from=%u t=%lu\r\n", (unsigned)from_scr, (unsigned long)HAL_GetTick());
}

void app_ui_nav_end(uint8_t to_scr)
{
    s_ui_nav_busy = 0u;
    NAV_LOG("[NAV] end to=%u t=%lu\r\n", (unsigned)to_scr, (unsigned long)HAL_GetTick());
#if (APP_USE_FREERTOS == 1)
    users_storage_kick_flush();
#endif
}

uint8_t app_ui_nav_is_busy(void)
{
    return s_ui_nav_busy;
}
