#include "app_host_hw_selftest.h"

#include <stdio.h>

#include "app_config.h"
#include "./SYSTEM/usart/usart.h"
#include "../Drivers/BSP/W25Q16/bsp_w25q16.h"
#include "../Drivers/BSP/mfrc522/MFRC522.h"
#include "../Drivers/BSP/LCD/lcd.h"
#include "../Drivers/BSP/TOUCH/ft5206.h"
#include "app_nfc_hw.h"
#include "app_home_fp_poll.h"
#include "cloud_aliyun_at.h"

extern uint8_t g_fp_hw_inited;

#if (APP_RS485_ENABLE == 1)
#include "app_rs485_proto.h"
#endif

static void hw_line_ok(const char *name)
{
    char buf[48];
    (void)snprintf(buf, sizeof(buf), "[HW] %s OK\r\n", name);
    usart_debug_tx_str(buf);
}

static void hw_line_fail(const char *name, const char *hint)
{
    char buf[96];
    if(hint != NULL && hint[0] != '\0') {
        (void)snprintf(buf, sizeof(buf), "[HW] %s FAIL (%s)\r\n", name, hint);
    } else {
        (void)snprintf(buf, sizeof(buf), "[HW] %s FAIL\r\n", name);
    }
    usart_debug_tx_str(buf);
}

static void hw_test_relay(void)
{
    GPIO_PinState st = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_9);
    if(st == GPIO_PIN_RESET) {
        hw_line_ok("RELAY");
    } else {
        hw_line_fail("RELAY", "PE9/JDQ_IN should be low at boot");
    }
}

static void hw_test_w25q16(void)
{
    bsp_w25q16_hw_diag_t d;
    uint32_t jedec;

    bsp_w25q16_hw_diag(&d);
    jedec = bsp_w25q16_probe_jedec_id();
    if(jedec == 0xEF4015u) {
        hw_line_ok("W25Q16");
    } else {
        char hint[48];
        (void)snprintf(hint, sizeof(hint), "JEDEC=0x%06lX PE0/PB3/4/5",
                       (unsigned long)(jedec & 0xFFFFFFu));
        hw_line_fail("W25Q16", hint);
    }
}

static void hw_line_skip(const char *name, const char *why)
{
    char buf[80];
    if(why != NULL && why[0] != '\0') {
        (void)snprintf(buf, sizeof(buf), "[HW] %s SKIP (%s)\r\n", name, why);
    } else {
        (void)snprintf(buf, sizeof(buf), "[HW] %s SKIP\r\n", name);
    }
    usart_debug_tx_str(buf);
}

static void hw_test_nfc(void)
{
#if (APP_NFC_ENABLE != 0)
    uint8_t ver;

    app_nfc_hw_init_once();
    ver = Read_MFRC522(0x37u);
    if(ver == 0x91u || ver == 0x92u) {
        hw_line_ok("NFC");
    } else {
        char hint[32];
        (void)snprintf(hint, sizeof(hint), "ver=0x%02X expect 91/92", (unsigned)ver);
        hw_line_fail("NFC", hint);
    }
#else
    hw_line_skip("NFC", "not soldered");
#endif
}

static void hw_test_fp(void)
{
#if (APP_TEMP_DISABLE_BIOMETRIC == 0)
    if(app_fp_hw_try_handshake_retries(&g_fp_hw_inited, 1u, 0u) != 0u) {
        hw_line_ok("FP");
    } else {
        hw_line_fail("FP", "CN7 USART3 PB10/PB11 power?");
    }
#else
    hw_line_fail("FP", "biometric disabled");
#endif
}

static void hw_test_wifi(void)
{
#if (APP_CLOUD_ENABLE != 0) && (APP_ALIYUN_AT_ENABLE != 0)
    if(cloud_uart2_try_modem_ready_timeout(400u) != 0u) {
        hw_line_ok("WIFI");
    } else {
        hw_line_fail("WIFI", "H5 PA2/PA3 PE12 power?");
    }
#else
    hw_line_fail("WIFI", "cloud disabled");
#endif
}

static void hw_test_rs485(void)
{
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
    app_rs485_flush_rx();
    if(app_rs485_proto_ping_peer(400u) != 0) {
        hw_line_ok("RS485");
    } else {
        hw_line_fail("RS485", "no slave ACK (host-only OK to ignore)");
    }
    app_rs485_flush_rx();
#else
    hw_line_skip("RS485", "disabled");
#endif
}

static void hw_test_touch(void)
{
    if(ft5206_init() == 0u) {
        hw_line_ok("TOUCH");
    } else {
        hw_line_fail("TOUCH", "FPC I2C PB6/PB7 or PE8 RST");
    }
}

void app_host_hw_selftest_run_pre_ui(void)
{
    usart_debug_tx_str("\r\n======== Host HW self-test ========\r\n");
    usart_debug_tx_str("[HW] log: RS485 H11 DI_485 PC6/7/8 115200 8N1\r\n");

    hw_test_relay();

#if (APP_HOST_HW_SELFTEST_DEFER_SLOW == 0)
    hw_test_w25q16();
    hw_test_nfc();
    hw_test_fp();
    hw_test_wifi();
    hw_test_rs485();
#endif

    usart_debug_tx_str("[HW] (LCD/Touch after display init)\r\n");
}

void app_host_hw_selftest_run_deferred(void)
{
#if (APP_HOST_HW_SELFTEST != 0) && (APP_HOST_HW_SELFTEST_DEFER_SLOW != 0)
    usart_debug_tx_str("[HW] deferred slow checks...\r\n");
    hw_test_w25q16();
    hw_test_nfc();
    hw_test_fp();
    hw_test_wifi();
    hw_test_rs485();
    hw_test_touch();
    usart_debug_tx_str("======== HW self-test done ========\r\n\r\n");
#else
    (void)0;
#endif
}

void app_host_hw_selftest_run_post_lcd(void)
{
    usart_debug_tx_str("[HW] post-lcd check...\r\n");

    if(lcddev.id == 0x9341u) {
        hw_line_ok("LCD");
    } else {
        char hint[32];
        (void)snprintf(hint, sizeof(hint), "id=0x%04X", (unsigned)lcddev.id);
        hw_line_fail("LCD", hint);
    }

#if (APP_HOST_HW_SELFTEST_DEFER_SLOW == 0)
    if(ft5206_init() == 0u) {
        hw_line_ok("TOUCH");
    } else {
        hw_line_fail("TOUCH", "FPC I2C PB6/PB7 or PE8 RST");
    }
    usart_debug_tx_str("======== HW self-test done ========\r\n\r\n");
#else
    usart_debug_tx_str("[HW] TOUCH deferred until UI up\r\n");
#endif
}
