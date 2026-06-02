#ifndef APP_SLAVE_HOST_TIME_H
#define APP_SLAVE_HOST_TIME_H

#include <stdint.h>

/* ?????? RS485 ?��??????????????????? SNTP/???????? */
void app_slave_host_time_on_host_frame(void);
uint8_t app_slave_host_time_ready(void);
uint8_t app_slave_host_time_host_seen(void);
/* 0 if ready or no host yet past fallback; else ms remaining */
uint32_t app_slave_host_time_ms_until_ready(void);
void app_slave_host_time_status_poll(void);

#endif
