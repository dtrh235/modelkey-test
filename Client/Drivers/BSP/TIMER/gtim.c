/**
 ****************************************************************************************************
 * @file        gtim.c
 * @author      小丁
 * @version     V1.0
 * @date        2023-04-23
 * @brief       通用定时器驱动代码
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

#include "./BSP/TIMER/gtim.h"
#include "./BSP/LED/led.h"

/* 通用定时器句柄 */
TIM_HandleTypeDef g_timx_int_handle = {0};
TIM_HandleTypeDef g_timx_pwm_handle = {0};
TIM_HandleTypeDef g_timx_cap_handle = {0};
TIM_HandleTypeDef g_timx_cnt_handle = {0};

/* 输入捕获定时器相关变量 */
uint8_t g_timx_chy_cap_sta = 0;     /* [7]: 捕获完成 [6]: 捕获到高电平 [5:0]: 捕获计数值溢出计数器 */
uint16_t g_timx_chy_cap_val = 0;    /* 捕获的计数值 */

/* 脉冲计数定时器相关变量 */
uint32_t g_timx_chy_cnt_ofcnt = 0;  /* 脉冲计数定时器溢出计数器 */

/**
 * @brief   初始化通用定时器中断
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void gtim_timx_int_init(uint16_t arr, uint16_t psc)
{
    g_timx_int_handle.Instance = GTIM_TIMX_INT;                 /* 定时器 */
    g_timx_int_handle.Init.Prescaler = psc;                     /* 预分频系数 */
    g_timx_int_handle.Init.CounterMode = TIM_COUNTERMODE_UP;    /* 计数模式 */
    g_timx_int_handle.Init.Period = arr;                        /* 重装载值 */
    HAL_TIM_Base_Init(&g_timx_int_handle);                      /* 定时器计数初始化 */
    HAL_TIM_Base_Start_IT(&g_timx_int_handle);                  /* 开启定时器计数和中断 */
}

/**
 * @brief   初始化通用定时器PWM
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void gtim_timx_pwm_chy_init(uint16_t arr, uint16_t psc)
{
    TIM_OC_InitTypeDef timx_oc_pwm_struct = {0};
    
    g_timx_pwm_handle.Instance = GTIM_TIMX_PWM;                                             /* 定时器 */
    g_timx_pwm_handle.Init.Prescaler = psc;                                                 /* 预分频器系数 */
    g_timx_pwm_handle.Init.CounterMode = TIM_COUNTERMODE_UP;                                /* 计数模式 */
    g_timx_pwm_handle.Init.Period = arr;                                                    /* 重装载值 */
    HAL_TIM_PWM_Init(&g_timx_pwm_handle);                                                   /* 定时器PWM初始化 */
    
    timx_oc_pwm_struct.OCMode = TIM_OCMODE_PWM1;                                            /* 输出比较模式 */
    timx_oc_pwm_struct.Pulse = (arr + 1) >> 1;                                              /* 占空比 */
    timx_oc_pwm_struct.OCPolarity = TIM_OCPOLARITY_HIGH;                                    /* 有效电平 */
    HAL_TIM_PWM_ConfigChannel(&g_timx_pwm_handle, &timx_oc_pwm_struct, GTIM_TIMX_PWM_CHY);  /* 配置定时器PWM通道 */
    HAL_TIM_PWM_Start(&g_timx_pwm_handle, GTIM_TIMX_PWM_CHY);                               /* 开启定时器PWM输出 */
}

/**
 * @brief   初始化通用定时器输入捕获
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void gtim_timx_cap_chy_init(uint16_t arr, uint16_t psc)
{
    TIM_IC_InitTypeDef timx_ic_cap_struct = {0};
    
    g_timx_cap_handle.Instance = GTIM_TIMX_CAP;                                             /* 定时器 */
    g_timx_cap_handle.Init.Prescaler = psc;                                                 /* 预分频器系数 */
    g_timx_cap_handle.Init.CounterMode = TIM_COUNTERMODE_UP;                                /* 计数模式 */
    g_timx_cap_handle.Init.Period = arr;                                                    /* 重装载值 */
    HAL_TIM_IC_Init(&g_timx_cap_handle);                                                    /* 定时器输入捕获初始化 */
    
    timx_ic_cap_struct.ICPolarity = TIM_ICPOLARITY_RISING;                                  /* 触发沿 */
    timx_ic_cap_struct.ICSelection = TIM_ICSELECTION_DIRECTTI;                              /* 映射选择 */
    timx_ic_cap_struct.ICPrescaler = TIM_ICPSC_DIV1;                                        /* 输入分频 */
    timx_ic_cap_struct.ICFilter = 0;                                                        /* 输入滤波 */
    HAL_TIM_IC_ConfigChannel(&g_timx_cap_handle, &timx_ic_cap_struct, GTIM_TIMX_CAP_CHY);   /* 配置定时器输入捕获通道 */
    
    __HAL_TIM_ENABLE_IT(&g_timx_cap_handle, TIM_IT_UPDATE);                                 /* 使能定时器更新中断 */
    HAL_TIM_IC_Start_IT(&g_timx_cap_handle, GTIM_TIMX_CAP_CHY);                             /* 开启定时器输入捕获 */
}

