/**
 ****************************************************************************************************
 * @file        atim.c
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

#include "./BSP/TIMER/atim.h"

/* 高级定时器定义 */
TIM_HandleTypeDef g_timx_npwm_handle = {0};
TIM_HandleTypeDef g_timx_comp_handle = {0};
TIM_HandleTypeDef g_timx_cplm_handle = {0};
TIM_HandleTypeDef g_timx_pwmin_handle = {0};

/* 输出指定个数PWM定时器相关变量 */
uint32_t g_timx_chy_npwm_remain = 0;                                    /* 输出指定个数PWM定时器剩余个数计数器 */

/* 互补输出带死区控制定时器相关变量 */
TIM_BreakDeadTimeConfigTypeDef g_break_dead_time_config_struct = {0};   /* 死区控制参数配置句柄 */

/* PWM输入定时器相关变量 */
uint8_t g_timx_chy_pwmin_sta = 0;                                       /* 捕获状态 */
uint32_t g_timx_chy_pwmin_hval;                                         /* PWM高电平脉宽 */
uint32_t g_timx_chy_pwmin_cval;                                         /* PWM周期 */

/**
 * @brief   初始化高级定时器输出指定个数PWM
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void atim_timx_chy_npwm_init(uint16_t arr, uint16_t psc)
{
    GPIO_InitTypeDef gpio_init_struct;
    TIM_OC_InitTypeDef timx_oc_init_struct = {0};
    
    /* 使能相关时钟 */
    ATIM_TIMX_NPWM_CHY_GPIO_CLK_ENABLE();
    ATIM_TIMX_NPWM_CLK_ENABLE();
    
    /* 配置PWM输出引脚 */
    gpio_init_struct.Pin = ATIM_TIMX_NPWM_CHY_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate = ATIM_TIMX_NPWM_CHY_GPIO_AF;
    HAL_GPIO_Init(ATIM_TIMX_NPWM_CHY_GPIO_PORT, &gpio_init_struct);
    
    /* 配置中断优先级并使能中断 */
    HAL_NVIC_SetPriority(ATIM_TIMX_NPWM_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(ATIM_TIMX_NPWM_IRQn);
    
    g_timx_npwm_handle.Instance = ATIM_TIMX_NPWM;                                               /* 定时器 */
    g_timx_npwm_handle.Init.Prescaler = psc;                                                    /* 预分频器系数 */
    g_timx_npwm_handle.Init.CounterMode = TIM_COUNTERMODE_UP;                                   /* 计数模式 */
    g_timx_npwm_handle.Init.Period = arr;                                                       /* 自动重装载值 */
    g_timx_npwm_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;                  /* 自动重装载预加载 */
    g_timx_npwm_handle.Init.RepetitionCounter = 0;                                              /* 重复计数数值 */
    HAL_TIM_PWM_Init(&g_timx_npwm_handle);                                                      /* 初始化定时器PWM输出 */
    
    timx_oc_init_struct.OCMode = TIM_OCMODE_PWM1;                                               /* 输出比较模式 */
    timx_oc_init_struct.Pulse = (arr + 1) >> 1;                                                 /* 占空比 */
    timx_oc_init_struct.OCPolarity = TIM_OCPOLARITY_HIGH;                                       /* 有效电平 */
    HAL_TIM_PWM_ConfigChannel(&g_timx_npwm_handle, &timx_oc_init_struct, ATIM_TIMX_NPWM_CHY);   /* 配置定时器PWM通道 */
    
    __HAL_TIM_ENABLE_IT(&g_timx_npwm_handle, TIM_IT_UPDATE);                                    /* 使能定时器更新中断 */
    HAL_TIM_PWM_Start(&g_timx_npwm_handle, ATIM_TIMX_NPWM_CHY);                                 /* 开启定时器PWM输出 */
}

/**
 * @brief   设置高级定时器输出指定个数PWM
 * @param   npwm: 指定PWM个数
 * @retval  无
 */
