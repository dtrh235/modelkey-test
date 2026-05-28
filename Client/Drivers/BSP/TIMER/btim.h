/**
 ****************************************************************************************************
 * @file        btim.h
 * @author      小丁
 * @version     V1.0
 * @date        2023-04-23
 * @brief       基本定时器驱动代码
 ****************************************************************************************************
 * @attention
 * 
 * 适配开发板:DRG STM32F407VET6开发板
 * 购买网址：https://makerbase.taobao.com/
 * 
 ****************************************************************************************************
 */

#ifndef __BTIM_H
#define __BTIM_H

#include "./SYSTEM/sys/sys.h"

/* 基本定时器定义 */
#define BTIM_TIMX_INT               TIM6
#define BTIM_TIMX_INT_IRQn          TIM6_DAC_IRQn
#define BTIM_TIMX_INT_IRQHandler    TIM6_DAC_IRQHandler
#define BTIM_TIMX_INT_CLK_ENABLE()  do { __HAL_RCC_TIM6_CLK_ENABLE(); } while (0)

/* 函数声明 */
void btim_timx_int_init(uint16_t arr, uint16_t psc);    /* 初始化基本定时器 */

#endif
