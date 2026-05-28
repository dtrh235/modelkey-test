#ifndef APP_CCM_RAM_H
#define APP_CCM_RAM_H

#include "stm32f4xx_hal.h"

/* STM32F407 64KB CCM(0x10000000)：CPU 访问与 SRAM 同速，适合大块缓冲；不可作 DMA 源/目的 */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
#define APP_CCM_DATA  __attribute__((section(".ccmram"), zero_init))
#elif defined(__GNUC__)
#define APP_CCM_DATA  __attribute__((section(".ccmram")))
#else
#define APP_CCM_DATA
#endif

void app_ccm_ram_init(void);

#endif
