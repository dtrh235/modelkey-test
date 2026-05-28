#ifndef BSP_W25Q16_H
#define BSP_W25Q16_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

/*
 * 板载 W25Q16：PB3/4/5 + PA15 CS；软件模拟 SPI（与 Client 工程一致）
 * 可与屏显 SPI1(PA5/6/7) 同时存在，互不影响。
 */

bool bsp_w25q16_init(void);
void bsp_w25q16_end_session(void);
uint32_t bsp_w25q16_read_jedec_id(void);
/* 不保持会话，仅探测 JEDEC（调试用） */
uint32_t bsp_w25q16_probe_jedec_id(void);

bool bsp_w25q16_read(uint32_t addr, uint8_t *buf, size_t len);
bool bsp_w25q16_write_page(uint32_t addr, const uint8_t *buf, size_t len);
bool bsp_w25q16_erase_sector_4k(uint32_t addr);

#endif