/**
 * @brief   初始化通用定时器脉冲计数
 * @param   psc: 预分频系数
 * @retval  无
 */
void gtim_timx_cnt_chy_init(uint16_t psc)
{
    TIM_SlaveConfigTypeDef tim_slave_config_struct = {0};
    
    g_timx_cnt_handle.Instance = GTIM_TIMX_CNT;                                 /* 定时器 */
    g_timx_cnt_handle.Init.Prescaler = psc;                                     /* 预分频器系数 */
    g_timx_cnt_handle.Init.CounterMode = TIM_COUNTERMODE_UP;                    /* 计数模式 */
    g_timx_cnt_handle.Init.Period = 0xFFFF;                                     /* 重装载值 */
    HAL_TIM_IC_Init(&g_timx_cnt_handle);                                        /* 定时器输入捕获初始化 */
    
    tim_slave_config_struct.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;                /* 从模式 */
    tim_slave_config_struct.InputTrigger = TIM_TS_TI1FP1;                       /* 输入触发 */
    tim_slave_config_struct.TriggerPolarity = TIM_TRIGGERPOLARITY_RISING;       /* 触发极性 */
    tim_slave_config_struct.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;       /* 触发分频 */
    tim_slave_config_struct.TriggerFilter = 0;                                  /* 触发滤波器 */
    HAL_TIM_SlaveConfigSynchro(&g_timx_cnt_handle, &tim_slave_config_struct);   /* 配置定时器从模式 */
    
    __HAL_TIM_ENABLE_IT(&g_timx_cnt_handle, TIM_IT_UPDATE);                     /* 使能定时器更新中断 */
    HAL_TIM_IC_Start(&g_timx_cnt_handle, GTIM_TIMX_CNT_CHY);                    /* 开启定时器输入捕获 */
}

/**
 * @brief   获取通用定时器脉冲计数值
 * @param   无
 * @retval  脉冲计数值
 */
uint32_t gtim_timx_cnt_chy_get_count(void)
{
    uint32_t total;
    
    /* 计算总脉冲计数值 */
    total = g_timx_chy_cnt_ofcnt * 0xFFFF;
    total += __HAL_TIM_GET_COUNTER(&g_timx_cnt_handle);
    
    return total;
}

/**
 * @brief   重启通用定时器脉冲计数
 * @param   无
 * @retval  无
 */
void gtim_timx_cnt_chy_restart(void)
{
    __HAL_TIM_DISABLE(&g_timx_cnt_handle);
    g_timx_chy_cnt_ofcnt = 0;
    __HAL_TIM_SET_COUNTER(&g_timx_cnt_handle, 0);
    __HAL_TIM_ENABLE(&g_timx_cnt_handle);
}

/**
 * @brief   HAL库基本定时器初始化MSP函数
 * @param   无
 * @retval  无
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == GTIM_TIMX_INT)
    {
        /* 使能通用定时器时钟 */
        GTIM_TIMX_INT_CLK_ENABLE();
        
        /* 配置中断优先级并使能中断 */
        HAL_NVIC_SetPriority(GTIM_TIMX_INT_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(GTIM_TIMX_INT_IRQn);
    }
}

/**
 * @brief   HAL库定时器PWM初始化MSP函数
 * @param   无
 * @retval  无
 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef gpio_init_struct;
    
    /* PWM输出定时器 */
    if (htim->Instance == GTIM_TIMX_PWM)
    {
        /* 使能相关时钟 */
        GTIM_TIMX_PWM_CHY_GPIO_CLK_ENABLE();
        GTIM_TIMX_PWM_CLK_ENABLE();
        
        /* 初始化PWM输出引脚 */
        gpio_init_struct.Pin = GTIM_TIMX_PWM_CHY_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_struct.Alternate = GTIM_TIMX_PWM_CHY_GPIO_AF;
        HAL_GPIO_Init(GTIM_TIMX_PWM_CHY_GPIO_PORT, &gpio_init_struct);
    }
}

/**
 * @brief   HAL库定时器输出捕获初始化MSP函数
 * @param   无
 * @retval  无
 */
