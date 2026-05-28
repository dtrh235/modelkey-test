/**
 ****************************************************************************************************
 * @file        usart.c
 * @author      Mr.Mr.
 * @version     V1.0
 * @date        2021-10-14
 * @brief       ????????????(????????1)?????printf
 * @license     Copyright (c) 2020-2032,  
 ****************************************************************************************************
 * @attention
 *
 * ?????:ST-1 STM32F407?????
 *  
 *  
 * ??????:www.genbotter.com
 * ??????:makerbase.taobao.com
 *
 * ??????
 * V1.0 20211014
 * ?????????
 *
 ****************************************************************************************************
 */

#include "sys.h"
#include "usart.h"
#include <string.h>
#include "../../BSP/AS608/as608.h"
#include "app_config.h"

static UART_HandleTypeDef g_uart_dbg_handle = {0};  /* printf / usart_debug_tx_str */
static uint8_t g_uart_dbg_inited = 0u;
#define AS608_UART_BAUDRATE 57600u

static void usart_debug_init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

#if (APP_DEBUG_ON_USART6 != 0)
    /* 串口调试助手：USART6 PC6(TX) / PC7(RX)，115200 8N1 */
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio_init_struct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &gpio_init_struct);

    g_uart_dbg_handle.Instance = USART6;
#else
    /* USART1 TX: PA9, RX: PA10 */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio_init_struct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);

    g_uart_dbg_handle.Instance = USART1;
#endif
    g_uart_dbg_handle.Init.BaudRate = baudrate;
    g_uart_dbg_handle.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart_dbg_handle.Init.StopBits = UART_STOPBITS_1;
    g_uart_dbg_handle.Init.Parity = UART_PARITY_NONE;
    g_uart_dbg_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_uart_dbg_handle.Init.Mode = UART_MODE_TX_RX;
    if(HAL_UART_Init(&g_uart_dbg_handle) == HAL_OK) {
        g_uart_dbg_inited = 1u;
    }
}

void usart_debug_tx_str(const char *s)
{
    size_t n;

    if(s == NULL || g_uart_dbg_inited == 0u) {
        return;
    }
    n = strlen(s);
    if(n > 400u) {
        n = 400u;
    }
    (void)HAL_UART_Transmit(&g_uart_dbg_handle, (uint8_t *)s, (uint16_t)n, 80u);
}

/* ??????os,?????????????????? */
#if SYS_SUPPORT_OS
#include "os.h"                               /* os ??? */
#endif

/******************************************************************************************/
/* ???????????, ???printf????, ??????????use MicroLIB */

#if 1
#if (__ARMCC_VERSION >= 6010050)                    /* ???AC6??????? */
__asm(".global __use_no_semihosting\n\t");          /* ???????????????? */
__asm(".global __ARM_use_no_argv \n\t");            /* AC6?????????main????????????????????????????????????? */

#else
/* ???AC5???????, ?????????__FILE ?? ???????????? */
#pragma import(__use_no_semihosting)

struct __FILE
{
    int handle;
    /* Whatever you require here. If the only file you are using is */
    /* standard output using printf() for debugging, no file handling */
    /* is required. */
};

#endif

/* ??????????????????????????_ttywrch\_sys_exit\_sys_command_string????,????????AC6??AC5?? */
int _ttywrch(int ch)
{
    ch = ch;
    return ch;
}

/* ????_sys_exit()??????????????? */
void _sys_exit(int x)
{
    x = x;
}

char *_sys_command_string(char *cmd, int len)
{
    return NULL;
}

/* FILE ?? stdio.h???????. */
FILE __stdout;

/* ?????fputc????, printf????????????????fputc?????????????? */
int fputc(int ch, FILE *f)
{
    USART_TypeDef *uartx = g_uart_dbg_inited ? g_uart_dbg_handle.Instance : g_uart1_handle.Instance;
    if(uartx == NULL) return ch;
    while((uartx->SR & USART_SR_TXE) == 0u) {
    }
    uartx->DR = (uint8_t)ch;
    return ch;
}
#endif
/***********************************************END*******************************************/
    
#if USART_EN_RX                                     /* ??????????? */

/* ???????, ???USART_REC_LEN?????. */
uint8_t g_usart_rx_buf[USART_REC_LEN];

/*  ??????
 *  bit15??      ?????????
 *  bit14??      ?????0x0d
 *  bit13~0??    ?????????????????
*/
uint16_t g_usart_rx_sta = 0;

uint8_t g_rx_buffer[RXBUFFERSIZE];                  /* HAL??????????????? */

UART_HandleTypeDef g_uart1_handle;                  /* UART??? */


/**
 * @brief       ????X?????????
 * @param       baudrate: ??????, ?????????????????????
 * @note        ???: ?????????????????, ??????????????????.
 *              ?????USART????????sys_stm32_clock_init()?????????????????.
 * @retval      ??
 */
