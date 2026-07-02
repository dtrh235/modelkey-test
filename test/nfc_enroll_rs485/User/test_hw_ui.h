#ifndef TEST_HW_UI_H
#define TEST_HW_UI_H

#include <stdint.h>

void test_hw_ui_lcd_run(void);
void test_hw_ui_show_touch_hw(uint8_t i2c_ok, uint8_t chip_id);
void test_hw_ui_touch_event(uint16_t x, uint16_t y, uint8_t down);

#endif /* TEST_HW_UI_H */
