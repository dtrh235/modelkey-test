#ifndef APP_SLAVE_UNLOCK_QUEUE_H
#define APP_SLAVE_UNLOCK_QUEUE_H

#include <stdint.h>

void app_slave_unlock_queue_push(const char *acc, uint8_t method_id);
void app_slave_unlock_queue_flush_rs485(void);

#endif
