#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* test/nfc_enroll_rs485：PCB4_5 全模块一次性自检，接线与 Host 主工程一致 */

#define APP_RS485_ENABLE           0
#define APP_NFC_ENABLE             1
#define APP_USE_FREERTOS           0
#define APP_CLOUD_ENABLE           0
#define APP_WIFI_MODEM_WF24        1

#define APP_DEBUG_ON_USART6        0
#define APP_DEBUG_UART_BAUD        115200u
#define APP_TOUCH_UART_DEBUG       0
#define APP_TP_FT6336_CAP_ONLY     1
#define APP_W25Q_DEBUG             0
#define APP_LCD_SKIP_BOOT_CLEAR    1
#define APP_LCD_SPI_PRESCALER      4   /* PCLK2=84MHz /4 ≈ 21MHz */
#define APP_LCD_HW_TEST            0
#define APP_LCD_FORCE_ST7789       0
#define APP_LCD_ID_UNKNOWN_USE_ST7789  0

/* 触屏/LCD 日志走 RS485 转 USB（与 test_usb_uart 同口） */
#define TEST_HW_LOG_USE_USB        1
#define TEST_HW_LOG_ENABLE         0

#define TEST_USB_UART_BAUD         115200u
#define TEST_USB_UART_HEARTBEAT_MS 10000u

#define TEST_ENABLE_LCD_HW         1

/* 指纹 CN7 USART3 PB10/PB11 WAK PE10 */
#define TEST_FP_UART_BAUD          57600u
#define TEST_FP_POLL_MS            500u
#define TEST_FP_HANDSHAKE_MS       2000u

/* WiFi WF24 H5 USART2 PA2/3 KEY PE12 */
#define TEST_WIFI_UART_BAUD        115200u
#define TEST_WIFI_KEY_LOW_MS       100u
#define TEST_WIFI_KEY_BOOT_MS      3000u
#define TEST_WIFI_POST_RESET_MS    2000u
#define TEST_WIFI_AT_LISTEN_MS     3000u
#define TEST_WIFI_AT_INTERVAL_MS   2000u
#define TEST_WIFI_KEY_RECOVER_N    5u

/* 继电器 H9 PE9 */
#define TEST_RELAY_ON_MS           400u
#define TEST_RELAY_OFF_MS          600u

/* 蜂鸣器 PC9 */
#define TEST_BUZZER_ON_MS          200u
#define TEST_BUZZER_OFF_MS         800u

/* 语音 UART4 PC10/11 */
#define TEST_VOICE_UART_BAUD       115200u
#define TEST_VOICE_TRIGGER_MS      3000u

#endif /* APP_CONFIG_H */
