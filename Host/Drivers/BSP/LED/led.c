/**
 ****************************************************************************************************
 * @file        led.c
 * @author      genbotter
 * @version     V1.0
 * @date        2023-04-23
 * @brief       LED驱动代码
 * @license     Copyright (c) 2020-2032, 
 ****************************************************************************************************
 * @attention
 * 
 * 
 * 适配开发板:DRG ST-1 STM32F407VET6开发板
 * 购买网址：https://makerbase.taobao.com/
 * 
 * 
 ****************************************************************************************************
 */

#include "./BSP/LED/led.h"

/**
 * @brief   初始化LED
 * @param   无
 * @retval  无
 */
void led_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};
    
    /* 使能GPIO端口时钟 */
    LED2_GPIO_CLK_ENABLE();
    LED3_GPIO_CLK_ENABLE();
    
    /* 配置LED0控制引脚 */
    gpio_init_struct.Pin = LED2_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLDOWN;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED2_GPIO_PORT, &gpio_init_struct);
    
    /* 配置LED1控制引脚 */
    gpio_init_struct.Pin = LED3_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLDOWN;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED3_GPIO_PORT, &gpio_init_struct);
    
    /* 关闭LED0、LED1 */
    LED2(1);
    LED3(1);
}
