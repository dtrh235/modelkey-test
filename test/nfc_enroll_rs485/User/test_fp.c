/**
 * @file test_fp.c
 * @brief AS608 CN7 自检 — USART3 RXNE 直中断（WiFi 同款收包）
 */

#include "test_fp.h"

#include "app_config.h"
#include "test_usb_uart.h"
#include "./SYSTEM/delay/delay.h"
#include "SYSTEM/usart/usart.h"
#include "as608.h"
#include "stm32f4xx_hal.h"

extern void test_fp3_uart_init(void);
extern void test_fp3_uart_rx_reset(void);
extern uint32_t test_fp3_isr_rx_count(void);
extern uint32_t test_fp3_ore_count(void);
extern uint8_t aRxBuffer[];

static uint8_t s_fp_ok;
static uint32_t s_last_poll;
static uint32_t s_last_hs;
static uint8_t s_last_sta;
static uint32_t s_hs_count;

static void test_fp_log_rx_hex(const char *tag)
{
    uint16_t n = as608_rx_widx_get();
    uint16_t i;

    test_usb_uart_printf("%s rx=%uB hex:", tag, (unsigned)n);
    for(i = 0u; i < n && i < 32u; i++) {
        test_usb_uart_printf(" %02X", (unsigned)(uint8_t)aRxBuffer[i]);
    }
    if(n > 32u) {
        test_usb_uart_print(" ...");
    }
    test_usb_uart_print("\r\n");
}

static void test_fp_log_uart_dia(void)
{
    uint32_t moder = GPIOB->MODER;

    test_usb_uart_printf("[FP-DIA] BRR=0x%04lX CR1=0x%04lX SR=0x%04lX\r\n",
                         (unsigned long)USART3->BRR,
                         (unsigned long)USART3->CR1,
                         (unsigned long)(USART3->SR & 0xFFFFu));
    test_usb_uart_printf("[FP-DIA] PB10mod=%lu PB11mod=%lu ISRrx=%lu ORE=%lu\r\n",
                         (unsigned long)((moder >> 20) & 3u),
                         (unsigned long)((moder >> 22) & 3u),
                         (unsigned long)test_fp3_isr_rx_count(),
                         (unsigned long)test_fp3_ore_count());
}

static void test_fp_loopback(void)
{
    static const uint8_t k_pat[2] = {0x55u, 0xAAu};
    uint32_t t0;
    uint32_t isr0 = test_fp3_isr_rx_count();

    as608_rx_buffer_clear();
    (void)HAL_UART_Transmit(&g_uart1_handle, (uint8_t *)k_pat, 2u, 100u);

    t0 = HAL_GetTick();
    while((HAL_GetTick() - t0) < 50u) {
        if(as608_rx_widx_get() >= 2u) {
            break;
        }
    }

    test_usb_uart_print("[FP-DIA] loopback: wire PB10 to PB11, expect rx=2\r\n");
    test_fp_log_rx_hex("[FP-LB]");
    test_usb_uart_printf("[FP-DIA] ISR delta=%lu (no wire -> rx=0 is normal)\r\n",
                         (unsigned long)(test_fp3_isr_rx_count() - isr0));
    as608_rx_buffer_clear();
    test_fp3_uart_rx_reset();
}

static uint8_t test_fp_try_handshake(void)
{
    uint32_t addr = 0u;
    uint32_t isr0 = test_fp3_isr_rx_count();

    s_hs_count++;
    as608_rx_buffer_clear();

    if(GZ_HandShake(&addr) == 0u) {
        AS608Addr = addr;
        s_fp_ok = 1u;
        test_usb_uart_printf("[FP] *** PASS handshake #%lu addr=0x%08lX ***\r\n",
                             (unsigned long)s_hs_count,
                             (unsigned long)AS608Addr);
        return 1u;
    }

    test_fp_log_rx_hex("[FP-RX]");
    test_usb_uart_printf("[FP] handshake #%lu FAIL STA=%u ISR+%lu\r\n",
                         (unsigned long)s_hs_count,
                         (unsigned)((GZ_Sta == GPIO_PIN_SET) ? 1u : 0u),
                         (unsigned long)(test_fp3_isr_rx_count() - isr0));
    return 0u;
}

static void test_fp_read_info(void)
{
    SysPara para;
    uint16_t tpl_n = 0u;

    if(GZ_ReadSysPara(&para) == 0u) {
        test_usb_uart_printf("[FP] sys: max=%u level=%u pkt=%u baudN=%u\r\n",
                             (unsigned)para.GZ_max,
                             (unsigned)para.GZ_level,
                             (unsigned)para.GZ_size,
                             (unsigned)para.GZ_N);
    }
    if(GZ_ValidTempleteNum(&tpl_n) == 0u) {
        test_usb_uart_printf("[FP] templates in chip=%u\r\n", (unsigned)tpl_n);
    }
}

void test_fp_board_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);
}

void test_fp_uart_early_init(void)
{
    GZ_StaGPIO_Init();
    test_fp3_uart_init();
}

void test_fp_init(void)
{
    s_fp_ok = 0u;
    s_hs_count = 0u;
    s_last_hs = 0u;
    s_last_sta = (GZ_Sta == GPIO_PIN_SET) ? 1u : 0u;

    test_usb_uart_print("\r\n========================================\r\n");
    test_usb_uart_print("[FP] AS608 CN7 test (RXNE IRQ, same as WiFi fix)\r\n");
    test_usb_uart_print("[FP] CN7.3=TX CN7.2=RX CN7.5=WAK PE10\r\n");
    test_usb_uart_printf("[FP] STA=%u\r\n", (unsigned)s_last_sta);
    test_usb_uart_print("========================================\r\n");

    test_fp_log_uart_dia();
    test_fp_loopback();

    delay_ms(400);
    (void)test_fp_try_handshake();

    if(s_fp_ok == 0u) {
        test_usb_uart_print("[FP] retry every 2s; if ISR+0 -> check CN7.2 PB11\r\n");
    } else {
        test_fp_read_info();
    }

    test_fp_log_uart_dia();
    test_usb_uart_print("========================================\r\n");

    s_last_poll = HAL_GetTick();
    s_last_hs = HAL_GetTick();
}

uint8_t test_fp_hw_ok(void)
{
    return s_fp_ok;
}

void test_fp_poll(void)
{
    uint8_t sta_now;
    uint32_t now = HAL_GetTick();

    if(s_fp_ok == 0u) {
        if((now - s_last_hs) >= TEST_FP_HANDSHAKE_MS) {
            s_last_hs = now;
            if(test_fp_try_handshake() != 0u) {
                test_fp_read_info();
            }
        }
    }

    if((now - s_last_poll) < TEST_FP_POLL_MS) {
        return;
    }
    s_last_poll = now;

    sta_now = (GZ_Sta == GPIO_PIN_SET) ? 1u : 0u;
    if(sta_now != s_last_sta) {
        s_last_sta = sta_now;
        test_usb_uart_printf("[FP] STA=%u (%s)\r\n",
                             (unsigned)sta_now,
                             sta_now ? "finger?" : "idle");
    }
}
