#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* 焊板自检 + 触屏 + 开锁账号诊断（RS485 串口日志） */

#define APP_RS485_ENABLE           1
#define APP_DEBUG_ON_USART6        0
#define APP_RS485_UART_BAUD        115200u
#define APP_RS485_ROLE_MASTER      1u
#define APP_RS485_ROLE_SLAVE       2u
#define APP_RS485_NODE_ROLE        APP_RS485_ROLE_MASTER
#define APP_RS485_LOCAL_ADDR       0x01u
#define APP_RS485_PEER_ADDR        0x02u
#define APP_RS485_IS_SLAVE         0
#define APP_RS485_IS_MASTER        1

#define APP_USE_FREERTOS           0
#define APP_NFC_ENABLE             1
#define APP_CLOUD_ENABLE           0
#define APP_ALIYUN_AT_ENABLE       0
#define APP_HOST_RS485_DIAG        0
#define APP_W25Q_DEBUG             0

/* 软 SPI 位延时(us)：test 加大以降低 RC522 时钟 */
#define MFRC522_SOFT_SPI_DELAY_US  10u

/* 触屏：与 Host 主工程一致 FT6336 I2C PB6/PB7 */
#define APP_TOUCH_UART_DEBUG       0
#define APP_TP_FT6336_CAP_ONLY     1
#define APP_TP_MIRROR_X            0

/* 默认管理员（与 Host app_config.h 一致） */
#define ADMIN_DEFAULT_ACCOUNT      "1"
#define ADMIN_DEFAULT_PASSWORD     "1111"

/*
 * 一次性自检开关：某硬件确认正常后改为 0，上电跳过该项以缩短日志。
 * 软件诊断（开锁账号）不受此影响。
 */
#define TEST_ENABLE_W25Q_HW        1
#define TEST_ENABLE_NFC_HW         1
#define TEST_ENABLE_TOUCH_HW       1
#define TEST_ENABLE_UNLOCK_SW      1

#endif /* APP_CONFIG_H */
