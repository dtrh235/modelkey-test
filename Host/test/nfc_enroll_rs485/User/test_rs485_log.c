#include "test_rs485_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_rs485_link.h"

#define TEST_LOG_BUF_SZ  192u

void test_rs485_log_init(void)
{
    app_rs485_link_init();
}

void test_rs485_log_str(const char *s)
{
    uint16_t n;

    if(s == NULL || app_rs485_link_ready() == 0u) {
        return;
    }
    n = (uint16_t)strlen(s);
    if(n == 0u) {
        return;
    }
    (void)app_rs485_raw_send((const uint8_t *)s, n, 500u);
}

void test_rs485_logf(const char *fmt, ...)
{
    char buf[TEST_LOG_BUF_SZ];
    va_list ap;
    int n;

    if(fmt == NULL || app_rs485_link_ready() == 0u) {
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
    test_rs485_log_str(buf);
}
