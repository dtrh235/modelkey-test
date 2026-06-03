#include "app_slave_unlock_queue.h"

#include "app_config.h"
#include "app_rs485_proto.h"
#include "app_slave_host_time.h"
#include "app_slave_diag.h"

#include <string.h>
#include <stdio.h>

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

extern uint32_t HAL_GetTick(void);

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE

#define SLAVE_UNLOCK_Q_MAX  8u

typedef struct {
    char acc[13];
    uint8_t method_id;
    uint8_t used;
} slave_unlock_q_item_t;

static slave_unlock_q_item_t s_q[SLAVE_UNLOCK_Q_MAX];

#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
static uint32_t s_flush_skip_log_ms;
static uint8_t s_flush_skip_log_pend;
#endif
void app_slave_unlock_queue_push(const char *acc, uint8_t method_id)
{
    uint8_t i;

    if(acc == NULL) {
        acc = "";
    }
    for(i = 0u; i < SLAVE_UNLOCK_Q_MAX; i++) {
        if(s_q[i].used != 0u && strcmp(s_q[i].acc, acc) == 0 && s_q[i].method_id == method_id) {
#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
            SLAVE_UNLOCK_CLOUD_LOG("[SLV][UNLOCK] queue dup skip acc=%s mid=%u",
                                   acc, (unsigned)method_id);
#endif
            return;
        }
    }
    for(i = 0u; i < SLAVE_UNLOCK_Q_MAX; i++) {
        if(s_q[i].used != 0u) {
            continue;
        }
        strncpy(s_q[i].acc, acc, sizeof(s_q[i].acc) - 1u);
        s_q[i].acc[sizeof(s_q[i].acc) - 1u] = '\0';
        s_q[i].method_id = method_id;
        s_q[i].used = 1u;
#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
        SLAVE_UNLOCK_CLOUD_LOG("[SLV][UNLOCK] queue acc=%s mid=%u slot=%u",
                               s_q[i].acc, (unsigned)method_id, (unsigned)i);
#endif
        return;
    }
    /* 队列满：丢弃最旧一条 */
    memmove(&s_q[0], &s_q[1], sizeof(s_q[0]) * (SLAVE_UNLOCK_Q_MAX - 1u));
    strncpy(s_q[SLAVE_UNLOCK_Q_MAX - 1u].acc, acc, sizeof(s_q[SLAVE_UNLOCK_Q_MAX - 1u].acc) - 1u);
    s_q[SLAVE_UNLOCK_Q_MAX - 1u].acc[sizeof(s_q[SLAVE_UNLOCK_Q_MAX - 1u].acc) - 1u] = '\0';
    s_q[SLAVE_UNLOCK_Q_MAX - 1u].method_id = method_id;
    s_q[SLAVE_UNLOCK_Q_MAX - 1u].used = 1u;
#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
    SLAVE_UNLOCK_CLOUD_LOG("[SLV][UNLOCK] queue FULL drop oldest acc=%s",
                           s_q[SLAVE_UNLOCK_Q_MAX - 1u].acc);
#endif
}

void app_slave_unlock_queue_flush_rs485(void)
{
    uint8_t i;
    uint8_t pending = 0u;
    static uint32_t s_flush_fail_backoff_ms;

    for(i = 0u; i < SLAVE_UNLOCK_Q_MAX; i++) {
        if(s_q[i].used != 0u) {
            pending++;
        }
    }
    if(pending == 0u) {
        return;
    }
    if(app_slave_host_time_ready() == 0u) {
#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
        {
            uint32_t now = HAL_GetTick();
            if(pending != s_flush_skip_log_pend ||
               (uint32_t)(now - s_flush_skip_log_ms) >= 10000u) {
                s_flush_skip_log_ms = now;
                s_flush_skip_log_pend = pending;
                SLAVE_UNLOCK_CLOUD_LOG("[SLV][UNLOCK] flush skip not ready pend=%u wait_ms=%lu",
                                       (unsigned)pending,
                                       (unsigned long)app_slave_host_time_ms_until_ready());
            }
        }
#endif
        return;
    }
    if(s_flush_fail_backoff_ms != 0u &&
       (uint32_t)(HAL_GetTick() - s_flush_fail_backoff_ms) < 2000u) {
        return;
    }
#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
    SLAVE_UNLOCK_CLOUD_LOG("[SLV][UNLOCK] flush start pend=%u", (unsigned)pending);
#endif
    {
        uint8_t any_fail = 0u;

        for(i = 0u; i < SLAVE_UNLOCK_Q_MAX; i++) {
            if(s_q[i].used == 0u) {
                continue;
            }
            if(app_rs485_proto_slave_unlock_notify(s_q[i].acc, s_q[i].method_id, 400u) != 0u) {
#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
                SLAVE_UNLOCK_CLOUD_LOG("[SLV][UNLOCK] flush tx OK slot=%u acc=%s mid=%u",
                                       (unsigned)i, s_q[i].acc, (unsigned)s_q[i].method_id);
#endif
                s_q[i].used = 0u;
            } else {
                any_fail = 1u;
#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
                SLAVE_UNLOCK_CLOUD_LOG("[SLV][UNLOCK] flush tx FAIL slot=%u acc=%s mid=%u",
                                       (unsigned)i, s_q[i].acc, (unsigned)s_q[i].method_id);
#endif
            }
        }
        if(any_fail != 0u) {
            s_flush_fail_backoff_ms = HAL_GetTick();
        } else {
            s_flush_fail_backoff_ms = 0u;
        }
    }
}

#else

void app_slave_unlock_queue_push(const char *acc, uint8_t method_id)
{
    (void)acc;
    (void)method_id;
}

void app_slave_unlock_queue_flush_rs485(void)
{
}

#endif
