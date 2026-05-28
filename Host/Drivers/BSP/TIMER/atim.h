/**
 ****************************************************************************************************
 * @file        atim.h
 * @author      小丁
 * @version     V1.0
 * @date        2023-04-23
 * @brief       高级定时器驱动代码
 * @license     Copyright (c) 2020-2032, 
 ****************************************************************************************************
 * @attention
 * 
 * 实验开发板:DRG ST-1 STM32F407VET6核心板
 * 
 *
 * 公司网址:www.genbotter.com
 * 购买地址:makerbase.taobao.com
 * 
 ****************************************************************************************************
 */

#ifndef __ATIM_H
#define __ATIM_H

#include "./SYSTEM/sys/sys.h"

/* 高级定时器定义 */
#define ATIM_TIMX_NPWM                          TIM8
#define ATIM_TIMX_NPWM_IRQn                     TIM8_UP_TIM13_IRQn
#define ATIM_TIMX_NPWM_IRQHandler               TIM8_UP_TIM13_IRQHandler
#define ATIM_TIMX_NPWM_CLK_ENABLE()             do { __HAL_RCC_TIM8_CLK_ENABLE(); } while (0)
#define ATIM_TIMX_NPWM_CHY                      TIM_CHANNEL_1
#define ATIM_TIMX_NPWM_CHY_GPIO_PORT            GPIOC
#define ATIM_TIMX_NPWM_CHY_GPIO_PIN             GPIO_PIN_6
#define ATIM_TIMX_NPWM_CHY_GPIO_AF              GPIO_AF3_TIM8
#define ATIM_TIMX_NPWM_CHY_GPIO_CLK_ENABLE()    do { __HAL_RCC_GPIOC_CLK_ENABLE(); } while (0)

#define ATIM_TIMX_COMP                          TIM8
#define ATIM_TIMX_COMP_CLK_ENABLE()             do { __HAL_RCC_TIM8_CLK_ENABLE(); } while (0)
#define ATIM_TIMX_COMP_CH1_GPIO_PORT            GPIOC
#define ATIM_TIMX_COMP_CH1_GPIO_PIN             GPIO_PIN_6
#define ATIM_TIMX_COMP_CH1_GPIO_AF              GPIO_AF3_TIM8
#define ATIM_TIMX_COMP_CH1_GPIO_CLK_ENABLE()    do { __HAL_RCC_GPIOC_CLK_ENABLE(); } while (0)
#define ATIM_TIMX_COMP_CH2_GPIO_PORT            GPIOC
#define ATIM_TIMX_COMP_CH2_GPIO_PIN             GPIO_PIN_7
#define ATIM_TIMX_COMP_CH2_GPIO_AF              GPIO_AF3_TIM8
#define ATIM_TIMX_COMP_CH2_GPIO_CLK_ENABLE()    do { __HAL_RCC_GPIOC_CLK_ENABLE(); } while (0)

#define ATIM_TIMX_CPLM                          TIM1
#define ATIM_TIMX_CPLM_CLK_ENABLE()             do { __HAL_RCC_TIM1_CLK_ENABLE(); } while (0)
#define ATIM_TIMX_CPLM_CHY                      TIM_CHANNEL_1
#define ATIM_TIMX_CPLM_CHY_GPIO_PORT            GPIOA
#define ATIM_TIMX_CPLM_CHY_GPIO_PIN             GPIO_PIN_8
#define ATIM_TIMX_CPLM_CHY_GPIO_AF              GPIO_AF1_TIM1
#define ATIM_TIMX_CPLM_CHY_GPIO_CLK_ENABLE()    do { __HAL_RCC_GPIOA_CLK_ENABLE(); } while (0)
#define ATIM_TIMX_CPLM_CHYN_GPIO_PORT           GPIOA
#define ATIM_TIMX_CPLM_CHYN_GPIO_PIN            GPIO_PIN_7
#define ATIM_TIMX_CPLM_CHYN_GPIO_AF             GPIO_AF1_TIM1
#define ATIM_TIMX_CPLM_CHYN_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOA_CLK_ENABLE(); } while (0)
#define ATIM_TIMX_CPLM_BKIN_GPIO_PORT           GPIOA
#define ATIM_TIMX_CPLM_BKIN_GPIO_PIN            GPIO_PIN_6
#define ATIM_TIMX_CPLM_BKIN_GPIO_AF             GPIO_AF1_TIM1
#define ATIM_TIMX_CPLM_BKIN_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOA_CLK_ENABLE(); } while (0)

#define ATIM_TIMX_PWMIN                         TIM8
#define ATIM_TIMX_PWMIN_IRQn                    TIM8_CC_IRQn
#define ATIM_TIMX_PWMIN_IRQHandler              TIM8_CC_IRQHandler
#define ATIM_TIMX_PWMIN_CLK_ENABLE()            do { __HAL_RCC_TIM8_CLK_ENABLE(); } while (0)
#define ATIM_TIMX_PWMIN_CHY                     TIM_CHANNEL_1
#define ATIM_TIMX_PWMIN_CHY_GPIO_PORT           GPIOC
#define ATIM_TIMX_PWMIN_CHY_GPIO_PIN            GPIO_PIN_6
#define ATIM_TIMX_PWMIN_CHY_GPIO_AF             GPIO_AF3_TIM8
#define ATIM_TIMX_PWMIN_CHY_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOC_CLK_ENABLE(); } while (0)

/* 函数声明 */
void atim_timx_chy_npwm_init(uint16_t arr, uint16_t psc);   /* 初始化高级定时器输出指定个数PWM */
void atim_timx_chy_npwm_set(uint32_t npwm);                 /* 设置高级定时器输出指定个数PWM */
void atim_timx_comp_init(uint16_t arr, uint16_t psc);       /* 初始化高级定时器输出比较 */
void atim_timx_cplm_init(uint16_t arr, uint16_t psc);       /* 初始化高级定时器互补输出带死区控制 */
void atim_timx_cplm_set(uint16_t ccr, uint8_t dtg);         /* 设置高级定时器互补输出带死区控制 */
void atim_timx_pwmin_chy_init(uint16_t psc);                /* 初始化高级定时器PWM输入 */

#endif
