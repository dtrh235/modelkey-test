#include "app_rs485_link.h"

#if (APP_RS485_ENABLE == 1)

#include "./SYSTEM/sys/sys.h"
#include "app_config.h"
#include "app_iwdg.h"
#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

#define RS485_DE_GPIO_PORT GPIOC
#define RS485_DE_GPIO_PIN  GPIO_PIN_8

#define RS485_RX_RING_SZ 512u

static UART_HandleTypeDef s_huart6;
static uint8_t s_rs485_inited;
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static uint8_t s_rx_ring[RS485_RX_RING_SZ];

#if (APP_USE_FREERTOS == 1) && APP_RS485_IS_SLAVE
#define RS485_RX_WAIT_YIELD() vTaskDelay(pdMS_TO_TICKS(1u))
#else
#define RS485_RX_WAIT_YIELD() ((void)0)
#endif

static void rs485_rx_ring_push(uint8_t b)
{
    uint16_t next = (uint16_t)((s_rx_head + 1u) % RS485_RX_RING_SZ);

    if(next != s_rx_tail) {
        s_rx_ring[s_rx_head] = b;
        s_rx_head = next;
    }
}

static uint8_t rs485_rx_ring_pop(uint8_t *b)
{
    if(b == NULL || s_rx_tail == s_rx_head) {
        return 0u;
    }
    *b = s_rx_ring[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1u) % RS485_RX_RING_SZ);
    return 1u;
}

void USART6_IRQHandler(void)
{
    uint32_t isr;

    if(s_rs485_inited == 0u) {
        return;
    }

    isr = READ_REG(s_huart6.Instance->SR);
    if((isr & USART_SR_RXNE) != 0u) {
        rs485_rx_ring_push((uint8_t)(READ_REG(s_huart6.Instance->DR) & 0xFFu));
    }
    if((isr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) != 0u) {
        (void)READ_REG(s_huart6.Instance->DR);
    }
}

void app_rs485_de_tx(void)
{
    HAL_GPIO_WritePin(RS485_DE_GPIO_PORT, RS485_DE_GPIO_PIN, GPIO_PIN_SET);
}

void app_rs485_de_rx(void)
{
    HAL_GPIO_WritePin(RS485_DE_GPIO_PORT, RS485_DE_GPIO_PIN, GPIO_PIN_RESET);
}

void app_rs485_link_init(void)
{
    GPIO_InitTypeDef gpio_de = {0};

    if(s_rs485_inited != 0u) {
        return;
    }

    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio_de.Pin = RS485_DE_GPIO_PIN;
    gpio_de.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_de.Pull = GPIO_NOPULL;
    gpio_de.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(RS485_DE_GPIO_PORT, &gpio_de);
    app_rs485_de_rx();

    s_huart6.Instance = USART6;
    s_huart6.Init.BaudRate = APP_RS485_UART_BAUD;
    s_huart6.Init.WordLength = UART_WORDLENGTH_8B;
    s_huart6.Init.StopBits = UART_STOPBITS_1;
    s_huart6.Init.Parity = UART_PARITY_NONE;
    s_huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_huart6.Init.Mode = UART_MODE_TX_RX;

    if(HAL_UART_Init(&s_huart6) != HAL_OK) {
        return;
    }

    s_rx_head = 0u;
    s_rx_tail = 0u;
    __HAL_UART_DISABLE_IT(&s_huart6, UART_IT_RXNE);
    while(__HAL_UART_GET_FLAG(&s_huart6, UART_FLAG_RXNE) != RESET) {
        (void)READ_REG(s_huart6.Instance->DR);
    }
    __HAL_UART_ENABLE_IT(&s_huart6, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART6_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);

    app_rs485_de_rx();
    s_rs485_inited = 1u;
}

uint8_t app_rs485_link_ready(void)
{
    return s_rs485_inited;
}

HAL_StatusTypeDef app_rs485_raw_send(const uint8_t *data, uint16_t len, uint32_t tout_ms)
{
    HAL_StatusTypeDef st;

    if(s_rs485_inited == 0u || data == NULL || len == 0u) {
        return HAL_ERROR;
    }

    app_rs485_de_tx();
    st = HAL_UART_Transmit(&s_huart6, (uint8_t *)data, len, tout_ms);
    while(__HAL_UART_GET_FLAG(&s_huart6, UART_FLAG_TC) == RESET) {
    }
    app_rs485_de_rx();
    {
        volatile uint32_t d;
        for(d = 0u; d < 4000u; d++) {
            __NOP();
        }
    }
    return st;
}

