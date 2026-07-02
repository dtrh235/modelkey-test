/**
 * @file test_touch_la.c
 * @brief 触屏 I2C 连续探测，便于逻辑分析仪抓 PB6/PB7（及 PB1/PE8）
 */

#include "test_touch_la.h"

#include "app_config.h"
#include "test_rs485_log.h"

#include "./BSP/TOUCH/ctiic.h"
#include "./BSP/TOUCH/ft5206.h"
#include "./SYSTEM/delay/delay.h"
#include "stm32f4xx_hal.h"

#ifndef TEST_TOUCH_LA_PERIOD_MS
#define TEST_TOUCH_LA_PERIOD_MS    200u
#endif
#ifndef TEST_TOUCH_RST_PULSE_MS
#define TEST_TOUCH_RST_PULSE_MS    5000u
#endif
#ifndef TEST_TOUCH_LA_SCAN_EVERY
#define TEST_TOUCH_LA_SCAN_EVERY   10u
#endif

static uint32_t s_last_cycle;
static uint32_t s_last_rst;
static uint32_t s_last_log;
static uint32_t s_cycle_count;

static void test_touch_la_rst_pulse(void)
{
    FT5206_RST(0);
    delay_ms(20);
    FT5206_RST(1);
    delay_ms(50);
}

static void test_touch_la_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    FT5206_RST_GPIO_CLK_ENABLE();
    FT5206_INT_GPIO_CLK_ENABLE();

    gpio.Pin = FT5206_RST_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(FT5206_RST_GPIO_PORT, &gpio);

    gpio.Pin = FT5206_INT_GPIO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(FT5206_INT_GPIO_PORT, &gpio);

    FT5206_RST(1);
}

static void test_touch_la_log_banner(void)
{
    test_rs485_log_str("\r\n========================================\r\n");
    test_rs485_log_str("[TP-LA] touch I2C logic-analyzer mode\r\n");
    test_rs485_log_str("[TP-LA] probe: CH0=PB6 SCL  CH1=PB7 SDA\r\n");
    test_rs485_log_str("[TP-LA]       CH2=PB1 INT   CH3=PE8 RST\r\n");
    test_rs485_logf("[TP-LA] I2C bit delay=%uus period=%ums\r\n",
                    (unsigned)CT_IIC_DELAY_US,
                    (unsigned)TEST_TOUCH_LA_PERIOD_MS);
    test_rs485_log_str("[TP-LA] decoder: I2C  7-bit addr 0x38 (WR=0x70 RD=0x71)\r\n");
    test_rs485_log_str("[TP-LA] expect each cycle:\r\n");
    test_rs485_log_str("[TP-LA]   START + 0x70 + ACK + 0xA3 + ACK + read + NACK + STOP\r\n");
    test_rs485_log_str("[TP-LA] your board now: 0x70 NACK (no chip on bus)\r\n");
    test_rs485_log_str("[TP-LA] every 5s: PE8 RST low 20ms (see CH3)\r\n");
    test_rs485_log_str("========================================\r\n");
}

void test_touch_la_run_once(void)
{
    s_last_cycle = HAL_GetTick();
    s_last_rst = s_last_cycle;
    s_last_log = s_last_cycle;
    s_cycle_count = 0u;

    test_touch_la_gpio_init();
    test_touch_la_rst_pulse();
    ct_iic_init();

#if (TEST_HW_LOG_ENABLE != 0)
    test_touch_la_log_banner();
#endif
}

void test_touch_la_poll(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t nack70;
    uint8_t chip_id = 0xFFu;
    uint8_t ack0 = 1u;
    uint8_t ack1 = 1u;
    uint8_t ack2 = 1u;
    uint8_t addrs[8];
    uint8_t found = 0u;
    uint8_t i;

    if((now - s_last_cycle) < TEST_TOUCH_LA_PERIOD_MS) {
        return;
    }
    s_last_cycle = now;
    s_cycle_count++;

    if((now - s_last_rst) >= TEST_TOUCH_RST_PULSE_MS) {
        s_last_rst = now;
#if (TEST_HW_LOG_ENABLE != 0)
        test_rs485_log_str("[TP-LA] RST pulse PE8 low 20ms\r\n");
#endif
        test_touch_la_rst_pulse();
        ct_iic_init();
    }

    /* 逻辑分析仪上最易辨认的一帧：写 0x70 探测 + 读寄存器 0xA3 */
    nack70 = ct_iic_probe_wr(FT5206_CMD_WR);
    (void)ct_iic_rd_reg_trace(FT5206_CMD_WR, FT5206_CMD_RD,
                              FT5206_CHIP_ID_REG, &chip_id, 1u,
                              &ack0, &ack1, &ack2);

    if((s_cycle_count % TEST_TOUCH_LA_SCAN_EVERY) == 0u) {
        (void)ct_iic_bus_scan(addrs, (uint8_t)sizeof(addrs), &found);
    }

    if((now - s_last_log) >= 2000u) {
        s_last_log = now;
#if (TEST_HW_LOG_ENABLE != 0)
        test_rs485_logf("[TP-LA] #%lu nack70=%u ack(w/reg/r)=%u/%u/%u id=0x%02X INT=%u SCL=%u SDA=%u",
                        (unsigned long)s_cycle_count,
                        (unsigned)nack70,
                        (unsigned)ack0, (unsigned)ack1, (unsigned)ack2,
                        (unsigned)chip_id,
                        (unsigned)(FT5206_INT == 0 ? 0u : 1u),
                        (unsigned)ct_iic_pin_read_scl(),
                        (unsigned)ct_iic_pin_read_sda());
        if(found != 0u) {
            test_rs485_log_str(" scan:");
            for(i = 0u; i < found; i++) {
                test_rs485_logf(" 0x%02X", (unsigned)addrs[i]);
            }
        }
        test_rs485_log_str("\r\n");
        if(nack70 != 0u) {
            test_rs485_log_str("[TP-LA] LA: 0x70 NACK -> SDA stays HIGH at 9th SCL bit\r\n");
        } else if(chip_id == 0x51u) {
            test_rs485_log_str("[TP-LA] LA: chip OK, finger may pull INT(PB1) low\r\n");
        }
#endif
    }
}
