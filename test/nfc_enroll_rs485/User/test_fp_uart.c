/**
 * @file test_fp_uart.c
 * @brief USART3 指纹串口 — 发用 HAL，收用 RXNE 直中断（同 test_wifi.c，已验证）
 *
 * CN7.3 PB10 TX  CN7.2 PB11 RX  57600 8N1
 */

#include "SYSTEM/usart/usart.h"
#include "as608.h"
#include "stm32f4xx_hal.h"

#define FP3_BAUD  57600u

UART_HandleTypeDef g_uart1_handle;
uint8_t  g_usart_rx_buf[USART_REC_LEN];
uint16_t g_usart_rx_sta;
uint8_t  g_rx_buffer[RXBUFFERSIZE];

static uint8_t s_fp3_ok;
static volatile uint32_t s_fp3_isr_cnt;
static volatile uint32_t s_fp3_ore_cnt;

uint32_t test_fp3_isr_rx_count(void)
{
    return s_fp3_isr_cnt;
}

uint32_t test_fp3_ore_count(void)
{
    return s_fp3_ore_cnt;
}

void USART3_IRQHandler(void)
{
    uint32_t sr;

    if(s_fp3_ok == 0u) {
        return;
    }
    sr = READ_REG(USART3->SR);
    if((sr & USART_SR_RXNE) != 0u) {
#if (AS608_UART_SNIFF_IN_ISR != 0)
        as608_uart_rx_byte((uint8_t)(READ_REG(USART3->DR) & 0xFFu));
        s_fp3_isr_cnt++;
#else
        (void)READ_REG(USART3->DR);
#endif
    }
    if((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) != 0u) {
        if((sr & USART_SR_ORE) != 0u) {
            s_fp3_ore_cnt++;
        }
        (void)READ_REG(USART3->DR);
    }
}

static void test_fp3_rx_irq_on(void)
{
    s_fp3_isr_cnt = 0u;
    s_fp3_ore_cnt = 0u;
    __HAL_UART_DISABLE_IT(&g_uart1_handle, UART_IT_RXNE);
    while(__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_RXNE) != RESET) {
#if (AS608_UART_SNIFF_IN_ISR != 0)
        as608_uart_rx_byte((uint8_t)(READ_REG(USART3->DR) & 0xFFu));
        s_fp3_isr_cnt++;
#else
        (void)READ_REG(USART3->DR);
#endif
    }
    __HAL_UART_CLEAR_OREFLAG(&g_uart1_handle);
    g_uart1_handle.gState = HAL_UART_STATE_READY;
    g_uart1_handle.RxState = HAL_UART_STATE_READY;
    __HAL_UART_ENABLE_IT(&g_uart1_handle, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

void test_fp3_uart_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &gpio);

    g_uart1_handle.Instance = USART3;
    g_uart1_handle.Init.BaudRate = FP3_BAUD;
    g_uart1_handle.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart1_handle.Init.StopBits = UART_STOPBITS_1;
    g_uart1_handle.Init.Parity = UART_PARITY_NONE;
    g_uart1_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_uart1_handle.Init.Mode = UART_MODE_TX_RX;

    s_fp3_ok = (HAL_UART_Init(&g_uart1_handle) == HAL_OK) ? 1u : 0u;
    if(s_fp3_ok != 0u) {
        test_fp3_rx_irq_on();
    }
}

void test_fp3_uart_rx_reset(void)
{
    if(s_fp3_ok != 0u) {
        test_fp3_rx_irq_on();
    }
}