HAL_StatusTypeDef app_rs485_raw_recv(uint8_t *data, uint16_t len, uint32_t tout_ms)
{
    uint32_t t0;
    uint16_t i;

    if(s_rs485_inited == 0u || data == NULL || len == 0u) {
        return HAL_ERROR;
    }
    app_rs485_de_rx();
    t0 = HAL_GetTick();
    for(i = 0u; i < len; i++) {
        while(rs485_rx_ring_pop(&data[i]) == 0u) {
            if((HAL_GetTick() - t0) >= tout_ms) {
                return HAL_TIMEOUT;
            }
#if APP_RS485_IS_SLAVE
            app_iwdg_feed();
#endif
            RS485_RX_WAIT_YIELD();
        }
    }
    return HAL_OK;
}

HAL_StatusTypeDef app_rs485_recv_byte(uint8_t *b, uint32_t tout_ms)
{
    uint32_t t0;

    if(s_rs485_inited == 0u || b == NULL) {
        return HAL_ERROR;
    }
    app_rs485_de_rx();
    if(rs485_rx_ring_pop(b) != 0u) {
        return HAL_OK;
    }
    t0 = HAL_GetTick();
    while(rs485_rx_ring_pop(b) == 0u) {
        if((HAL_GetTick() - t0) >= tout_ms) {
            return HAL_TIMEOUT;
        }
#if APP_RS485_IS_SLAVE
        app_iwdg_feed();
#endif
        RS485_RX_WAIT_YIELD();
    }
    return HAL_OK;
}

#define RS485_LINK_SOF0 0xA5u
#define RS485_LINK_SOF1 0x5Au
#define RS485_LINK_HDR   8u
#define RS485_LINK_CRC   2u

uint16_t app_rs485_recv_frame(uint8_t *buf, uint16_t cap, uint32_t overall_timeout_ms)
{
    uint32_t t0;
    uint16_t plen;
    uint16_t total;
    uint16_t i;
    uint8_t b;

    if(s_rs485_inited == 0u || buf == NULL || cap < (uint16_t)(RS485_LINK_HDR + RS485_LINK_CRC)) {
        return 0u;
    }

    app_rs485_de_rx();
    t0 = HAL_GetTick();

    for(;;) {
        if((HAL_GetTick() - t0) >= overall_timeout_ms) {
            return 0u;
        }
#if APP_RS485_IS_SLAVE
        app_iwdg_feed();
#endif
        if(app_rs485_recv_byte(&b, 5u) != HAL_OK) {
            continue;
        }
        if(b != RS485_LINK_SOF0) {
            continue;
        }
        if(app_rs485_recv_byte(&b, 5u) != HAL_OK) {
            continue;
        }
        if(b != RS485_LINK_SOF1) {
            continue;
        }
        buf[0] = RS485_LINK_SOF0;
        buf[1] = RS485_LINK_SOF1;
        break;
    }

    for(i = 2u; i < RS485_LINK_HDR; i++) {
        if((HAL_GetTick() - t0) >= overall_timeout_ms) {
            return 0u;
        }
        if(app_rs485_recv_byte(&buf[i], 20u) != HAL_OK) {
            return 0u;
        }
    }

    plen = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8);
    total = (uint16_t)(RS485_LINK_HDR + plen + RS485_LINK_CRC);
    if(total > cap) {
        app_rs485_flush_rx();
        return 0u;
    }

    for(i = RS485_LINK_HDR; i < total; i++) {
        if((HAL_GetTick() - t0) >= overall_timeout_ms) {
            return 0u;
        }
        if(app_rs485_recv_byte(&buf[i], 25u) != HAL_OK) {
            return 0u;
        }
    }

    return total;
}

void app_rs485_flush_rx(void)
{
    if(s_rs485_inited == 0u) {
        return;
    }
    s_rx_head = 0u;
    s_rx_tail = 0u;
    while(__HAL_UART_GET_FLAG(&s_huart6, UART_FLAG_RXNE) != RESET) {
        rs485_rx_ring_push((uint8_t)(READ_REG(s_huart6.Instance->DR) & 0xFFu));
    }
    s_rx_head = 0u;
    s_rx_tail = 0u;
}

#endif /* APP_RS485_ENABLE */
