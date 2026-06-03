#ifndef APP_HOST_DIAG_H
#define APP_HOST_DIAG_H

#include "app_config.h"
#include <stdio.h>
#include "SYSTEM/usart/usart.h"

/* 调试口：默认 USART6 PC6(TX)/PC7(RX)；APP_DEBUG_ON_USART6=0 时为 PA9/PA10。AS608=USART3 PB10/PB11。 */
#ifndef APP_HOST_NAV_DIAG
#define APP_HOST_NAV_DIAG 0
#endif

#if (APP_HOST_NAV_DIAG != 0)
#define NAV_LOG(...) do { \
        char _nav_buf[160]; \
        (void)snprintf(_nav_buf, sizeof(_nav_buf), __VA_ARGS__); \
        usart_debug_tx_str(_nav_buf); \
    } while(0)
#else
#define NAV_LOG(...) ((void)0)
#endif

#if defined(APP_HOST_RS485_DIAG) && (APP_HOST_RS485_DIAG != 0)
#define HOST_RS485_LOG(...) do { \
        char _host_diag_buf[200]; \
        (void)snprintf(_host_diag_buf, sizeof(_host_diag_buf), __VA_ARGS__); \
        usart_debug_tx_str(_host_diag_buf); \
    } while(0)
#if (APP_RS485_IS_MASTER) && defined(APP_HOST_RS485_LOG_VERBOSE) && (APP_HOST_RS485_LOG_VERBOSE != 0)
#define HOST_RS485_LOG_VERBOSE(...) HOST_RS485_LOG(__VA_ARGS__)
#else
#define HOST_RS485_LOG_VERBOSE(...) ((void)0)
#endif
#else
#define HOST_RS485_LOG(...) ((void)0)
#define HOST_RS485_LOG_VERBOSE(...) ((void)0)
#endif

#ifndef APP_HOST_SLAVE_UNLOCK_CLOUD_TRACE
#define APP_HOST_SLAVE_UNLOCK_CLOUD_TRACE 0
#endif
#if (APP_HOST_SLAVE_UNLOCK_CLOUD_TRACE != 0)
#define HOST_UNLOCK_CLOUD_LOG(...) do { \
        char _host_uc_buf[200]; \
        (void)snprintf(_host_uc_buf, sizeof(_host_uc_buf), __VA_ARGS__); \
        usart_debug_tx_str(_host_uc_buf); \
    } while(0)
#else
#define HOST_UNLOCK_CLOUD_LOG(...) ((void)0)
#endif

#endif /* APP_HOST_DIAG_H */
