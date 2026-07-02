/**
 * @file test_wifi.c
 * @brief WF24 WiFi 连续 AT 探测（USART2 中断收包，与 Host cloud_aliyun_at 一致）
 *
 * H5: H5.2=USART2_TX  H5.5=USART2_RX  H5.4=KEY  H5.1=5V  H5.3=GND
 */

#include "test_wifi.h"

#include <string.h>

#include "app_config.h"
#include "test_usb_uart.h"
#include "./SYSTEM/delay/delay.h"
#include "stm32f4xx_hal.h"

#define WF24_KEY_GPIO_PORT   GPIOE
#define WF24_KEY_GPIO_PIN    GPIO_PIN_12

#define WIFI_RX_BUF_SZ       512u
#define WIFI2_RING_SZ        512u

static UART_HandleTypeDef s_wifi_uart;
static uint8_t s_wifi_ok;
static uint8_t s_key_gpio_ok;
static char s_wifi_rx[WIFI_RX_BUF_SZ];
static uint16_t s_wifi_rx_len;
static uint32_t s_last_at_ms;
static uint32_t s_at_seq;
static uint8_t s_modem_at_ok;
static uint32_t s_lifetime_rx_bytes;
static uint32_t s_no_reply_streak;
static volatile uint16_t s_wifi2_ring_head;
static volatile uint16_t s_wifi2_ring_tail;
static uint8_t s_wifi2_ring[WIFI2_RING_SZ];
static uint32_t s_wifi2_ore_cnt;

static void test_wifi_rx_clear(void);
static void test_wifi_log_payload(const char *prefix, const char *buf, uint16_t len);
static void test_wifi_log_hex(const char *tag, const char *buf, uint16_t len);
static void test_wifi_log_boot_summary(const char *tag);
static void test_wifi2_ring_clear(void);
static void test_wifi2_irq_enable(void);
static uint16_t test_wifi2_pop_to_buf(void);

static void test_wifi2_ring_push(uint8_t b)
{
    uint16_t next = (uint16_t)((s_wifi2_ring_head + 1u) % WIFI2_RING_SZ);

    if(next != s_wifi2_ring_tail) {
        s_wifi2_ring[s_wifi2_ring_head] = b;
        s_wifi2_ring_head = next;
    }
}

static uint8_t test_wifi2_ring_pop(uint8_t *b)
{
    if(b == NULL || s_wifi2_ring_tail == s_wifi2_ring_head) {
        return 0u;
    }
    *b = s_wifi2_ring[s_wifi2_ring_tail];
    s_wifi2_ring_tail = (uint16_t)((s_wifi2_ring_tail + 1u) % WIFI2_RING_SZ);
    return 1u;
}

void USART2_IRQHandler(void)
{
    uint32_t sr;

    if(s_wifi_ok == 0u) {
        return;
    }
    sr = READ_REG(USART2->SR);
    if((sr & USART_SR_RXNE) != 0u) {
        test_wifi2_ring_push((uint8_t)(READ_REG(USART2->DR) & 0xFFu));
    }
    if((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) != 0u) {
        if((sr & USART_SR_ORE) != 0u) {
            s_wifi2_ore_cnt++;
        }
        (void)READ_REG(USART2->DR);
    }
}

static void test_wifi2_ring_clear(void)
{
    s_wifi2_ring_head = 0u;
    s_wifi2_ring_tail = 0u;
}

