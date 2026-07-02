/**
 * @file test_main.c
 * @brief PCB4_5 全模块一次性自检（日志 RS485 H11 / USART6）
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "stm32f4xx_hal.h"

#include "test_usb_uart.h"
#include "test_fp.h"
#include "test_hw_all.h"

int main(void)
{
    HAL_Init();
    sys_stm32_clock_init(336, 8, 2, 7);
    delay_init(168);
    delay_ms(50);

    test_fp_board_init();
    test_fp_uart_early_init();
    test_usb_uart_init();
    test_hw_all_run_once();

    while(1) {
        test_hw_all_poll();
        test_usb_uart_poll();
    }
}
