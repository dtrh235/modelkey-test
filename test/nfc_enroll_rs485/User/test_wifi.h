#ifndef TEST_WIFI_H
#define TEST_WIFI_H

#include <stdint.h>

void test_wifi_init(void);
void test_wifi_poll(void);
/** 上电尽早调用：释放 WF24 PE12(KEY)，与 Host board_default_gpio_init 一致 */
void test_wifi_board_early_gpio(void);
/** 一次性：KEY 复位 + 单次 AT，返回 1=收到 OK */
uint8_t test_wifi_run_boot_once(void);

#endif /* TEST_WIFI_H */
