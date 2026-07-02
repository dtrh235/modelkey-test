/**
 * @file test_nfc.c
 * @brief MFRC522 NFC 自检（CN10 软 SPI PB12-15 + PD1 RST）
 */

#include "test_nfc.h"

#include <stdio.h>

#include "app_config.h"
#include "test_usb_uart.h"
#include "app_nfc_hw.h"
#include "MFRC522.h"
#include "door.h"
#include "./SYSTEM/delay/delay.h"
#include "stm32f4xx_hal.h"

static uint8_t s_nfc_ok;
static uint32_t s_last_card_log;

uint8_t test_nfc_run_once(void)
{
    uint8_t ver;

    test_usb_uart_print("\r\n[NFC] ----- MFRC522 CN10 -----\r\n");
    test_usb_uart_print("[NFC] PB12=NSS PB13=SCK PB15=MOSI PB14=MISO PD1=RST\r\n");

    app_nfc_hw_init_once();
    ver = Read_MFRC522(0x37u);
    test_usb_uart_printf("[NFC] VersionReg=0x%02X (OK=0x91/0x92)\r\n", (unsigned)ver);

    if(ver == 0x91u || ver == 0x92u) {
        s_nfc_ok = 1u;
        test_usb_uart_print("[NFC] PASS chip communication\r\n");
        test_usb_uart_print("[NFC] swipe card -> expect [NFC] CARD UID=...\r\n");
        return 1u;
    }

    s_nfc_ok = 0u;
    test_usb_uart_printf("[NFC] FAIL ver=0x%02X reset_ret=%u\r\n",
                         (unsigned)ver, (unsigned)app_nfc_last_reset_ret());
    test_usb_uart_print("[NFC] check CN10 power, NSS/SCK/MOSI/MISO/RST\r\n");
    return 0u;
}

void test_nfc_poll(void)
{
    uint8_t uid[4];
    uint32_t now;

    if(s_nfc_ok == 0u) {
        return;
    }

    if(NFC_Read_UID(uid) == 0u) {
        return;
    }

    now = HAL_GetTick();
    if((now - s_last_card_log) < 800u) {
        return;
    }
    s_last_card_log = now;

    test_usb_uart_printf("[NFC] CARD UID=%02X%02X%02X%02X\r\n",
                         (unsigned)uid[0], (unsigned)uid[1],
                         (unsigned)uid[2], (unsigned)uid[3]);
}
