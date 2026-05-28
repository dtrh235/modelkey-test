#include "app_wifi_diag.h"

#include "app_config.h"
#include "SYSTEM/usart/usart.h"

#include <stdio.h>
#include <stdarg.h>

void app_wifi_diag_init(void)
{
}

void app_wifi_diag_log(const char *fmt, ...)
{
#if (APP_WIFI_UART_DEBUG == 0)
    (void)fmt;
    return;
#else
    char buf[200];
    int n;
    va_list ap;

    if(fmt == NULL) {
        return;
    }
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf) - 3, fmt, ap);
    va_end(ap);
    if(n < 0) {
        n = 0;
    }
    if((size_t)n > sizeof(buf) - 3) {
        n = (int)(sizeof(buf) - 3);
    }
    buf[n++] = '\r';
    buf[n++] = '\n';
    buf[n] = '\0';
    usart_debug_tx_str("[WiFi] ");
    usart_debug_tx_str(buf);
#endif
}

void app_cloud_diag_log(const char *fmt, ...)
{
#if (APP_CLOUD_UART_DEBUG == 0) && (APP_WIFI_UART_DEBUG == 0)
    (void)fmt;
    return;
#else
    char buf[200];
    int n;
    va_list ap;

    if(fmt == NULL) {
        return;
    }
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf) - 3, fmt, ap);
    va_end(ap);
    if(n < 0) {
        n = 0;
    }
    if((size_t)n > sizeof(buf) - 3) {
        n = (int)(sizeof(buf) - 3);
    }
    buf[n++] = '\r';
    buf[n++] = '\n';
    buf[n] = '\0';
    usart_debug_tx_str("[ALIYUN] ");
    usart_debug_tx_str(buf);
#endif
}

void app_remember_diag_log(const char *fmt, ...)
{
#if (APP_CLOUD_UART_DEBUG == 0)
    (void)fmt;
    return;
#else
    char buf[200];
    int n;
    va_list ap;

    if(fmt == NULL) {
        return;
    }
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf) - 3, fmt, ap);
    va_end(ap);
    if(n < 0) {
        n = 0;
    }
    if((size_t)n > sizeof(buf) - 3) {
        n = (int)(sizeof(buf) - 3);
    }
    buf[n++] = '\r';
    buf[n++] = '\n';
    buf[n] = '\0';
    usart_debug_tx_str("[REMEMBER] ");
    usart_debug_tx_str(buf);
#endif
}
