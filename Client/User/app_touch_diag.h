#ifndef APP_TOUCH_DIAG_H
#define APP_TOUCH_DIAG_H

#include "app_config.h"

#ifndef APP_TOUCH_UART_DEBUG
#define APP_TOUCH_UART_DEBUG  APP_SLAVE_USART1_DEBUG
#endif

#include "app_slave_diag.h"

#if (APP_TOUCH_UART_DEBUG != 0) && defined(APP_SLAVE_TOUCH_TRACE_LOG) && (APP_SLAVE_TOUCH_TRACE_LOG != 0)
#define TOUCH_LOG(...) SLAVE_DBG_LOG("[SLV][TOUCH] " __VA_ARGS__)
#else
#define TOUCH_LOG(...) ((void)0)
#endif

void app_touch_diag_log_init_once(void);

#endif
