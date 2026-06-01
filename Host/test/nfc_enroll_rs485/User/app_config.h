#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* NFC + RS485 硬件测试工程：仅保留录入测试所需宏 */

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

#endif /* APP_CONFIG_H */
