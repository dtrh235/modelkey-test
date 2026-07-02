#include "test_rs485_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

#if (TEST_HW_LOG_USE_USB != 0)
#include "test_usb_uart.h"
#endif

#define TEST_LOG_BUF_SZ  192u

void test_rs485_log_init(void)
{
}

void test_rs485_log_str(const char *s)
{
#if (TEST_HW_LOG_USE_USB != 0)
    if(s != NULL) {
        test_usb_uart_print(s);
    }
#else
    (void)s;
#endif
}

void test_rs485_logf(const char *fmt, ...)
{
    char buf[TEST_LOG_BUF_SZ];
    va_list ap;
    int n;

#if (TEST_HW_LOG_USE_USB != 0)
    if(fmt == NULL) {
        return;
    }
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if(n <= 0) {
        return;
    }
    if((size_t)n >= sizeof(buf)) {
        n = (int)(sizeof(buf) - 1u);
        buf[n] = '\0';
    }
    test_usb_uart_print(buf);
#else
    (void)fmt;
#endif
}
