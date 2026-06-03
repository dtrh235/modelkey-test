/**
  ******************************************************************************
  * @file    Templates/Src/stm32f4xx_it.c 
  * @author  MCD Application Team
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "./SYSTEM/sys/sys.h"
#include "stm32f4xx_it.h"
#include "cloud_ota_service.h"
#include "app_config.h"
#include "app_iwdg.h"
#include "app_slave_diag.h"

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"

extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
#endif

/** @addtogroup STM32F4xx_HAL_Examples
  * @{
  */

/** @addtogroup Templates
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
static uint8_t app_sp_in_ram(uint32_t sp)
{
  if((sp >= 0x20000000u) && (sp <= 0x20020000u - 32u)) {
    return 1u; /* SRAM1/2 */
  }
  if((sp >= 0x10000000u) && (sp <= 0x10010000u - 32u)) {
    return 1u; /* CCM RAM */
  }
  return 0u;
}

static void app_fault_dump_stack(const char *tag, uint32_t sp, uint32_t exc_lr)
{
#if (APP_SLAVE_USART1_DEBUG != 0)
  uint32_t r0 = 0u, r1 = 0u, r2 = 0u, r3 = 0u, r12 = 0u, lr = 0u, pc = 0u, xpsr = 0u;
  uint32_t *stk = (uint32_t *)sp;
  if(app_sp_in_ram(sp) != 0u) {
    r0 = stk[0];
    r1 = stk[1];
    r2 = stk[2];
    r3 = stk[3];
    r12 = stk[4];
    lr = stk[5];
    pc = stk[6];
    xpsr = stk[7];
  }
  SLAVE_DBG_LOG("[SLV][FAULT] %s CFSR=0x%08lX HFSR=0x%08lX MMFAR=0x%08lX BFAR=0x%08lX",
                (tag != NULL) ? tag : "?",
                (unsigned long)SCB->CFSR,
                (unsigned long)SCB->HFSR,
                (unsigned long)SCB->MMFAR,
                (unsigned long)SCB->BFAR);
  SLAVE_DBG_LOG("[SLV][FAULT] SP=0x%08lX EXC_LR=0x%08lX PC=0x%08lX LR=0x%08lX xPSR=0x%08lX",
                (unsigned long)sp, (unsigned long)exc_lr,
                (unsigned long)pc, (unsigned long)lr, (unsigned long)xpsr);
  SLAVE_DBG_LOG("[SLV][FAULT] R0=0x%08lX R1=0x%08lX R2=0x%08lX R3=0x%08lX R12=0x%08lX",
                (unsigned long)r0, (unsigned long)r1, (unsigned long)r2,
                (unsigned long)r3, (unsigned long)r12);
#else
  (void)tag;
  (void)sp;
  (void)exc_lr;
#endif
}

static void app_fault_dump_and_reset(const char *tag)
{
#if (APP_SLAVE_USART1_DEBUG != 0)
  SLAVE_DBG_LOG("[SLV][FAULT] %s CFSR=0x%08lX HFSR=0x%08lX MMFAR=0x%08lX BFAR=0x%08lX",
                (tag != NULL) ? tag : "?",
                (unsigned long)SCB->CFSR,
                (unsigned long)SCB->HFSR,
                (unsigned long)SCB->MMFAR,
                (unsigned long)SCB->BFAR);
#endif
  NVIC_SystemReset();
  while(1) {
  }
}

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  uint32_t msp = __get_MSP();
  uint32_t psp = __get_PSP();
  uint32_t exc_lr = 0u;
  /* HardFault 时异常栈帧包含：r0,r1,r2,r3,r12,LR,PC,xPSR。
   * 这里不依赖内联汇编读取 EXC_RETURN/LR，直接从 MSP/PSP 的栈帧安全取值。
   */
  if(app_sp_in_ram(psp) != 0u) {
    exc_lr = ((uint32_t *)psp)[5];
    app_fault_dump_stack("HardFault", psp, exc_lr);
  } else if(app_sp_in_ram(msp) != 0u) {
    exc_lr = ((uint32_t *)msp)[5];
    app_fault_dump_stack("HardFault", msp, exc_lr);
  }
  app_fault_dump_and_reset("HardFault");
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  app_fault_dump_and_reset("MemManage");
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  app_fault_dump_and_reset("BusFault");
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  app_fault_dump_and_reset("UsageFault");
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
#if (APP_USE_FREERTOS == 1)
  vPortSVCHandler();
#endif
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
#if (APP_USE_FREERTOS == 1)
  xPortPendSVHandler();
#endif
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
  app_iwdg_feed();
#if (APP_CLOUD_ENABLE == 1)
  cloud_ota_service_tick_1ms();
#endif
#if (APP_USE_FREERTOS == 1)
  if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
    xPortSysTickHandler();
  }
#endif
}

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f4xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/


/**
  * @}
  */ 

/**
  * @}
  */
