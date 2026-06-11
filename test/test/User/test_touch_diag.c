/**
 * @file test_touch_diag.c
 * @brief FT6336 电容触屏硬件自检（I2C PB6/PB7, INT PB1, RST PE8）
 */

#include "test_touch_diag.h"
#include "test_rs485_log.h"
#include "app_config.h"

#include "./BSP/TOUCH/touch.h"
#include "./BSP/TOUCH/ft5206.h"
#include "./BSP/TOUCH/ctiic.h"
#include "./SYSTEM/delay/delay.h"
#include "stm32f4xx_hal.h"

#define TEST_TP_PROMPT_MS   3000u

static uint8_t s_tp_inited = 0u;
static uint32_t s_tp_last_prompt = 0u;
static uint8_t s_tp_was_down = 0u;

static void test_touch_log_bus(const char *tag)
{
    uint8_t ack70;
    uint8_t ack28;
    uint8_t id = 0u;

    ct_iic_bind(GPIOB, GPIO_PIN_6, GPIOB, GPIO_PIN_7, 0u);
    ct_iic_init();
    ack70 = ct_iic_probe_wr(FT5206_CMD_WR);
    ack28 = ct_iic_probe_wr(0x28u);
    ft5206_rd_reg(FT5206_CHIP_ID_REG, &id, 1);

    test_rs485_logf("[TP] %s bus=%s ack70=%u ack28=%u SCL=%u SDA=%u INT=%u RST=%u id(A3)=0x%02X\r\n",
                    tag,
                    ct_iic_bus_name(),
                    (unsigned)ack70,
                    (unsigned)ack28,
                    (unsigned)ct_iic_pin_read_scl(),
                    (unsigned)ct_iic_pin_read_sda(),
                    (unsigned)(FT5206_INT == 0 ? 0u : 1u),
                    (unsigned)(HAL_GPIO_ReadPin(FT5206_RST_GPIO_PORT, FT5206_RST_GPIO_PIN) == GPIO_PIN_SET ? 1u : 0u),
                    (unsigned)id);
}

void test_touch_diag_run_once(void)
{
    uint8_t chip_id = 0u;
    uint8_t init_ret;

    test_rs485_log_str("\r\n[TP] ===== touch hardware diag =====\r\n");
    test_rs485_log_str("[TP] FT6336 I2C PB6=SCL PB7=SDA INT=PB1 RST=PE8\r\n");

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
        test_rs485_log_str("[TP] FAIL hardware: I2C no ACK on PB6/PB7 (wiring/chip/power)\r\n");
    } else if(chip_id == 0x51u) {
        test_rs485_log_str("[TP] PASS hardware: FT6336 id=0x51 I2C OK\r\n");
    } else {
        test_rs485_logf("[TP] WARN hardware: I2C OK but id=0x%02X (部分模组 A3=0 仍可触摸)\r\n",
                        (unsigned)chip_id);
    }

    test_rs485_log_str("[TP] touch screen -> expect [TP] DOWN x=.. y=..\r\n");
    test_rs485_log_str("[TP] no DOWN=HW fault; has DOWN but Host fail=UI(double-tap lock btn)\r\n");

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
            test_rs485_logf("[TP] DOWN x=%u y=%u sta=0x%04X INT=%u\r\n",
                            (unsigned)tp_dev.x[0],
                            (unsigned)tp_dev.y[0],
                            (unsigned)tp_dev.sta,
                            (unsigned)(FT5206_INT == 0 ? 0u : 1u));
        }
        s_tp_was_down = 1u;
    } else if(s_tp_was_down != 0u) {
        test_rs485_logf("[TP] UP   x=%u y=%u\r\n",
                        (unsigned)tp_dev.x[0],
                        (unsigned)tp_dev.y[0]);
        s_tp_was_down = 0u;
    }

    if((now - s_tp_last_prompt) >= TEST_TP_PROMPT_MS) {
        s_tp_last_prompt = now;
        test_rs485_log_str("[TP] waiting touch...\r\n");
    }
}
