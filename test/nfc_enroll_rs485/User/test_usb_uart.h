#ifndef TEST_USB_UART_H
#define TEST_USB_UART_H

#include <stdint.h>

void test_usb_uart_init(void);
void test_usb_uart_poll(void);
void test_usb_uart_print(const char *s);
void test_usb_uart_printf(const char *fmt, ...);

#endif /* TEST_USB_UART_H */
