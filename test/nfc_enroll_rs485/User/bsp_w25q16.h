#ifndef BSP_W25Q16_H
#define BSP_W25Q16_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

/*
 * test 工程：W25Q16 软 SPI，PE0=CS PB3/4/5（仅 test/User/bsp_w25q16.c 使用）
 */

typedef struct {
    uint8_t jedec1[3];
    uint8_t jedec2[3];
    uint8_t miso_idle;
    uint8_t cs_level;
    uint8_t sck_level;
    uint8_t sck_lo;    /* 1: drive SCK low, pin reads 0 */
    uint8_t sck_hi;
    uint8_t mosi_lo;
    uint8_t mosi_hi;
    uint8_t cs_lo;
    uint8_t cs_hi;
} bsp_w25q16_hw_diag_t;

bool bsp_w25q16_init(void);
void bsp_w25q16_end_session(void);
uint32_t bsp_w25q16_read_jedec_id(void);
/* 不保持会话，仅探测 JEDEC（调试用） */
uint32_t bsp_w25q16_probe_jedec_id(void);
/* 不依赖 init 成功：释放 JTAG 占用并读两次原始 JEDEC，供自检日志 */
void bsp_w25q16_hw_diag(bsp_w25q16_hw_diag_t *out);

bool bsp_w25q16_read(uint32_t addr, uint8_t *buf, size_t len);
bool bsp_w25q16_write_page(uint32_t addr, const uint8_t *buf, size_t len);
bool bsp_w25q16_erase_sector_4k(uint32_t addr);

#endif
