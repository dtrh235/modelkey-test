/**
 * @file test_stubs.c
 * @brief 测试工程桩：lcddev + at24cxx（FT6336 不用 EEPROM 校准，但 touch.c 仍链接）
 */

#include "./BSP/LCD/lcd.h"
#include "./BSP/24CXX/24cxx.h"
#include <string.h>

_lcd_dev lcddev = {
    240u,   /* width  ILI9341 竖屏 */
    320u,   /* height */
    0x9341u,
    0u,
    0u,
    0u,
    0u
};

void at24cxx_read(uint16_t addr, uint8_t *pbuf, uint16_t datalen)
{
    if(pbuf != NULL && datalen > 0u) {
        memset(pbuf, 0, datalen);
    }
    (void)addr;
}

uint8_t at24cxx_read_one_byte(uint16_t addr)
{
    (void)addr;
    return 0u;
}

void at24cxx_write(uint16_t addr, uint8_t *pbuf, uint16_t datalen)
{
    (void)addr;
    (void)pbuf;
    (void)datalen;
}

void at24cxx_write_one_byte(uint16_t addr, uint8_t data)
{
    (void)addr;
    (void)data;
}
