/**
 * @file test_usb_uart.c
 * @brief 测试日志走 RS485 调试口（DI_485 / USART6）
 *
 * 网表 PCB4_5:
 *   DI_485 H11.4 -> U27.63 PC6 (USART6_TX)
 *   RO_485 H11.1 -> U27.64 PC7 (USART6_RX)
 *   EN_485 H11.2/3 -> U27.65 PC8 (DE，发送前拉高)
 *
 * 外接 USB-RS485 转串口，A/B 接 H11，115200 8N1。
 */

#include "test_usb_uart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "stm32f4xx_hal.h"

#define TEST_LOG_TX_BUF     160u
#define RS485_DE_GPIO_PORT  GPIOC
#define RS485_DE_GPIO_PIN   GPIO_PIN_8

static UART_HandleTypeDef s_log_uart;
static uint8_t s_log_uart_ok;

static void rs485_de_tx(void)
{
    HAL_GPIO_WritePin(RS485_DE_GPIO_PORT, RS485_DE_GPIO_PIN, GPIO_PIN_SET);
}

static void rs485_de_rx(void)
{
    HAL_GPIO_WritePin(RS485_DE_GPIO_PORT, RS485_DE_GPIO_PIN, GPIO_PIN_RESET);
}

static void test_log_hw_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = RS485_DE_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(RS485_DE_GPIO_PORT, &gpio);
    rs485_de_rx();

    s_log_uart.Instance = USART6;
    s_log_uart.Init.BaudRate = TEST_USB_UART_BAUD;
    s_log_uart.Init.WordLength = UART_WORDLENGTH_8B;
    s_log_uart.Init.StopBits = UART_STOPBITS_1;
    s_log_uart.Init.Parity = UART_PARITY_NONE;
    s_log_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_log_uart.Init.Mode = UART_MODE_TX_RX;

    s_log_uart_ok = (HAL_UART_Init(&s_log_uart) == HAL_OK) ? 1u : 0u;
}

static void test_log_tx(const uint8_t *data, uint16_t len)
{
    if(s_log_uart_ok == 0u || data == NULL || len == 0u) {
        return;
    }
    rs485_de_tx();
    (void)HAL_UART_Transmit(&s_log_uart, (uint8_t *)data, len, 500u);
    rs485_de_rx();
}

void test_usb_uart_print(const char *s)
{
    size_t n;

    if(s == NULL) {
        return;
    }
    n = strlen(s);
    if(n == 0u) {
        return;
    }
    if(n > 400u) {
        n = 400u;
    }
    test_log_tx((const uint8_t *)s, (uint16_t)n);
}

void test_usb_uart_printf(const char *fmt, ...)
{
    char buf[TEST_LOG_TX_BUF];
    va_list ap;
    int n;

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
        n = (int)(sizeof(buf) - 1);
        buf[n] = '\0';
    }
    test_usb_uart_print(buf);
}

void test_usb_uart_init(void)
{
    test_log_hw_init();

    test_usb_uart_print("\r\n========================================\r\n");
    test_usb_uart_print("[RS485-LOG] PCB4_5 ALL-MODULE self-test\r\n");
    test_usb_uart_printf("[RS485-LOG] USART6 %lu baud 8N1\r\n",
                         (unsigned long)TEST_USB_UART_BAUD);
    test_usb_uart_print("[RS485-LOG] PC6=DI_485(TX) PC7=RO_485(RX) PC8=DE\r\n");
    test_usb_uart_print("[RS485-LOG] USB-RS485 on H11 A/B, 115200 8N1\r\n");
    if(s_log_uart_ok != 0u) {
        test_usb_uart_print("[RS485-LOG] USART6 init OK\r\n");
        test_usb_uart_print("[RS485-LOG] type keys -> echo back\r\n");
    } else {
        test_usb_uart_print("[RS485-LOG] FAIL USART6 init\r\n");
    }
    test_usb_uart_print("========================================\r\n");
}

void test_usb_uart_poll(void)
{
    static uint32_t s_last_hb;
    uint32_t now;
    uint8_t byte;
    char echo[8];

    if(s_log_uart_ok == 0u) {
        return;
    }

    if(HAL_UART_Receive(&s_log_uart, &byte, 1u, 0u) == HAL_OK) {
        if(byte == '\r') {
            test_usb_uart_print("\r\n[ECHO] ");
        } else if(byte == '\n') {
            /* ignore bare LF after CR */
        } else if(byte >= 32u && byte < 127u) {
            echo[0] = (char)byte;
            echo[1] = '\0';
            test_usb_uart_print(echo);
        } else {
            test_usb_uart_printf("[ECHO] 0x%02X ", (unsigned)byte);
        }
    }

    now = HAL_GetTick();
    if(TEST_USB_UART_HEARTBEAT_MS != 0u &&
       (now - s_last_hb) >= TEST_USB_UART_HEARTBEAT_MS) {
        s_last_hb = now;
        test_usb_uart_printf("[RS485-LOG] alive ms=%lu\r\n", (unsigned long)now);
    }
}
