#include "app_slave_unlock_queue.h"

#include "app_config.h"
#include "app_rs485_proto.h"
#include "app_slave_host_time.h"
#include "app_slave_diag.h"

#include <string.h>

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE

#define SLAVE_UNLOCK_Q_MAX  8u

typedef struct {
    char acc[13];
    uint8_t method_id;
    uint8_t used;
} slave_unlock_q_item_t;

static slave_unlock_q_item_t s_q[SLAVE_UNLOCK_Q_MAX];

void app_slave_unlock_queue_push(const char *acc, uint8_t method_id)
{
    uint8_t i;

    if(acc == NULL) {
        acc = "";
    }
    for(i = 0u; i < SLAVE_UNLOCK_Q_MAX; i++) {
        if(s_q[i].used != 0u) {
            continue;
        }
        strncpy(s_q[i].acc, acc, sizeof(s_q[i].acc) - 1u);
        s_q[i].acc[sizeof(s_q[i].acc) - 1u] = '\0';
        s_q[i].method_id = method_id;
        s_q[i].used = 1u;
#if (APP_SLAVE_USART1_DEBUG != 0)
        SLAVE_DBG_LOG("[SLV][UNLOCK] queue push acc=%s mid=%u slot=%u",
                      s_q[i].acc, (unsigned)method_id, (unsigned)i);
#endif
        return;
    }
    /* ?????????????????? */
    memmove(&s_q[0], &s_q[1], sizeof(s_q[0]) * (SLAVE_UNLOCK_Q_MAX - 1u));
    strncpy(s_q[SLAVE_UNLOCK_Q_MAX - 1u].acc, acc, sizeof(s_q[SLAVE_UNLOCK_Q_MAX - 1u].acc) - 1u);
    s_q[SLAVE_UNLOCK_Q_MAX - 1u].acc[sizeof(s_q[SLAVE_UNLOCK_Q_MAX - 1u].acc) - 1u] = '\0';
    s_q[SLAVE_UNLOCK_Q_MAX - 1u].method_id = method_id;
    s_q[SLAVE_UNLOCK_Q_MAX - 1u].used = 1u;
}

void app_slave_unlock_queue_flush_rs485(void)
{
    uint8_t i;

    if(app_slave_host_time_ready() == 0u) {
        return;
    }
    for(i = 0u; i < SLAVE_UNLOCK_Q_MAX; i++) {
        if(s_q[i].used == 0u) {
            continue;
        }
        if(app_rs485_proto_slave_unlock_notify(s_q[i].acc, s_q[i].method_id, 400u) != 0u) {
            s_q[i].used = 0u;
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
