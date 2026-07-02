#ifndef APP_HOST_HW_SELFTEST_H
#define APP_HOST_HW_SELFTEST_H

/* 上电串口外设自检：USART1 PA9 115200，每项打印 [HW] xxx OK / FAIL */

void app_host_hw_selftest_run_pre_ui(void);
void app_host_hw_selftest_run_post_lcd(void);
void app_host_hw_selftest_run_deferred(void);

#endif
