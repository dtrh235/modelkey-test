#ifndef APP_WIFI_DIAG_H
#define APP_WIFI_DIAG_H

#include "app_config.h"

void app_wifi_diag_init(void);
void app_wifi_diag_log(const char *fmt, ...);
void app_cloud_diag_log(const char *fmt, ...);
void app_remember_diag_log(const char *fmt, ...);

#if (APP_WIFI_UART_DEBUG != 0)
#define WIFI_DBG(...) app_wifi_diag_log(__VA_ARGS__)
#else
#define WIFI_DBG(...) ((void)0)
#endif

#if (APP_CLOUD_UART_DEBUG != 0)
#define CLOUD_DBG(...) app_cloud_diag_log(__VA_ARGS__)
#define REMEMBER_DBG(...) app_remember_diag_log(__VA_ARGS__)
#else
#define CLOUD_DBG(...) ((void)0)
#define REMEMBER_DBG(...) ((void)0)
#endif

#endif
