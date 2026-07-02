/**
 * @file test_hw_all.c
 * @brief PCB4_5 核心模块上电一次性自检 + 汇总表
 */

#include "test_hw_all.h"

#include <stdio.h>

#include "app_config.h"
#include "test_usb_uart.h"
#include "test_w25q.h"
#include "test_nfc.h"
#include "test_fp.h"
#include "test_wifi.h"
#include "test_touch_diag.h"
#include "test_hw_ui.h"
#include "./BSP/LCD/lcd.h"
#include "./SYSTEM/delay/delay.h"
#include "stm32f4xx_hal.h"

#define TEST_HW_MOD_COUNT  6u

typedef struct {
    const char *name;
    uint8_t pass;   /* 1=PASS 0=FAIL */
    uint8_t ran;    /* 1=已测 */
} test_hw_mod_t;

static test_hw_mod_t s_mod[TEST_HW_MOD_COUNT];
static uint8_t s_suite_done;

static void test_hw_mark(uint8_t idx, const char *name, uint8_t pass)
{
    if(idx >= TEST_HW_MOD_COUNT) {
        return;
    }
    s_mod[idx].name = name;
    s_mod[idx].pass = pass;
    s_mod[idx].ran = 1u;
}

static uint8_t test_hw_lcd_once(void)
{
#if (TEST_ENABLE_LCD_HW != 0)
    test_usb_uart_print("\r\n[LCD] ----- ILI9341 SPI1 PA5/6/7 PE4/6/7 -----\r\n");
    test_hw_ui_lcd_run();
    test_usb_uart_printf("[LCD] id=0x%04X w=%u h=%u\r\n",
                         (unsigned)lcddev.id,
                         (unsigned)lcddev.width,
                         (unsigned)lcddev.height);
    if(lcddev.id == 0u || lcddev.id == 0xFFFFu) {
        test_usb_uart_print("[LCD] FAIL read chip id\r\n");
        return 0u;
    }
    test_usb_uart_print("[LCD] PASS color bars + id OK\r\n");
    return 1u;
#else
    test_usb_uart_print("[LCD] SKIP (TEST_ENABLE_LCD_HW=0)\r\n");
    return 1u;
#endif
}

static void test_hw_print_summary(void)
{
    uint8_t i;
    uint8_t fail_n = 0u;
    uint8_t ran_n = 0u;

    test_usb_uart_print("\r\n========================================\r\n");
    test_usb_uart_print(" HW SELF-TEST SUMMARY\r\n");
    test_usb_uart_print("========================================\r\n");

    for(i = 0u; i < TEST_HW_MOD_COUNT; i++) {
        if(s_mod[i].ran == 0u) {
            continue;
        }
        ran_n++;
        if(s_mod[i].pass == 0u) {
            fail_n++;
        }
        test_usb_uart_printf(" %-14s %s\r\n",
                             s_mod[i].name,
                             s_mod[i].pass != 0u ? "PASS" : "FAIL");
    }

    test_usb_uart_print("----------------------------------------\r\n");
    test_usb_uart_printf(" tested=%u  fail=%u\r\n",
                         (unsigned)ran_n, (unsigned)fail_n);
    if(fail_n == 0u) {
        test_usb_uart_print(" ALL AUTO CHECKS PASS\r\n");
    } else {
        test_usb_uart_print(" FIX modules marked FAIL above\r\n");
    }
    test_usb_uart_print("========================================\r\n");
    test_usb_uart_print(" interactive: FP STA / touch / NFC card\r\n");
    test_usb_uart_print("========================================\r\n\r\n");
}

void test_hw_all_run_once(void)
{
    uint8_t ok;

    if(s_suite_done != 0u) {
        return;
    }
    s_suite_done = 1u;

    test_usb_uart_print("\r\n########################################\r\n");
    test_usb_uart_print("# PCB4_5 HW SELF-TEST (core modules)   #\r\n");
    test_usb_uart_print("# log: USB-RS485 H11 115200 8N1        #\r\n");
    test_usb_uart_print("########################################\r\n");

    ok = test_hw_lcd_once();
    test_hw_mark(0u, "LCD", ok);

    ok = test_w25q_run_once();
    test_hw_mark(1u, "W25Q16", ok);

    test_touch_diag_run_once();
    ok = test_touch_diag_hw_ok();
    test_hw_mark(2u, "Touch", ok);

    ok = test_nfc_run_once();
    test_hw_mark(3u, "NFC", ok);

    test_fp_init();
    ok = test_fp_hw_ok();
    test_hw_mark(4u, "FP AS608", ok);

    test_wifi_board_early_gpio();
    ok = test_wifi_run_boot_once();
    test_hw_mark(5u, "WiFi WF24", ok);

    test_hw_print_summary();
}

void test_hw_all_poll(void)
{
    test_fp_poll();
    test_touch_diag_poll();
    test_nfc_poll();
}
