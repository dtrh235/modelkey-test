/**
 * @file test_buzzer.c
 * @brief 有源蜂鸣器 BUZZER2 自检（网表 BEEP -> U27.42 = PC9，经 R13/Q9 驱动）
 */

#include "test_buzzer.h"

#include "app_config.h"
#include "test_usb_uart.h"
#include "stm32f4xx_hal.h"

#define BEEP_GPIO_PORT   GPIOC
#define BEEP_GPIO_PIN    GPIO_PIN_9

static uint8_t s_beep_on;
static uint32_t s_last_toggle;
static uint32_t s_cycle;

static void beep_set(uint8_t on)
{
    s_beep_on = on;
    HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void test_buzzer_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin = BEEP_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BEEP_GPIO_PORT, &gpio);

    beep_set(0u);
    s_last_toggle = HAL_GetTick();
    s_cycle = 0u;

    test_usb_uart_print("\r\n========================================\r\n");
    test_usb_uart_print("[BEEP] BUZZER2 hardware test\r\n");
    test_usb_uart_print("[BEEP] net: BEEP -> U27.42(PC9) -> R13 -> Q9 -> BUZZER2\r\n");
    test_usb_uart_printf("[BEEP] pattern: ON %ums / OFF %ums\r\n",
                         (unsigned)TEST_BUZZER_ON_MS,
                         (unsigned)TEST_BUZZER_OFF_MS);
    test_usb_uart_print("[BEEP] expect: short beep every ~1s\r\n");
    test_usb_uart_print("========================================\r\n");
}

void test_buzzer_poll(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t interval = s_beep_on ? TEST_BUZZER_ON_MS : TEST_BUZZER_OFF_MS;

    if((now - s_last_toggle) < interval) {
        return;
    }
    s_last_toggle = now;

    if(s_beep_on == 0u) {
        beep_set(1u);
    } else {
        beep_set(0u);
        s_cycle++;
        test_usb_uart_printf("[BEEP] cycle #%lu (heard beep?)\r\n",
                             (unsigned long)s_cycle);
    }
}
