/**
 ****************************************************************************************************
 * @file        led.h
 * @author      genbotter
 * @version     V1.0
 * @date        2023-04-23
 * @brief       LED驱动代码
 * @license     Copyright (c) 2020-2032, 
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:ST-1 STM32F407核心板
 * 
 * 
 * 
 * 购买链接：https://item.taobao.com/item.htm?spm=a21n57.1.0.0.4d53523chbY4Wg&id=730217214783&ns=1&abbucket=12#detail
 * 
 ****************************************************************************************************
 */

#ifndef __LED_H
#define __LED_H

#include "./SYSTEM/sys/sys.h"

/* 引脚定义 */
#define LED2_GPIO_PORT          GPIOA
#define LED2_GPIO_PIN           GPIO_PIN_6
#define LED2_GPIO_CLK_ENABLE()  do { __HAL_RCC_GPIOF_CLK_ENABLE(); } while (0)
#define LED3_GPIO_PORT          GPIOA
#define LED3_GPIO_PIN           GPIO_PIN_7
#define LED3_GPIO_CLK_ENABLE()  do { __HAL_RCC_GPIOF_CLK_ENABLE(); } while (0)

/* IO操作 */
#define LED2(x)                 do { (x) ?                                                              \
                                    HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_SET):     \
                                    HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_RESET);   \
                                } while (0)
#define LED3(x)                 do { (x) ?                                                              \
                                    HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_GPIO_PIN, GPIO_PIN_SET):     \
                                    HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_GPIO_PIN, GPIO_PIN_RESET);   \
                                } while (0)
#define LED2_TOGGLE()           do { HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_GPIO_PIN); } while (0)
#define LED3_TOGGLE()           do { HAL_GPIO_TogglePin(LED3_GPIO_PORT, LED3_GPIO_PIN); } while (0)

/* 函数声明 */
void led_init(void);    /* 初始化LED */

#endif
