/**
 * @file test_hw_ui.c
 * @brief LCD 显示 + 触屏可视化自检（不依赖 RS485 日志）
 */

#include "test_hw_ui.h"

#include <stdio.h>

#include "./BSP/LCD/lcd.h"
#include "./SYSTEM/delay/delay.h"

#define TEST_UI_STATUS_Y   40u
#define TEST_UI_TOUCH_Y    120u

static void test_ui_draw_status_line(uint16_t y, const char *text, uint16_t color)
{
    lcd_fill(8u, y, (uint16_t)(lcddev.width - 9u), (uint16_t)(y + 15u), WHITE);
    lcd_show_string(10u, y, (uint16_t)(lcddev.width - 20u), 16u, 16u, (char *)text, color);
}

void test_hw_ui_lcd_run(void)
{
    char line[40];

    lcd_init();

    lcd_fill(0u, 0u, 79u, (uint16_t)(lcddev.height - 1u), RED);
    delay_ms(250);
    lcd_fill(80u, 0u, 159u, (uint16_t)(lcddev.height - 1u), GREEN);
    delay_ms(250);
    lcd_fill(160u, 0u, (uint16_t)(lcddev.width - 1u), (uint16_t)(lcddev.height - 1u), BLUE);
    delay_ms(250);

    lcd_clear(WHITE);
    lcd_show_string(10u, 8u, 220u, 24u, 16u, "HW TEST", RED);
    snprintf(line, sizeof(line), "LCD id=0x%04X", (unsigned)lcddev.id);
    test_ui_draw_status_line(TEST_UI_STATUS_Y, line, BLUE);
    test_ui_draw_status_line((uint16_t)(TEST_UI_STATUS_Y + 20u), "LCD: color bars OK", GREEN);
    test_ui_draw_status_line((uint16_t)(TEST_UI_TOUCH_Y - 20u), "Touch: initializing...", GRAY);
    lcd_draw_rectangle(0u, 0u, (uint16_t)(lcddev.width - 1u), (uint16_t)(lcddev.height - 1u), GRAY);
}

void test_hw_ui_show_touch_hw(uint8_t i2c_ok, uint8_t chip_id)
{
    char line[40];

    if(i2c_ok != 0u) {
        snprintf(line, sizeof(line), "TP I2C OK id=0x%02X", (unsigned)chip_id);
        test_ui_draw_status_line(TEST_UI_TOUCH_Y, line, GREEN);
        test_ui_draw_status_line((uint16_t)(TEST_UI_TOUCH_Y + 20u), "Finger: draw dots", BLUE);
    } else {
        test_ui_draw_status_line(TEST_UI_TOUCH_Y, "TP I2C FAIL", RED);
        test_ui_draw_status_line((uint16_t)(TEST_UI_TOUCH_Y + 20u), "PB6/7 FPC 3V3 pull-up", RED);
    }
}

void test_hw_ui_touch_event(uint16_t x, uint16_t y, uint8_t down)
{
    char line[40];
    uint16_t color;

    if(down != 0u) {
        lcd_fill_circle(x, y, 6u, RED);
        lcd_draw_hline((uint16_t)(x > 8u ? x - 8u : 0u), y, 16u, BLUE);
        lcd_draw_line(x, (uint16_t)(y > 8u ? y - 8u : 0u), x, (uint16_t)(y + 8u), BLUE);
    }

    snprintf(line, sizeof(line), "x=%u y=%u %s", (unsigned)x, (unsigned)y,
             down != 0u ? "DOWN" : "UP  ");
    color = down != 0u ? RED : GRAY;
    test_ui_draw_status_line((uint16_t)(TEST_UI_TOUCH_Y + 40u), line, color);
}
