#ifndef TEST_FP_H
#define TEST_FP_H

#include <stdint.h>

/* 与 Host main 相同：在 RS485/其它 UART 之前初始化 USART3 */
void test_fp_board_init(void);
void test_fp_uart_early_init(void);
void test_fp_init(void);
void test_fp_poll(void);
uint8_t test_fp_hw_ok(void);

#endif /* TEST_FP_H */
