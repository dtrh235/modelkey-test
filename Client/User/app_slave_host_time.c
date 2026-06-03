#include "app_slave_host_time.h"

#include "app_config.h"
#include "app_slave_diag.h"
#include "app_nfc_hw.h"

#include <stdint.h>
#include <stdio.h>

extern uint32_t HAL_GetTick(void);

#ifndef APP_SLAVE_UNLOCK_DEFER_RS485_MS
#define APP_SLAVE_UNLOCK_DEFER_RS485_MS  15000u
#endif
#ifndef APP_SLAVE_UNLOCK_NO_HOST_FALLBACK_MS
#define APP_SLAVE_UNLOCK_NO_HOST_FALLBACK_MS  120000u
#endif

static uint32_t s_host_first_ms;
static uint8_t s_host_seen;
#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
static uint8_t s_ready_logged;
#endif

void app_slave_host_time_on_host_frame(void)
{
    if(s_host_seen == 0u) {
        s_host_first_ms = HAL_GetTick();
        s_host_seen = 1u;
#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
        s_ready_logged = 0u;
        SLAVE_UNLOCK_CLOUD_LOG("[SLV][HOST] first RS485 from master defer=%ums",
                               (unsigned)APP_SLAVE_UNLOCK_DEFER_RS485_MS);
#endif
#if (APP_SLAVE_USART1_DEBUG != 0)
        SLAVE_DBG_LOG("[SLV][HOST] RS485 frame from master, wait %ums for unlock notify",
                      (unsigned)APP_SLAVE_UNLOCK_DEFER_RS485_MS);
#endif
    }
}

uint8_t app_slave_host_time_host_seen(void)
{
    return s_host_seen;
}

uint32_t app_slave_host_time_ms_until_ready(void)
{
    uint32_t now = HAL_GetTick();

    if(s_host_seen != 0u) {
        if((now - s_host_first_ms) >= APP_SLAVE_UNLOCK_DEFER_RS485_MS) {
            return 0u;
        }
        return APP_SLAVE_UNLOCK_DEFER_RS485_MS - (now - s_host_first_ms);
    }
    if(now >= APP_SLAVE_UNLOCK_NO_HOST_FALLBACK_MS) {
        return 0u;
    }
    return APP_SLAVE_UNLOCK_NO_HOST_FALLBACK_MS - now;
}

uint8_t app_slave_host_time_ready(void)
{
    return (app_slave_host_time_ms_until_ready() == 0u) ? 1u : 0u;
}

void app_slave_host_time_status_poll(void)
{
    static uint32_t s_last_ms;
    uint32_t now = HAL_GetTick();
    uint32_t left;
    uint8_t ready;

#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
    ready = app_slave_host_time_ready();
    if(ready != 0u && s_ready_logged == 0u) {
        s_ready_logged = 1u;
        SLAVE_UNLOCK_CLOUD_LOG("[SLV][HOST] unlock RS485 ready host_seen=%u",
                               (unsigned)s_host_seen);
    }
#endif

#if (APP_SLAVE_USART1_DEBUG != 0) || (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
    if((now - s_last_ms) < 10000u) {
        return;
    }
    s_last_ms = now;

    ready = app_slave_host_time_ready();
    left = app_slave_host_time_ms_until_ready();
#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
    SLAVE_UNLOCK_CLOUD_LOG("[SLV][STAT] host_seen=%u ready=%u wait_ms=%lu",
                           (unsigned)s_host_seen,
                           (unsigned)ready,
                           (unsigned long)left);
#else
    SLAVE_DBG_LOG("[SLV][STAT] host_seen=%u unlock_ready=%u wait_ms=%lu nfc_ready=%u",
                  (unsigned)s_host_seen,
                  (unsigned)ready,
                  (unsigned long)left,
                  (unsigned)app_nfc_hw_ready());
#endif
#endif
}