static void test_wifi2_irq_enable(void)
{
    __HAL_UART_DISABLE_IT(&s_wifi_uart, UART_IT_RXNE);
    while(__HAL_UART_GET_FLAG(&s_wifi_uart, UART_FLAG_RXNE) != RESET) {
        test_wifi2_ring_push((uint8_t)(READ_REG(USART2->DR) & 0xFFu));
    }
    __HAL_UART_CLEAR_OREFLAG(&s_wifi_uart);
    __HAL_UART_ENABLE_IT(&s_wifi_uart, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void wf24_key_gpio_init(void)
{
    if(s_key_gpio_ok != 0u) {
        return;
    }
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    gpio.Pin = WF24_KEY_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(WF24_KEY_GPIO_PORT, &gpio);

    HAL_GPIO_WritePin(WF24_KEY_GPIO_PORT, WF24_KEY_GPIO_PIN, GPIO_PIN_SET);
    s_key_gpio_ok = 1u;
}

void test_wifi_board_early_gpio(void)
{
    wf24_key_gpio_init();
}

static void wf24_key_set(uint8_t high)
{
    if(s_key_gpio_ok == 0u) {
        return;
    }
    HAL_GPIO_WritePin(WF24_KEY_GPIO_PORT, WF24_KEY_GPIO_PIN,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void test_wifi_hw_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    s_wifi_uart.Instance = USART2;
    s_wifi_uart.Init.BaudRate = TEST_WIFI_UART_BAUD;
    s_wifi_uart.Init.WordLength = UART_WORDLENGTH_8B;
    s_wifi_uart.Init.StopBits = UART_STOPBITS_1;
    s_wifi_uart.Init.Parity = UART_PARITY_NONE;
    s_wifi_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_wifi_uart.Init.Mode = UART_MODE_TX_RX;

    s_wifi_ok = (HAL_UART_Init(&s_wifi_uart) == HAL_OK) ? 1u : 0u;
    if(s_wifi_ok != 0u) {
        test_wifi2_ring_clear();
        s_wifi2_ore_cnt = 0u;
        test_wifi2_irq_enable();
    }
}

static void test_wifi_rx_clear(void)
{
    s_wifi_rx_len = 0u;
    s_wifi_rx[0] = '\0';
}

static void test_wifi_log_payload(const char *prefix, const char *buf, uint16_t len)
{
    uint16_t i;

    test_usb_uart_printf("%s %uB: ", prefix, (unsigned)len);
    for(i = 0u; i < len; i++) {
        char c = buf[i];
        if(c >= 32 && c < 127) {
            char one[2] = { c, '\0' };
            test_usb_uart_print(one);
        } else if(c == '\r' || c == '\n') {
            test_usb_uart_print("\\n");
        } else {
            test_usb_uart_printf("\\x%02X", (unsigned)(uint8_t)c);
        }
    }
    test_usb_uart_print("\r\n");
}

static void test_wifi_log_hex(const char *tag, const char *buf, uint16_t len)
{
    uint16_t i;

    test_usb_uart_printf("%s hex %uB:", tag, (unsigned)len);
    for(i = 0u; i < len; i++) {
        test_usb_uart_printf(" %02X", (unsigned)(uint8_t)buf[i]);
    }
    test_usb_uart_print("\r\n");
}

static uint16_t test_wifi2_pop_to_buf(void)
{
    uint8_t b;
    uint16_t n = 0u;

    while(test_wifi2_ring_pop(&b) != 0u) {
        n++;
        s_lifetime_rx_bytes++;
        if(s_wifi_rx_len < (WIFI_RX_BUF_SZ - 1u)) {
            s_wifi_rx[s_wifi_rx_len++] = (char)b;
            s_wifi_rx[s_wifi_rx_len] = '\0';
        }
    }
    return n;
}

static void test_wifi_log_boot_summary(const char *tag)
{
    (void)test_wifi2_pop_to_buf();

    if(s_wifi_rx_len == 0u) {
        test_usb_uart_printf("[WiFi-RX] %s summary: 0 bytes (silent)\r\n", tag);
        return;
    }

    test_usb_uart_printf("[WiFi-RX] %s summary: %u bytes\r\n", tag, (unsigned)s_wifi_rx_len);
    test_wifi_log_hex("[WiFi-RX] boot", s_wifi_rx, s_wifi_rx_len);
    test_wifi_log_payload("[WiFi-RX] boot txt", s_wifi_rx, s_wifi_rx_len);
}

static void wf24_key_hw_reset(void)
{
    uint32_t t0;

    test_wifi2_ring_clear();
    test_wifi_rx_clear();
    test_usb_uart_print("[WiFi] KEY reset: H5.4 low 100ms -> release -> boot wait\r\n");
    wf24_key_set(0u);
    delay_ms(TEST_WIFI_KEY_LOW_MS);
    wf24_key_set(1u);

    t0 = HAL_GetTick();
    while((HAL_GetTick() - t0) < TEST_WIFI_KEY_BOOT_MS) {
        (void)test_wifi2_pop_to_buf();
        delay_ms(5);
    }

    t0 = HAL_GetTick();
    while((HAL_GetTick() - t0) < TEST_WIFI_POST_RESET_MS) {
        (void)test_wifi2_pop_to_buf();
        delay_ms(5);
    }

    test_wifi_log_boot_summary("boot");
    test_wifi2_ring_clear();
    test_wifi_rx_clear();
}

static uint8_t test_wifi_send_at_and_listen(uint32_t seq)
{
    static const char k_at[] = "AT\r\n";
    uint32_t t0;

    test_wifi2_ring_clear();
    test_wifi_rx_clear();
    test_usb_uart_printf("[WiFi-TX] #%lu -> H5.2 USART2_TX: AT\\r\\n\r\n",
                         (unsigned long)seq);

    if(HAL_UART_Transmit(&s_wifi_uart, (uint8_t *)k_at, 4u, 200u) != HAL_OK) {
        test_usb_uart_print("[WiFi-TX] FAIL HAL transmit\r\n");
        return 0u;
    }

    t0 = HAL_GetTick();
    while((HAL_GetTick() - t0) < TEST_WIFI_AT_LISTEN_MS) {
        (void)test_wifi2_pop_to_buf();
        if(strstr(s_wifi_rx, "OK") != NULL) {
            s_modem_at_ok = 1u;
            s_no_reply_streak = 0u;
            test_usb_uart_printf("[WiFi] *** PASS AT OK (seq #%lu) ***\r\n",
                                 (unsigned long)seq);
            test_wifi_log_hex("[WiFi-RX] final", s_wifi_rx, s_wifi_rx_len);
            test_wifi_log_payload("[WiFi-RX] final txt", s_wifi_rx, s_wifi_rx_len);
            return 1u;
        }
        delay_ms(2);
    }

    (void)test_wifi2_pop_to_buf();

    test_usb_uart_printf("[WiFi-DIA] #%lu rx=%uB ORE=%lu SR=0x%04lX\r\n",
                         (unsigned long)seq,
                         (unsigned)s_wifi_rx_len,
                         (unsigned long)s_wifi2_ore_cnt,
                         (unsigned long)(USART2->SR & 0xFFFFu));

    if(s_wifi_rx_len == 0u) {
        test_usb_uart_printf("[WiFi-RX] #%lu (empty)\r\n", (unsigned long)seq);
        s_no_reply_streak++;
    } else if(strstr(s_wifi_rx, "OK") != NULL) {
        s_modem_at_ok = 1u;
        s_no_reply_streak = 0u;
        test_usb_uart_printf("[WiFi] *** PASS AT OK (seq #%lu) ***\r\n",
                             (unsigned long)seq);
        test_wifi_log_hex("[WiFi-RX] final", s_wifi_rx, s_wifi_rx_len);
        return 1u;
    } else {
        test_wifi_log_hex("[WiFi-RX] raw", s_wifi_rx, s_wifi_rx_len);
        test_wifi_log_payload("[WiFi-RX] raw txt", s_wifi_rx, s_wifi_rx_len);
        if(s_wifi2_ore_cnt != 0u) {
            test_usb_uart_print("[WiFi-DIA] ORE>0: bytes lost before IRQ fix?\r\n");
        }
        test_usb_uart_printf("[WiFi-RX] #%lu no OK in buffer\r\n", (unsigned long)seq);
        s_no_reply_streak++;
    }
    return 0u;
}

void test_wifi_init(void)
{
    s_last_at_ms = 0u;
    s_at_seq = 0u;
    s_modem_at_ok = 0u;
    s_lifetime_rx_bytes = 0u;
    s_no_reply_streak = 0u;

    wf24_key_gpio_init();

    test_usb_uart_print("\r\n========================================\r\n");
    test_usb_uart_print("[WiFi] WF24 continuous AT test (WiFi only)\r\n");
    test_usb_uart_print("[WiFi] H5.2=USART2_TX  H5.5=USART2_RX  H5.4=KEY\r\n");
    test_usb_uart_print("[WiFi] USART2 RX=IRQ ring (same as Host)\r\n");
    test_usb_uart_print("[WiFi] RS485 log: [WiFi-TX] send  [WiFi-RX] reply\r\n");
    test_usb_uart_print("========================================\r\n");

    test_wifi_hw_init();
    if(s_wifi_ok != 0u) {
        test_usb_uart_printf("[WiFi] USART2 init OK baud=%lu BRR=0x%04lX\r\n",
                             (unsigned long)TEST_WIFI_UART_BAUD,
                             (unsigned long)USART2->BRR);
    } else {
        test_usb_uart_print("[WiFi] FAIL USART2 init\r\n");
        return;
    }

    wf24_key_hw_reset();
    test_usb_uart_print("[WiFi] enter continuous AT loop...\r\n\r\n");
}

void test_wifi_poll(void)
{
    uint32_t now = HAL_GetTick();

    (void)test_wifi2_pop_to_buf();

    if(s_modem_at_ok != 0u) {
        if((now - s_last_at_ms) < 5000u) {
            return;
        }
        s_last_at_ms = now;
        test_usb_uart_printf("[WiFi] alive OK, lifetime_rx=%lu bytes\r\n",
                             (unsigned long)s_lifetime_rx_bytes);
        return;
    }

    if((now - s_last_at_ms) < TEST_WIFI_AT_INTERVAL_MS) {
        return;
    }
    s_last_at_ms = now;
    s_at_seq++;

    if(s_no_reply_streak != 0u &&
       (s_no_reply_streak % TEST_WIFI_KEY_RECOVER_N) == 0u) {
        test_usb_uart_printf("[WiFi] no reply x%lu -> KEY reset\r\n",
                             (unsigned long)s_no_reply_streak);
        wf24_key_hw_reset();
    }

    (void)test_wifi_send_at_and_listen(s_at_seq);
}

uint8_t test_wifi_run_boot_once(void)
{
    s_last_at_ms = 0u;
    s_at_seq = 0u;
    s_modem_at_ok = 0u;
    s_lifetime_rx_bytes = 0u;
    s_no_reply_streak = 0u;

    test_usb_uart_print("\r\n[WiFi] ----- WF24 H5 USART2 PA2/3 KEY PE12 -----\r\n");

    wf24_key_gpio_init();
    test_wifi_hw_init();
    if(s_wifi_ok == 0u) {
        test_usb_uart_print("[WiFi] FAIL USART2 init\r\n");
        return 0u;
    }

    wf24_key_hw_reset();
    if(test_wifi_send_at_and_listen(1u) != 0u) {
        test_usb_uart_print("[WiFi] PASS boot AT OK\r\n");
        return 1u;
    }

    test_usb_uart_print("[WiFi] FAIL no AT OK (check H5 power/UART swap)\r\n");
    return 0u;
}
