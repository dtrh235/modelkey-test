#ifndef APP_TOUCH_DIAG_H
#define APP_TOUCH_DIAG_H

#include "app_config.h"
#include <stdio.h>
#include "SYSTEM/usart/usart.h"

/* ??????�USART1 PA9(TX)/PA10(RX)???? APP_DEBUG_ON_USART6=0 */
#ifndef APP_TOUCH_UART_DEBUG
#define APP_TOUCH_UART_DEBUG  0
#endif

#if (APP_TOUCH_UART_DEBUG != 0)
#define TOUCH_LOG(...) do { \
        char _tpbuf[256]; \
        (void)snprintf(_tpbuf, sizeof(_tpbuf), __VA_ARGS__); \
        usart_debug_tx_str(_tpbuf); \
    } while(0)
#else
#define TOUCH_LOG(...) ((void)0)
#endif

void app_touch_diag_log_init_once(void);

#endif
