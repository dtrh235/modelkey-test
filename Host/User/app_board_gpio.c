#include "app_board_gpio.h"

#include "./SYSTEM/sys/sys.h"
#include "app_config.h"
#include "stm32f4xx_hal.h"

/* 网表 PCB4_5: JDQ_IN -> U27.40(PE9) -> R5 -> Q1 -> RELAY1 线圈；高电平吸合 */
#define JDQ_GPIO_PORT   GPIOE
#define JDQ_GPIO_PIN    GPIO_PIN_9

#ifndef APP_RELAY_UNLOCK_MS
#define APP_RELAY_UNLOCK_MS  3000u
#endif

static uint32_t s_relay_off_at;

static void board_jdq_set(uint8_t on)
{
    HAL_GPIO_WritePin(JDQ_GPIO_PORT, JDQ_GPIO_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_default_gpio_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    gpio_init.Pin = GPIO_PIN_9 | GPIO_PIN_11;
#if (APP_WIFI_MODEM_WF24 != 0)
    /* WF24 KEY：上电即释放，避免浮空进下载/复位态导致模组发烫、UART2 无 AT */
    gpio_init.Pin |= GPIO_PIN_12;
#endif
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio_init);

    /* 默认拉低 JDQ_IN，继电器释放；仅开锁成功时再吸合 */
    board_jdq_set(0u);
    s_relay_off_at = 0u;
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
#if (APP_WIFI_MODEM_WF24 != 0)
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
#endif
}

void board_relay_unlock_pulse(void)
{
    board_jdq_set(1u);
    s_relay_off_at = HAL_GetTick() + APP_RELAY_UNLOCK_MS;
}

void board_relay_poll(void)
{
    if(s_relay_off_at == 0u) {
        return;
    }
    if((int32_t)(HAL_GetTick() - s_relay_off_at) >= 0) {
        board_jdq_set(0u);
        s_relay_off_at = 0u;
    }
}
