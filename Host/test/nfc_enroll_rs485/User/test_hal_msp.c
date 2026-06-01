/**
 * @file test_hal_msp.c
 * @brief 测试工程 USART6(RS485) MSP，避免链接完整 usart.c / AS608
 */

#include "stm32f4xx_hal.h"

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if(huart->Instance != USART6) {
        return;
    }

    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &gpio);

    /* DE/RE：与 app_rs485_link 一致，PC8 在 link_init 中再配为输出 */
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if(huart->Instance != USART6) {
        return;
    }
    __HAL_RCC_USART6_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6 | GPIO_PIN_7);
}
