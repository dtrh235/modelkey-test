#ifndef APP_WIFI_DIAG_H
#define APP_WIFI_DIAG_H

#include "app_config.h"
#include "SYSTEM/usart/usart.h"

void app_wifi_diag_init(void);
void app_wifi_trace_stage(const char *stage);
void app_wifi_diag_log(const char *fmt, ...);
void app_cloud_diag_log(const char *fmt, ...);
void app_remember_diag_log(const char *fmt, ...);

#ifndef APP_LOG_ESSENTIAL
#define APP_LOG_ESSENTIAL 0
#endif
#if (APP_LOG_ESSENTIAL != 0)
#define LOG_ESSENTIAL(s)  usart_debug_tx_str(s)
#else
#define LOG_ESSENTIAL(s)  ((void)0)
#endif

#if (APP_WIFI_UART_DEBUG != 0)
#define WIFI_DBG(...) app_wifi_diag_log(__VA_ARGS__)
#else
#define WIFI_DBG(...) ((void)0)
#endif

#if (APP_WIFI_UART_DEBUG != 0) || (APP_WIFI_CWJAP_TRACE != 0)
#define WIFI_TRACE(s) app_wifi_trace_stage(s)
#else
#define WIFI_TRACE(s) ((void)0)
#endif

#if (APP_CLOUD_UART_DEBUG != 0)
#define CLOUD_DBG(...) app_cloud_diag_log(__VA_ARGS__)
#define REMEMBER_DBG(...) app_remember_diag_log(__VA_ARGS__)
#else
#define CLOUD_DBG(...) ((void)0)
#define REMEMBER_DBG(...) ((void)0)
#endif

#if (APP_TIME_TRACE != 0)
void time_trace_tx(const char *s);
#define TIME_TRACE_MSG(s)   time_trace_tx(s)
#else
#define TIME_TRACE_MSG(s)   ((void)0)
#endif
#if (APP_CLOUD_TRACE != 0) || (APP_HOST_SLAVE_UNLOCK_CLOUD_TRACE != 0)
void cloud_trace_tx(const char *s);
#endif
#if (APP_CLOUD_TRACE != 0)
#define CLOUD_TRACE_MSG(s)  cloud_trace_tx(s)
#else
#define CLOUD_TRACE_MSG(s)  ((void)0)
#endif
#ifndef APP_HOST_SLAVE_UNLOCK_CLOUD_TRACE
#define APP_HOST_SLAVE_UNLOCK_CLOUD_TRACE 0
#endif
#if (APP_HOST_SLAVE_UNLOCK_CLOUD_TRACE != 0)
#define UNLOCK_CLOUD_TRACE_MSG(s)  cloud_trace_tx(s)
#else
#define UNLOCK_CLOUD_TRACE_MSG(s)  ((void)0)
#endif

#endif