void atim_timx_chy_npwm_set(uint32_t npwm)
{
    if (npwm == 0)
    {
        return;
    }
    
    /* 产生更新事件来输出指定个数PWM */
    g_timx_chy_npwm_remain = npwm;
    HAL_TIM_GenerateEvent(&g_timx_npwm_handle, TIM_EVENTSOURCE_UPDATE);
    __HAL_TIM_ENABLE(&g_timx_npwm_handle);
}

/**
 * @brief   输出指定个数PWM高级定时器中断回调函数
 * @param   无
 * @retval  无
 */
void ATIM_TIMX_NPWM_IRQHandler(void)
{
    uint16_t npwm = 0;
    
    /* 定时器更新事件中断 */
    if (__HAL_TIM_GET_FLAG(&g_timx_npwm_handle, TIM_FLAG_UPDATE) != RESET)
    {
        /* 每次最多输出256个PWM（TIMx_RCR只有[7:0]有效） */
        if (g_timx_chy_npwm_remain > 256)
        {
            g_timx_chy_npwm_remain -= 256;
            npwm = 256;
        }
        else if ((g_timx_chy_npwm_remain % 256) != 0)
        {
            npwm = g_timx_chy_npwm_remain % 256;
            g_timx_chy_npwm_remain = 0;
        }
        
        if (npwm != 0)
        {
            /* 利用高级定时器的重复计数器来输出PWM */
            ATIM_TIMX_NPWM->RCR = npwm - 1;
            HAL_TIM_GenerateEvent(&g_timx_npwm_handle, TIM_EVENTSOURCE_UPDATE);
            __HAL_TIM_ENABLE(&g_timx_npwm_handle);
        }
        else
        {
            ATIM_TIMX_NPWM->CR1 &= ~(TIM_CR1_CEN);
        }
        
        /* 清除更新事件标志 */
        __HAL_TIM_CLEAR_IT(&g_timx_npwm_handle, TIM_IT_UPDATE);
    }
}

/**
 * @brief   初始化高级定时器输出比较
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void atim_timx_comp_init(uint16_t arr, uint16_t psc)
{
    GPIO_InitTypeDef gpio_init_struct;
    TIM_OC_InitTypeDef timx_oc_init_struct = {0};
    
    /* 使能相关时钟 */
    ATIM_TIMX_COMP_CH1_GPIO_CLK_ENABLE();
    ATIM_TIMX_COMP_CH2_GPIO_CLK_ENABLE();
    ATIM_TIMX_COMP_CLK_ENABLE();
    
    /* 配置输出比较通道1输出引脚 */
    gpio_init_struct.Pin = ATIM_TIMX_COMP_CH1_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate = ATIM_TIMX_COMP_CH1_GPIO_AF;
    HAL_GPIO_Init(ATIM_TIMX_COMP_CH1_GPIO_PORT, &gpio_init_struct);
    
    /* 配置输出比较通道2输出引脚 */
    gpio_init_struct.Pin = ATIM_TIMX_COMP_CH2_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate = ATIM_TIMX_COMP_CH2_GPIO_AF;
    HAL_GPIO_Init(ATIM_TIMX_COMP_CH2_GPIO_PORT, &gpio_init_struct);
    
    g_timx_comp_handle.Instance = ATIM_TIMX_COMP;                                       /* 定时器 */
    g_timx_comp_handle.Init.Prescaler = psc;                                            /* 预分频器系数 */
    g_timx_comp_handle.Init.CounterMode = TIM_COUNTERMODE_UP;                           /* 计数模式 */
    g_timx_comp_handle.Init.Period = arr;                                               /* 自动重装载值 */
    g_timx_comp_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;          /* 自动重装载预加载 */
    g_timx_comp_handle.Init.RepetitionCounter = 0;                                      /* 重复计数数值 */
    HAL_TIM_OC_Init(&g_timx_comp_handle);                                               /* 初始化定时器输出比较 */
    
    timx_oc_init_struct.OCMode = TIM_OCMODE_TOGGLE;                                     /* 输出比较模式 */
    timx_oc_init_struct.Pulse = ((arr + 1) >> 2) * 1;                                   /* 比较值 */
    timx_oc_init_struct.OCPolarity = TIM_OCPOLARITY_HIGH;                               /* 输出比较极性 */
    HAL_TIM_OC_ConfigChannel(&g_timx_comp_handle, &timx_oc_init_struct, TIM_CHANNEL_1); /* 配置输出比较通道 */
    
    timx_oc_init_struct.OCMode = TIM_OCMODE_TOGGLE;                                     /* 输出比较模式 */
    timx_oc_init_struct.Pulse = ((arr + 1) >> 2) * 2;                                   /* 比较值 */
    timx_oc_init_struct.OCPolarity = TIM_OCPOLARITY_HIGH;                               /* 输出比较极性 */
    HAL_TIM_OC_ConfigChannel(&g_timx_comp_handle, &timx_oc_init_struct, TIM_CHANNEL_2); /* 配置输出比较通道 */
    
    /* 开启输出比较通道输出 */
    HAL_TIM_OC_Start(&g_timx_comp_handle, TIM_CHANNEL_1);
    HAL_TIM_OC_Start(&g_timx_comp_handle, TIM_CHANNEL_2);
}

