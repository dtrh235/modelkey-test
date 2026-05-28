#ifndef APP_UNLOCK_UART4_H
#define APP_UNLOCK_UART4_H

#include <stdint.h>

/* 开锁成功提示音：主从工程相同——UART4 PC10(TX) 发单字节 ASCII '1'，波特率=APP_RS485_UART_BAUD(115200)。
 * 外接语音/功放板若接此脚，只有走到 app_unlock_event_handle_success 才会发声；从机 NFC 未匹配则不会发。 */
void app_unlock_uart4_init(void);
void app_unlock_uart4_poll(void);
void app_unlock_uart4_on_unlock_ok(const char *account, const char *method);

#endif