void usart_init(uint32_t baudrate)
{
    /* 调试口：默认 USART6 PC6/PC7；APP_DEBUG_ON_USART6=0 时为 PA9/PA10 */
    usart_debug_init(APP_DEBUG_UART_BAUD);

    g_uart1_handle.Instance = USART_UX;                         /* USART1 */
    g_uart1_handle.Init.BaudRate = AS608_UART_BAUDRATE;         /* AS608 common default baudrate */
    g_uart1_handle.Init.WordLength = UART_WORDLENGTH_8B;        /* ????8???????? */
    g_uart1_handle.Init.StopBits = UART_STOPBITS_1;             /* ??????? */
    g_uart1_handle.Init.Parity = UART_PARITY_NONE;              /* ??????????? */
    g_uart1_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;        /* ????????? */
    g_uart1_handle.Init.Mode = UART_MODE_TX_RX;                 /* ????? */
    HAL_UART_Init(&g_uart1_handle);                             /* HAL_UART_Init()?????UART1 */
    
    /* ????????????????????????UART_IT_RXNE?????????????????????????????????????? */
    HAL_UART_Receive_IT(&g_uart1_handle, (uint8_t *)g_rx_buffer, RXBUFFERSIZE);
}

/**
 * @brief       UART???????????
 * @param       huart: UART??????????
 * @note        ???????HAL_UART_Init()????
 *              ???????????????????????????
 * @retval      ??
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio_init_struct;
    if(huart->Instance == USART_UX)                             /* ????????1??????????1 MSP????? */
    {
        USART_UX_CLK_ENABLE();                                  /* USART1 ?????? */
        USART_TX_GPIO_CLK_ENABLE();                             /* ?????????????? */
        USART_RX_GPIO_CLK_ENABLE();                             /* ?????????????? */

        gpio_init_struct.Pin = USART_TX_GPIO_PIN;               /* TX???? */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* ??????????? */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* ???? */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* ???? */
        gpio_init_struct.Alternate = USART_TX_GPIO_AF;          /* ?????USART1 */
        HAL_GPIO_Init(USART_TX_GPIO_PORT, &gpio_init_struct);   /* ????????????? */

        gpio_init_struct.Pin = USART_RX_GPIO_PIN;               /* RX???? */
        gpio_init_struct.Alternate = USART_RX_GPIO_AF;          /* ?????USART1 */
        HAL_GPIO_Init(USART_RX_GPIO_PORT, &gpio_init_struct);   /* ????????????? */

#if USART_EN_RX
        HAL_NVIC_EnableIRQ(USART_UX_IRQn);                      /* ???USART1??????? */
        HAL_NVIC_SetPriority(USART_UX_IRQn, 3, 3);              /* ????????3?????????3 */
#endif
    }
    else if(huart->Instance == USART2)
    {
        /* ESP8266 WiFi：PA2=TX PA3=RX（cloud_aliyun_at） */
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        gpio_init_struct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_struct.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &gpio_init_struct);
    }
    else if(huart->Instance == UART4)
    {
        /* UART4: PC10 TX, PC11 RX (AF8), used for unlock success '1' output */
        __HAL_RCC_UART4_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        gpio_init_struct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_struct.Alternate = GPIO_AF8_UART4;
        HAL_GPIO_Init(GPIOC, &gpio_init_struct);
    }
#if (APP_DEBUG_ON_USART6 != 0)
    else if(huart->Instance == USART6)
    {
        /* 调试串口 USART6：PC6 TX / PC7 RX */
        __HAL_RCC_USART6_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        gpio_init_struct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_struct.Alternate = GPIO_AF8_USART6;
        HAL_GPIO_Init(GPIOC, &gpio_init_struct);
    }
#elif (APP_RS485_ENABLE == 1)
    else if(huart->Instance == USART6)
    {
        /* RS485：USART6 + PC8 DE */
        __HAL_RCC_USART6_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        gpio_init_struct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio_init_struct.Alternate = GPIO_AF8_USART6;
        HAL_GPIO_Init(GPIOC, &gpio_init_struct);

        gpio_init_struct.Pin = GPIO_PIN_8;
        gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
        gpio_init_struct.Pull = GPIO_NOPULL;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOC, &gpio_init_struct);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET);
    }
#endif
}

/**
 * @brief       Rx??????????
 * @param       huart: UART??????????
 * @retval      ??
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART_UX)             /* ????????1 */
    {
#if AS608_UART_SNIFF_IN_ISR
        as608_uart_rx_byte(g_rx_buffer[0]);
#endif

        if((g_usart_rx_sta & 0x8000) == 0)      /* ????????? */
        {
            if(g_usart_rx_sta & 0x4000)         /* ???????0x0d */
            {
                if(g_rx_buffer[0] != 0x0a) 
                {
                    g_usart_rx_sta = 0;         /* ???????,?????? */
                }
                else 
                {
                    g_usart_rx_sta |= 0x8000;   /* ????????? */
                }
            }
            else                                /* ??????0X0D */
            {
                if(g_rx_buffer[0] == 0x0d)
                {
                    g_usart_rx_sta |= 0x4000;
                }
                else
                {
                    g_usart_rx_buf[g_usart_rx_sta & 0X3FFF] = g_rx_buffer[0] ;
                    g_usart_rx_sta++;
                    if(g_usart_rx_sta > (USART_REC_LEN - 1))
                    {
                        g_usart_rx_sta = 0;     /* ???????????,?????????? */
                    }
                }
            }
        }
        HAL_UART_Receive_IT(&g_uart1_handle, (uint8_t *)g_rx_buffer, RXBUFFERSIZE);
    }
}

/**
 * @brief       ????1?????????
 * @param       ??
 * @retval      ??
 */
void USART_UX_IRQHandler(void)
{
#if SYS_SUPPORT_OS                              /* ???OS */
    OSIntEnter();    
#endif
    HAL_UART_IRQHandler(&g_uart1_handle);       /* ????HAL????????????????? */
#if SYS_SUPPORT_OS                              /* ???OS */
    OSIntExit();
#endif
}

#endif


 

 




