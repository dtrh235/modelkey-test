/**
 * @file test_touch_diag.c
 * @brief FT6336 电容触屏硬件自检（I2C PB6/PB7, INT PB1, RST PE8）
 */

#include "test_touch_diag.h"
#include "test_hw_ui.h"
#include "test_rs485_log.h"
#include "app_config.h"

#include "./BSP/TOUCH/touch.h"
#include "./BSP/TOUCH/ft5206.h"
#include "./BSP/TOUCH/ctiic.h"
#include "./SYSTEM/delay/delay.h"
#include "stm32f4xx_hal.h"

#define TEST_TP_PROMPT_MS   3000u

static uint8_t s_tp_inited = 0u;
static uint8_t s_tp_hw_ok = 0u;
static uint32_t s_tp_last_prompt = 0u;
static uint8_t s_tp_was_down = 0u;
static uint32_t s_tp_down_count = 0u;

uint8_t test_touch_diag_hw_ok(void)
{
    return s_tp_hw_ok;
}

static void test_touch_rst_pulse(void)
{
    FT5206_RST(0);
    delay_ms(20);
    FT5206_RST(1);
    delay_ms(50);
}

static void test_touch_log_gpio_test(const char *tag, GPIO_TypeDef *scl_port, uint16_t scl_pin,
                                     GPIO_TypeDef *sda_port, uint16_t sda_pin)
{
    ct_iic_gpio_test_t gt;
    uint8_t addrs[8];
    uint8_t found = 0u;
    uint8_t i;
    uint8_t nack70;
    uint8_t id = 0u;

    ct_iic_bind(scl_port, scl_pin, sda_port, sda_pin, 0u);
    ct_iic_init();
    ct_iic_gpio_line_test(&gt);
    test_rs485_logf("[TP] %s GPIO PB6/PB7: SCL lo=%u hi=%u | SDA lo=%u hi=%u\r\n",
                    tag,
                    (unsigned)gt.scl_low, (unsigned)gt.scl_high,
                    (unsigned)gt.sda_low, (unsigned)gt.sda_high);

    if(gt.scl_low == 0u || gt.sda_low == 0u) {
        test_rs485_log_str("[TP] STEP1 FAIL: MCU cannot pull SCL/SDA low -> wrong pin/solder/short\r\n");
    } else if(gt.scl_high == 0u || gt.sda_high == 0u) {
        test_rs485_log_str("[TP] STEP1 FAIL: line stuck LOW -> short to GND or chip pulling bus\r\n");
    } else {
        test_rs485_log_str("[TP] STEP1 PASS: PB6/PB7 GPIO OK\r\n");
    }

    nack70 = ct_iic_probe_wr(FT5206_CMD_WR);
    (void)ct_iic_bus_scan(addrs, (uint8_t)sizeof(addrs), &found);
    if(found == 0u) {
        test_rs485_log_str("[TP] STEP2 FAIL: I2C scan found 0 devices\r\n");
    } else {
        test_rs485_logf("[TP] STEP2: I2C scan found %u dev, addrs:",
                        (unsigned)found);
        for(i = 0u; i < found; i++) {
            test_rs485_logf(" 0x%02X", (unsigned)addrs[i]);
        }
        test_rs485_log_str("\r\n");
    }

    ft5206_rd_reg(FT5206_CHIP_ID_REG, &id, 1);
    test_rs485_logf("[TP] %s nack70=%u(0=ACK) id=0x%02X INT=%u RST=%u\r\n",
                    tag,
                    (unsigned)nack70,
                    (unsigned)id,
                    (unsigned)(FT5206_INT == 0 ? 0u : 1u),
                    (unsigned)(HAL_GPIO_ReadPin(FT5206_RST_GPIO_PORT, FT5206_RST_GPIO_PIN) == GPIO_PIN_SET ? 1u : 0u));
}

static void test_touch_log_bus(const char *tag)
{
    test_touch_log_gpio_test(tag, GPIOB, GPIO_PIN_6, GPIOB, GPIO_PIN_7);
}

