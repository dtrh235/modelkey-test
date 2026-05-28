#include "app_board_gpio.h"

#include "./SYSTEM/sys/sys.h"

void board_default_gpio_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    gpio_init.Pin = GPIO_PIN_9 | GPIO_PIN_11;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio_init);

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9 | GPIO_PIN_11, GPIO_PIN_SET);
}
