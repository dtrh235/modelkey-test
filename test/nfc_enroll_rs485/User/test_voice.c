/**
 * @file test_voice.c
 * @brief ASRPRO 语音模块 U34 自检：UART4 + 喇叭 SPK
 *
 * 网表 PCB4_5:
 *   UART4_TX U27.78 (PC10) -> U34.15
 *   UART4_RX U27.79 (PC11) <- U34.16
 *   SPK+/SPK- -> H3 -> U34.8/9
 *
 * 与 Host 主工程一致：115200 8N1，周期发送 ASCII '1' 触发播报（开锁成功同款命令）。
 */

#include "test_voice.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "test_usb_uart.h"
#include "stm32f4xx_hal.h"

static UART_HandleTypeDef s_voice_uart;
static uint8_t s_voice_ok;
static uint32_t s_last_tx;
static uint32_t s_tx_count;

/* 启英泰伦系播报 ID=1：A5 FA 00 03 01 00 FD FB（备用，部分固件支持） */
static const uint8_t s_ci_play_id1[] = { 0xA5u, 0xFAu, 0x00u, 0x03u, 0x01u, 0x00u, 0xFDu, 0xFBu };

static void test_voice_hw_init(void)
{
    s_voice_uart.Instance = UART4;
    s_voice_uart.Init.BaudRate = TEST_VOICE_UART_BAUD;
    s_voice_uart.Init.WordLength = UART_WORDLENGTH_8B;
    s_voice_uart.Init.StopBits = UART_STOPBITS_1;
    s_voice_uart.Init.Parity = UART_PARITY_NONE;
    s_voice_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_voice_uart.Init.Mode = UART_MODE_TX_RX;

    s_voice_ok = (HAL_UART_Init(&s_voice_uart) == HAL_OK) ? 1u : 0u;
}

static void test_voice_log_rx(void)
{
    uint8_t buf[32];
    uint16_t n = 0u;
    uint8_t b;

    while(HAL_UART_Receive(&s_voice_uart, &b, 1u, 0u) == HAL_OK) {
        if(n < sizeof(buf)) {
            buf[n++] = b;
        }
    }
    if(n == 0u) {
        return;
    }

    test_usb_uart_print("[VOICE] RX ");
    for(uint16_t i = 0u; i < n; i++) {
        char line[8];
        (void)snprintf(line, sizeof(line), "%02X ", (unsigned)buf[i]);
        test_usb_uart_print(line);
    }
    test_usb_uart_print("\r\n");
}

void test_voice_init(void)
{
    s_last_tx = HAL_GetTick();
    s_tx_count = 0u;

    test_voice_hw_init();

    test_usb_uart_print("\r\n========================================\r\n");
    test_usb_uart_print("[VOICE] ASRPRO U34 speaker test\r\n");
    test_usb_uart_print("[VOICE] UART4 PC10=TX PC11=RX -> U34\r\n");
    test_usb_uart_print("[VOICE] SPK+/- -> H3 (listen to speaker)\r\n");
    test_usb_uart_printf("[VOICE] baud=%lu 8N1\r\n", (unsigned long)TEST_VOICE_UART_BAUD);
    if(s_voice_ok != 0u) {
        test_usb_uart_print("[VOICE] UART4 init OK\r\n");
        test_usb_uart_print("[VOICE] every 3s: send '1' (Host unlock voice cmd)\r\n");
        test_usb_uart_print("[VOICE] expect: speaker plays voice\r\n");
    } else {
        test_usb_uart_print("[VOICE] FAIL UART4 init\r\n");
    }
    test_usb_uart_print("========================================\r\n");
}

void test_voice_poll(void)
{
    uint32_t now;

    if(s_voice_ok == 0u) {
        return;
    }

    test_voice_log_rx();

    now = HAL_GetTick();
    if((now - s_last_tx) < TEST_VOICE_TRIGGER_MS) {
        return;
    }
    s_last_tx = now;
    s_tx_count++;

    if((s_tx_count % 4u) == 0u) {
        (void)HAL_UART_Transmit(&s_voice_uart, (uint8_t *)s_ci_play_id1,
                                (uint16_t)sizeof(s_ci_play_id1), 100u);
        test_usb_uart_printf("[VOICE] TX #%lu CI play id=1 (binary)\r\n",
                             (unsigned long)s_tx_count);
    } else {
        uint8_t ch = (uint8_t)'1';
        (void)HAL_UART_Transmit(&s_voice_uart, &ch, 1u, 50u);
        test_usb_uart_printf("[VOICE] TX #%lu ASCII '1' (unlock cmd)\r\n",
                             (unsigned long)s_tx_count);
    }
}
