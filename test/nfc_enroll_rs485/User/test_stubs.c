/**
 * @file test_stubs.c
 * @brief 测试工程桩：at24cxx（FT6336 不用 EEPROM；lcddev 由 lcd.c 提供）
 */

#include "./BSP/24CXX/24cxx.h"
#include <string.h>

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