/**
 * @brief   初始化高级定时器互补输出带死区控制
 * @param   arr: 自动重装载值
 * @param   psc: 预分频系数
 * @retval  无
 */
void atim_timx_cplm_init(uint16_t arr, uint16_t psc)
{
    GPIO_InitTypeDef gpio_init_struct;
    TIM_OC_InitTypeDef timx_oc_struct = {0};
    
    /* 使能相关时钟 */
    ATIM_TIMX_CPLM_CHY_GPIO_CLK_ENABLE();
    ATIM_TIMX_CPLM_CHYN_GPIO_CLK_ENABLE();
    ATIM_TIMX_CPLM_BKIN_GPIO_CLK_ENABLE();
    ATIM_TIMX_CPLM_CLK_ENABLE();
    
    /* 配置输出比较通道1输出引脚 */
    gpio_init_struct.Pin = ATIM_TIMX_CPLM_CHY_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate = ATIM_TIMX_CPLM_CHY_GPIO_AF;
    HAL_GPIO_Init(ATIM_TIMX_CPLM_CHY_GPIO_PORT, &gpio_init_struct);
    
    /* 配置输出比较通道1互补输出引脚 */
    gpio_init_struct.Pin = ATIM_TIMX_CPLM_CHYN_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate = ATIM_TIMX_CPLM_CHYN_GPIO_AF;
    HAL_GPIO_Init(ATIM_TIMX_CPLM_CHYN_GPIO_PORT, &gpio_init_struct);
    
    /* 配置输出比较通道刹车输入引脚 */
    gpio_init_struct.Pin = ATIM_TIMX_CPLM_BKIN_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate = ATIM_TIMX_CPLM_BKIN_GPIO_AF;
    HAL_GPIO_Init(ATIM_TIMX_CPLM_BKIN_GPIO_PORT, &gpio_init_struct);
    
    g_timx_cplm_handle.Instance = ATIM_TIMX_CPLM;                                           /* 定时器 */
    g_timx_cplm_handle.Init.Prescaler = psc;                                                /* 预分频器系数 */
    g_timx_cplm_handle.Init.CounterMode = TIM_COUNTERMODE_UP;                               /* 计数模式 */
    g_timx_cplm_handle.Init.Period = arr;                                                   /* 自动重装载值 */
    g_timx_cplm_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV4;                         /* 计数时钟分频器系数 */
    g_timx_cplm_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;              /* 自动重装载预加载 */
    g_timx_cplm_handle.Init.RepetitionCounter = 0;                                          /* 重复计数数值 */
    HAL_TIM_PWM_Init(&g_timx_cplm_handle);                                                  /* 初始化定时器PWM输出 */
        
    timx_oc_struct.OCMode = TIM_OCMODE_PWM1;                                                /* 输出比较模式 */
    timx_oc_struct.Pulse = (arr + 1) >> 1;                                                  /* 比较值 */
    timx_oc_struct.OCPolarity = TIM_OCPOLARITY_LOW;                                         /* 输出比较极性 */
    timx_oc_struct.OCNPolarity = TIM_OCNPOLARITY_LOW;                                       /* 互补通道输出比较极性 */
    timx_oc_struct.OCIdleState = TIM_OCIDLESTATE_SET;                                       /* 空闲输出极性 */
    timx_oc_struct.OCNIdleState = TIM_OCNIDLESTATE_SET;                                     /* 互补通道空闲输出极性 */
    HAL_TIM_PWM_ConfigChannel(&g_timx_cplm_handle, &timx_oc_struct, ATIM_TIMX_CPLM_CHY);    /* 配置PWM输出通道 */
    
    g_break_dead_time_config_struct.OffStateRunMode = TIM_OSSR_DISABLE;                     /* 运行模式的关闭输出状态 */
    g_break_dead_time_config_struct.OffStateIDLEMode = TIM_OSSI_DISABLE;                    /* 空闲模式的关闭输出状态 */
    g_break_dead_time_config_struct.LockLevel = TIM_LOCKLEVEL_OFF;                          /* 寄存器锁 */
    g_break_dead_time_config_struct.BreakState = TIM_BREAK_ENABLE;                          /* 刹车输入使能 */
    g_break_dead_time_config_struct.BreakPolarity = TIM_BREAKPOLARITY_HIGH;                 /* 刹车输入有效极性 */
    g_break_dead_time_config_struct.AutomaticOutput = TIM_AUTOMATICOUTPUT_ENABLE;           /* 自动使能输出 */
    HAL_TIMEx_ConfigBreakDeadTime(&g_timx_cplm_handle, &g_break_dead_time_config_struct);   /* 配置死区、刹车等参数 */
    
    /* 开启PWM输出通道输出 */
    HAL_TIM_PWM_Start(&g_timx_cplm_handle, ATIM_TIMX_CPLM_CHY);
    HAL_TIMEx_PWMN_Start(&g_timx_cplm_handle, ATIM_TIMX_CPLM_CHY);
}

