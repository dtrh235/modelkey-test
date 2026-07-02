/**
 * @file test_relay.c
 * @brief 继电器 RELAY1 + 磁吸锁 H9 硬件自检（万用表测 12V）
 *
 * 网表 PCB4_5:
 *   JDQ_IN -> U27.40(PE9) -> R5 -> Q1(S8050) -> RELAY1 线圈
 *   触点 RELAY1.2 -> H9.1；H9.2 -> GND
 *   线圈/触点电源侧 VCCBAT（板载 12V 输入经 DC1 后）
 *
 * 高电平 JDQ_IN = 吸合继电器，H9 两端应有 ~12V（需外接 12V 电源）。
 */

#include "test_relay.h"

#include "app_config.h"
#include "test_usb_uart.h"
#include "stm32f4xx_hal.h"

#define RELAY_GPIO_PORT   GPIOE
#define RELAY_GPIO_PIN    GPIO_PIN_9

static uint8_t s_relay_on;
static uint32_t s_last_toggle;
static uint32_t s_cycle;

static void relay_set(uint8_t on)
{
    s_relay_on = on;
    HAL_GPIO_WritePin(RELAY_GPIO_PORT, RELAY_GPIO_PIN,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void test_relay_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

    gpio.Pin = RELAY_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RELAY_GPIO_PORT, &gpio);

    relay_set(0u);
    s_last_toggle = HAL_GetTick();
    s_cycle = 0u;

    test_usb_uart_print("\r\n========================================\r\n");
    test_usb_uart_print("[RELAY] magnetic lock / RELAY1 test\r\n");
    test_usb_uart_print("[RELAY] net: JDQ_IN -> U27.40(PE9) -> Q1 -> RELAY1\r\n");
    test_usb_uart_print("[RELAY] output: H9 (2-pin), expect ~12V when ON\r\n");
    test_usb_uart_printf("[RELAY] pattern: ON %ums / OFF %ums\r\n",
                         (unsigned)TEST_RELAY_ON_MS,
                         (unsigned)TEST_RELAY_OFF_MS);
    test_usb_uart_print("[RELAY] need 12V on DC jack (VCCBAT); LED9 on when relay ON\r\n");
    test_usb_uart_print("[RELAY] DMM: H9 pin1 vs pin2 -> ~12V ON, ~0V OFF\r\n");
    test_usb_uart_print("========================================\r\n");
}

void test_relay_poll(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t interval = s_relay_on ? TEST_RELAY_ON_MS : TEST_RELAY_OFF_MS;

    if((now - s_last_toggle) < interval) {
        return;
    }
    s_last_toggle = now;

    if(s_relay_on == 0u) {
        relay_set(1u);
        test_usb_uart_print("[RELAY] ON  -> measure H9 ~12V (LED9 should light)\r\n");
    } else {
        relay_set(0u);
        s_cycle++;
        test_usb_uart_printf("[RELAY] OFF -> cycle #%lu (H9 ~0V)\r\n",
                             (unsigned long)s_cycle);
    }
}
