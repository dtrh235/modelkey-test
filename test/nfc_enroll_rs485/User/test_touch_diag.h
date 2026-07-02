#ifndef TEST_TOUCH_DIAG_H
#define TEST_TOUCH_DIAG_H

#include <stdint.h>

void test_touch_diag_run_once(void);
void test_touch_diag_poll(void);
/* 1=I2C 探测通过（可触摸或 id 异常但 ACK 正常） */
uint8_t test_touch_diag_hw_ok(void);

#endif /* TEST_TOUCH_DIAG_H */