/**
 * @brief   设置高级定时器互补输出带死区控制
 * @param   ccr: 输出比较值
 * @param   dtg: 死区时间
 * @retval  无
 */
void atim_timx_cplm_set(uint16_t ccr, uint8_t dtg)
{
    g_break_dead_time_config_struct.DeadTime = dtg;                                         /* 死区时间 */
    HAL_TIMEx_ConfigBreakDeadTime(&g_timx_cplm_handle, &g_break_dead_time_config_struct);   /* 配置死区时间参数 */
    __HAL_TIM_MOE_ENABLE(&g_timx_cplm_handle);                                              /* 使能主输出 */
    ATIM_TIMX_CPLM->CCR1 = ccr;                                                             /* 设置比较寄存器 */
}

/**
 * @brief   初始化高级定时器PWM输入
 * @param   psc: 预分频系数
 * @retval  无
 */
void atim_timx_pwmin_chy_init(uint16_t psc)
{
    GPIO_InitTypeDef gpio_init_struct;
    TIM_SlaveConfigTypeDef tim_slave_config_struct = {0};
    TIM_IC_InitTypeDef tim_ic_init_struct = {0};
    
    /* 使能相关时钟 */
    ATIM_TIMX_PWMIN_CLK_ENABLE();
    ATIM_TIMX_PWMIN_CHY_GPIO_CLK_ENABLE();
    
    /* 配置PWM输入引脚 */
    gpio_init_struct.Pin = ATIM_TIMX_PWMIN_CHY_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP; 
    gpio_init_struct.Pull = GPIO_PULLDOWN;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate = ATIM_TIMX_PWMIN_CHY_GPIO_AF;
    HAL_GPIO_Init(ATIM_TIMX_PWMIN_CHY_GPIO_PORT, &gpio_init_struct);
    
    g_timx_pwmin_handle.Instance = ATIM_TIMX_PWMIN;                                     /* 定时器 */
    g_timx_pwmin_handle.Init.Prescaler = psc;                                           /* 预分频器系数 */
    g_timx_pwmin_handle.Init.CounterMode = TIM_COUNTERMODE_UP;                          /* 计数模式 */
    g_timx_pwmin_handle.Init.Period = 0xFFFF;                                           /* 自动重装载值 */
    HAL_TIM_IC_Init(&g_timx_pwmin_handle);
    
    tim_slave_config_struct.SlaveMode = TIM_SLAVEMODE_RESET;                            /* 从模式 */
    tim_slave_config_struct.InputTrigger = TIM_TS_TI1FP1;                               /* 输入触发源 */
    tim_slave_config_struct.TriggerPolarity = TIM_TRIGGERPOLARITY_RISING;               /* 触发极性 */
    tim_slave_config_struct.TriggerFilter = 0;                                          /* 触发滤波 */
    HAL_TIM_SlaveConfigSynchro(&g_timx_pwmin_handle, &tim_slave_config_struct);         /* 配置从模式 */

    tim_ic_init_struct.ICPolarity = TIM_ICPOLARITY_RISING;                              /* 输入捕获极性 */
    tim_ic_init_struct.ICSelection = TIM_ICSELECTION_DIRECTTI;                          /* 输入捕获选择 */
    tim_ic_init_struct.ICPrescaler = TIM_ICPSC_DIV1;                                    /* 输入捕获分频系数 */
    tim_ic_init_struct.ICFilter = 0;                                                    /* 输入捕获滤波 */
    HAL_TIM_IC_ConfigChannel(&g_timx_pwmin_handle, &tim_ic_init_struct, TIM_CHANNEL_1); /* 配置输入捕获通道1 */
    
    tim_ic_init_struct.ICPolarity = TIM_ICPOLARITY_FALLING;                             /* 输入捕获极性 */
    tim_ic_init_struct.ICSelection = TIM_ICSELECTION_INDIRECTTI;                        /* 输入捕获选择 */
    tim_ic_init_struct.ICPrescaler = TIM_ICPSC_DIV1;                                    /* 输入捕获分频系数 */
    tim_ic_init_struct.ICFilter = 0;                                                    /* 输入捕获滤波 */
    HAL_TIM_IC_ConfigChannel(&g_timx_pwmin_handle, &tim_ic_init_struct, TIM_CHANNEL_2); /* 配置输入捕获通道2 */
    
    /* 配置中断优先级并使能中断 */
    HAL_NVIC_SetPriority(ATIM_TIMX_PWMIN_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ( ATIM_TIMX_PWMIN_IRQn );
    
    /* 开启输入捕获通道 */
    HAL_TIM_IC_Start_IT(&g_timx_pwmin_handle, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&g_timx_pwmin_handle, TIM_CHANNEL_2);
}

/**
 * @brief   PWM输入高级定时器中断回调函数
 * @param   无
 * @retval  无
 */
void ATIM_TIMX_PWMIN_IRQHandler(void)
{
    /* 输入捕获通道1捕获到上升沿 */
    if (__HAL_TIM_GET_FLAG(&g_timx_pwmin_handle, TIM_FLAG_CC1) != RESET)
    {
        /* 获取捕获到PWM的周期 */
        g_timx_chy_pwmin_cval = HAL_TIM_ReadCapturedValue(&g_timx_pwmin_handle, TIM_CHANNEL_1) + 1;
        g_timx_chy_pwmin_sta = 1;
        
        __HAL_TIM_CLEAR_FLAG(&g_timx_pwmin_handle, TIM_FLAG_CC1);
    }
    
    /* 输入捕获通道2捕获到下降沿 */
    if (__HAL_TIM_GET_FLAG(&g_timx_pwmin_handle, TIM_FLAG_CC2) != RESET)
    {
        /* 获取捕获到PWM的高电平脉宽 */
        g_timx_chy_pwmin_hval = HAL_TIM_ReadCapturedValue(&g_timx_pwmin_handle, TIM_CHANNEL_2) + 1;
        
        __HAL_TIM_CLEAR_FLAG(&g_timx_pwmin_handle, TIM_FLAG_CC2);
    }
}
