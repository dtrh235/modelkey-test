#ifndef APP_IWDG_H
#define APP_IWDG_H

#include "app_config.h"
#include "stm32f4xx.h"

/* 硬件 IWDG（Option Byte）上电即跑；不喂会在 1~2s 内复位 → 屏幕反复初始化像“一直刷屏”。 */
static inline void app_iwdg_feed(void)
{
    IWDG->KR = (uint16_t)0xAAAAu;
}

#endif /* APP_IWDG_H */
