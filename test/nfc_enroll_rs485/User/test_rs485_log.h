#ifndef TEST_RS485_LOG_H
#define TEST_RS485_LOG_H

#include <stdint.h>

void test_rs485_log_init(void);
void test_rs485_log_str(const char *s);
void test_rs485_logf(const char *fmt, ...);

#endif /* TEST_RS485_LOG_H */