void test_touch_diag_run_once(void)
{
    uint8_t chip_id = 0u;
    uint8_t init_ret;

    test_rs485_log_str("\r\n[TP] ===== touch hardware diag =====\r\n");
    test_rs485_log_str("[TP] FT6336 I2C PB6=SCL PB7=SDA INT=PB1 RST=PE8\r\n");
    test_rs485_log_str("[TP] note: tp_init ret always 0 in test build; trust STEP1/2 below\r\n");

    {
        GPIO_InitTypeDef gpio_init_struct;
        FT5206_RST_GPIO_CLK_ENABLE();
        FT5206_INT_GPIO_CLK_ENABLE();
        gpio_init_struct.Pin = FT5206_RST_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(FT5206_RST_GPIO_PORT, &gpio_init_struct);
        gpio_init_struct.Pin = FT5206_INT_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_INPUT;
        gpio_init_struct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(FT5206_INT_GPIO_PORT, &gpio_init_struct);
        test_touch_rst_pulse();
    }

    test_touch_log_gpio_test("pre_init", GPIOB, GPIO_PIN_6, GPIOB, GPIO_PIN_7);
    test_rs485_log_str("[TP] try swapped SCL/SDA (PB7=SCL PB6=SDA):\r\n");
    test_touch_log_gpio_test("swap", GPIOB, GPIO_PIN_7, GPIOB, GPIO_PIN_6);

    init_ret = tp_dev.init();
    s_tp_inited = 1u;

    test_rs485_logf("[TP] tp_init ret=%u cap=%u scan_fn=%u\r\n",
                    (unsigned)init_ret,
                    (unsigned)((tp_dev.touchtype & 0x80u) ? 1u : 0u),
                    (unsigned)(tp_dev.scan != 0));

    test_touch_log_bus("after_init");

    (void)tp_cap_chip_id_read(&chip_id);
    (void)tp_dev.scan(0);

    test_rs485_logf("[TP] chip_id=0x%02X sta=0x%04X xy=%u,%u\r\n",
                    (unsigned)chip_id,
                    (unsigned)tp_dev.sta,
                    (unsigned)tp_dev.x[0],
                    (unsigned)tp_dev.y[0]);

    if(ct_iic_probe_wr(FT5206_CMD_WR) != 0u) {
        s_tp_hw_ok = 0u;
        test_rs485_log_str("[TP] FAIL: FT6336 no I2C ACK (addr 0x70)\r\n");
        test_rs485_log_str("[TP] FIX-A: STEP1 fail -> check PB6/PB7 solder to MCU\r\n");
        test_rs485_log_str("[TP] FIX-B: STEP1 OK scan 0 -> add 4.7k pull-up on SCL+SDA to 3.3V\r\n");
        test_rs485_log_str("[TP] FIX-C: swap OK but normal fail -> SCL/SDA reversed on FPC\r\n");
        test_rs485_log_str("[TP] FIX-D: check touch 3.3V/GND and FPC seated; RST=PE8\r\n");
    } else {
        s_tp_hw_ok = 1u;
        if(chip_id == 0x51u) {
            test_rs485_log_str("[TP] PASS hardware: FT6336 id=0x51 I2C OK\r\n");
        } else {
            test_rs485_logf("[TP] WARN hardware: I2C ACK OK id=0x%02X (部分模组 A3=0 仍可触摸)\r\n",
                            (unsigned)chip_id);
        }
    }

#if (TEST_ENABLE_LCD_HW != 0)
    test_hw_ui_show_touch_hw(s_tp_hw_ok, chip_id);
#endif

    test_rs485_log_str("[TP] ----- touch test: finger on screen -----\r\n");
    test_rs485_log_str("[TP] expect: [TP] DOWN x=.. y=..  then  [TP] UP\r\n");
    test_rs485_log_str("[TP] coords x~0-240 y~0-320 (portrait)\r\n");

    s_tp_last_prompt = HAL_GetTick();
}

void test_touch_diag_poll(void)
{
    uint8_t down;
    uint32_t now;

    if(s_tp_inited == 0u) {
        return;
    }

    now = HAL_GetTick();
    tp_dev.scan(0);
    down = (tp_dev.sta & TP_PRES_DOWN) ? 1u : 0u;

    if(down != 0u) {
        if(s_tp_was_down == 0u) {
            s_tp_down_count++;
            test_rs485_logf("[TP] DOWN #%lu x=%u y=%u sta=0x%04X INT=%u\r\n",
                            (unsigned long)s_tp_down_count,
                            (unsigned)tp_dev.x[0],
                            (unsigned)tp_dev.y[0],
                            (unsigned)tp_dev.sta,
                            (unsigned)(FT5206_INT == 0 ? 0u : 1u));
#if (TEST_ENABLE_LCD_HW != 0)
            test_hw_ui_touch_event(tp_dev.x[0], tp_dev.y[0], 1u);
#endif
        }
        s_tp_was_down = 1u;
    } else if(s_tp_was_down != 0u) {
        test_rs485_logf("[TP] UP   x=%u y=%u\r\n",
                        (unsigned)tp_dev.x[0],
                        (unsigned)tp_dev.y[0]);
#if (TEST_ENABLE_LCD_HW != 0)
        test_hw_ui_touch_event(tp_dev.x[0], tp_dev.y[0], 0u);
#endif
        s_tp_was_down = 0u;
    }

    if((now - s_tp_last_prompt) >= TEST_TP_PROMPT_MS) {
        s_tp_last_prompt = now;
        test_rs485_log_str("[TP] waiting touch...\r\n");
    }
}
