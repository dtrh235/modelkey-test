/**
 * @file test_w25q.c
 * @brief W25Q16 外部 Flash 自检（PE0 CS + PB3/4/5 软 SPI，与 Host 一致）
 */

#include "test_w25q.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "test_usb_uart.h"
#include "bsp_w25q16.h"
#include "./SYSTEM/delay/delay.h"

uint8_t test_w25q_run_once(void)
{
    uint32_t jedec;
    uint8_t wr[16];
    uint8_t rd[16];
    uint32_t addr = 0x10000u;
    uint8_t i;
    uint8_t ok = 1u;

    test_usb_uart_print("\r\n[W25Q] ----- ext Flash (PE0/PB3/4/5) -----\r\n");

    if(!bsp_w25q16_init()) {
        jedec = bsp_w25q16_probe_jedec_id();
        test_usb_uart_printf("[W25Q] FAIL init JEDEC=0x%06lX (expect 0xEF4015)\r\n",
                             (unsigned long)(jedec & 0xFFFFFFu));
        return 0u;
    }

    jedec = bsp_w25q16_read_jedec_id();
    test_usb_uart_printf("[W25Q] JEDEC=0x%06lX\r\n", (unsigned long)(jedec & 0xFFFFFFu));
    if(jedec != 0xEF4015u) {
        test_usb_uart_print("[W25Q] FAIL JEDEC mismatch\r\n");
        return 0u;
    }

    for(i = 0u; i < sizeof(wr); i++) {
        wr[i] = (uint8_t)(0xA5u + i);
    }
    memset(rd, 0, sizeof(rd));

    if(!bsp_w25q16_erase_sector_4k(addr)) {
        test_usb_uart_print("[W25Q] FAIL sector erase\r\n");
        return 0u;
    }
    if(!bsp_w25q16_write_page(addr, wr, sizeof(wr))) {
        test_usb_uart_print("[W25Q] FAIL page program\r\n");
        return 0u;
    }
    if(!bsp_w25q16_read(addr, rd, sizeof(rd))) {
        test_usb_uart_print("[W25Q] FAIL read back\r\n");
        return 0u;
    }
    for(i = 0u; i < sizeof(wr); i++) {
        if(rd[i] != wr[i]) {
            ok = 0u;
            break;
        }
    }
    if(ok == 0u) {
        test_usb_uart_printf("[W25Q] FAIL data mismatch @0x%05lX byte %u\r\n",
                             (unsigned long)addr, (unsigned)i);
        return 0u;
    }

    test_usb_uart_print("[W25Q] PASS JEDEC + erase/write/read\r\n");
    return 1u;
}
