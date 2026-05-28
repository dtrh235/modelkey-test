#ifndef BSP_W25Q16_H
#define BSP_W25Q16_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

/*
 * 板载 W25Q16：软件模拟 SPI（不占用 SPI 外设），引脚固定
 *   PB3=SCK, PB5=MOSI, PB4=MISO, PA15=软件 CS
 * 可与屏显 SPI1(PA5/6/7) 同时存在，互不影响。
 */

bool bsp_w25q16_init(void);
void bsp_w25q16_end_session(void);
uint32_t bsp_w25q16_read_jedec_id(void);

bool bsp_w25q16_read(uint32_t addr, uint8_t *buf, size_t len);
bool bsp_w25q16_write_page(uint32_t addr, const uint8_t *buf, size_t len);
bool bsp_w25q16_erase_sector_4k(uint32_t addr);

#endif