void HAL_TIM_IC_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef gpio_init_struct;
    
    /* 输入捕获定时器 */
    if (htim->Instance == GTIM_TIMX_CAP)
    {
        /* 使能相关时钟 */
        GTIM_TIMX_CAP_CHY_GPIO_CLK_ENABLE();
        GTIM_TIMX_CAP_CLK_ENABLE();
        
        /* 初始化输入捕获引脚 */
        gpio_init_struct.Pin = GTIM_TIMX_CAP_CHY_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLDOWN;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_struct.Alternate = GTIM_TIMX_CAP_CHY_GPIO_AF;
        HAL_GPIO_Init(GTIM_TIMX_CAP_CHY_GPIO_PORT, &gpio_init_struct);
        
        /* 配置中断优先级并使能中断 */
        HAL_NVIC_SetPriority(GTIM_TIMX_CAP_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(GTIM_TIMX_CAP_IRQn);
    }
    /* 脉冲计数定时器 */
    if (htim->Instance == GTIM_TIMX_CNT)
    {
        /* 使能相关时钟 */
        GTIM_TIMX_CNT_CHY_GPIO_CLK_ENABLE();
        GTIM_TIMX_CNT_CLK_ENABLE();
        
        /* 初始化脉冲输入引脚 */
        gpio_init_struct.Pin = GTIM_TIMX_CNT_CHY_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLDOWN;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_struct.Alternate = GTIM_TIMX_CNT_CHY_GPIO_AF;
        HAL_GPIO_Init(GTIM_TIMX_CNT_CHY_GPIO_PORT, &gpio_init_struct);
        
        /* 配置中断优先级并使能中断 */
        HAL_NVIC_SetPriority(GTIM_TIMX_CNT_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(GTIM_TIMX_CNT_IRQn);
    }
}

/**
 * @brief   计数通用定时器中断服务函数
 * @param   无
 * @retval  无
 */
void GTIM_TIMX_INT_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&g_timx_int_handle);
}

/**
 * @brief   输入捕获通用定时器中断回调函数
 * @param   无
 * @retval  无
 */
void GTIM_TIMX_CAP_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&g_timx_cap_handle);
}

/**
 * @brief   脉冲计数通用定时器中断回调函数
 * @param   无
 * @retval  无
 */
void GTIM_TIMX_CNT_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&g_timx_cnt_handle);
}

/**
 * @brief   HAL库定时器超时中断回调函数
 * @param   无
 * @retval  无
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* 计数定时器 */
    if (htim->Instance == GTIM_TIMX_INT)
    {
        LED1_TOGGLE();
    }
    /* 输入捕获定时器 */
    else if (htim->Instance == GTIM_TIMX_CAP)
    {
        if ((g_timx_chy_cap_sta & 0x80) == 0)               /* 还未捕获完成 */
        {
            if ((g_timx_chy_cap_sta & 0x40) != 0)           /* 已经捕获到高电平 */
            {
                if ((g_timx_chy_cap_sta & 0x3F) == 0x3F)    /* 捕获计数值溢出计数器已满 */
                {
                    /* 重新使能捕获并强制标记为捕获完成 */
                    TIM_RESET_CAPTUREPOLARITY(&g_timx_cap_handle, GTIM_TIMX_CAP_CHY);
                    TIM_SET_CAPTUREPOLARITY(&g_timx_cap_handle, GTIM_TIMX_CAP_CHY, TIM_ICPOLARITY_RISING);
                    g_timx_chy_cap_sta |= 0x80;
                    g_timx_chy_cap_val = 0xFFFF;
                }
                else
                {
                    /* 更新捕获计数值溢出计数器 */
                    g_timx_chy_cap_sta++;
                }
            }
        }
    }
    /* 脉冲计数定时器 */
    else if (htim->Instance == GTIM_TIMX_CNT)
    {
        /* 更新脉冲计数定时器溢出次数 */
        g_timx_chy_cnt_ofcnt++;
    }
}

/**
 * @brief   HAL库定时器输入捕获中断回调函数
 * @param   无
 * @retval  无
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == GTIM_TIMX_CAP)
    {
        if ((g_timx_chy_cap_sta & 0x80) == 0)       /* 捕获未完成 */
        {
            if ((g_timx_chy_cap_sta & 0x40) != 0)   /* 已捕获到上升沿 */
            {
                /* 标记为捕获完成并重新开启输入捕获 */
                g_timx_chy_cap_sta |= 0x80; 
                g_timx_chy_cap_val = HAL_TIM_ReadCapturedValue(&g_timx_cap_handle, GTIM_TIMX_CAP_CHY);
                TIM_RESET_CAPTUREPOLARITY(&g_timx_cap_handle, GTIM_TIMX_CAP_CHY);
                TIM_SET_CAPTUREPOLARITY(&g_timx_cap_handle, GTIM_TIMX_CAP_CHY, TIM_ICPOLARITY_RISING);
            }
            else                                    /* 未捕获到上升沿 */
            {
                /* 从捕获到第一个上升沿开始计数 */
                g_timx_chy_cap_sta = 0;
                g_timx_chy_cap_val = 0;
                g_timx_chy_cap_sta |= 0x40;
                __HAL_TIM_DISABLE(&g_timx_cap_handle);
                __HAL_TIM_SET_COUNTER(&g_timx_cap_handle, 0);
                TIM_RESET_CAPTUREPOLARITY(&g_timx_cap_handle, GTIM_TIMX_CAP_CHY);
                TIM_SET_CAPTUREPOLARITY(&g_timx_cap_handle, GTIM_TIMX_CAP_CHY, TIM_ICPOLARITY_FALLING);
                __HAL_TIM_ENABLE(&g_timx_cap_handle);
            }
        }
    }
}
