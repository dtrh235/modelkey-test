#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Cloud switch: set to 1 to enable ESP8266 WiFi/MQTT (无固件 OTA). */
#define APP_CLOUD_ENABLE 1

/* Aliyun MQTT over ESP8266(AT) on USART2 PA2/PA3, RST=PA8 */
#define APP_ALIYUN_AT_ENABLE 1
#ifndef APP_ALIYUN_UART_BAUD
#define APP_ALIYUN_UART_BAUD     115200u
#endif
/* 0=仅固定波特率，避免 AT 失败时 115200/9600 来回切导致模组更乱 */
#ifndef APP_ALIYUN_BAUD_PROBE
#define APP_ALIYUN_BAUD_PROBE    0
#endif
#define APP_ALIYUN_PRODUCT_KEY     "k1vtgud0XCS"
#define APP_ALIYUN_DEVICE_NAME     "1111"
#define APP_ALIYUN_DEVICE_SECRET   "84f8b089dee226d9a7a84d7f439f2069"
#define APP_ALIYUN_REGION          "cn-shanghai"
/* Legacy defaults (unused when app_wifi_remember has entries). */
#define APP_ALIYUN_WIFI_SSID       ""
#define APP_ALIYUN_WIFI_PASSWORD   ""
/* 0: no hardcoded AP auto-join; use remembered WiFi + scr11 auto-connect */
#define APP_ALIYUN_WIFI_AUTO_JOIN  0
/* SNTP：模组 AT 校时（与 MQTT 不能同时发 AT，默认不在连 MQTT 前阻塞） */
#define APP_ALIYUN_SNTP_ENABLE     1
#define APP_ALIYUN_SNTP_TZ_HOURS   8
/* 0=先连阿里云 MQTT（快）；1=先 ESP SNTP 再 MQTT（慢，约 20s+） */
#ifndef APP_ALIYUN_SNTP_BEFORE_MQTT
#define APP_ALIYUN_SNTP_BEFORE_MQTT  0
#endif
/* 0=关闭 HTTP 百度对时备用；MQTT NTP 仍可用 */
#ifndef APP_ALIYUN_HTTP_DATE_ENABLE
#define APP_ALIYUN_HTTP_DATE_ENABLE  0
#endif
/*
 * Fill with HMAC-SHA1 hex of:
 *   "clientId<CLIENTID>deviceName<DEVICENAME>productKey<PRODUCTKEY>"
 * key = DEVICE_SECRET, uppercase hex output.
 * If empty, cloud connect will not proceed.
 */
#define APP_ALIYUN_MQTT_PASSWORD   "6DE74821026461352E60C38C45380F5F44D1A70F"

/* Feature / boot flags */
#define APP_NFC_ENABLE          1
#ifndef APP_TEMP_DISABLE_BIOMETRIC
#define APP_TEMP_DISABLE_BIOMETRIC 1
#endif
#define APP_BOOT_VTOR_RELOCATE  0
#define APP_USE_FREERTOS        1

/* RS485: USART6 PC6(TX→DI) / PC7(RX←RO) / PC8(DE+RE)。与调试口独立，默认关闭。 */
#define APP_RS485_ENABLE         0
/* 1=调试口 USART6 PC6(TX)/PC7(RX)；0=USART1 PA9/PA10 */
#ifndef APP_DEBUG_ON_USART6
#define APP_DEBUG_ON_USART6      0
#endif
#ifndef APP_DEBUG_UART_BAUD
#define APP_DEBUG_UART_BAUD      115200u
#endif
/* 0=WiFi 页仅显示；1=扫描列表+连接（云端连当前/记忆的 WiFi） */
#ifndef APP_WIFI_UI_SCAN_ENABLE
#define APP_WIFI_UI_SCAN_ENABLE  1
#endif
/* 0=仅手动选热点+输密码连接；1=进 WiFi 页按 Flash 记忆自动连（暂关闭） */
#ifndef APP_WIFI_AUTO_CONNECT_ENABLE
#define APP_WIFI_AUTO_CONNECT_ENABLE  0
#endif
/* 1=仅输出 [WiFi] 扫描/UI 串口日志（会链入 vsnprintf，Flash 紧张请关） */
#ifndef APP_WIFI_UART_DEBUG
#define APP_WIFI_UART_DEBUG      0
#endif
/* 1=CWJAP 关键步骤固定串口日志（不占 printf，可与 APP_WIFI_UART_DEBUG=0 同开） */
#ifndef APP_WIFI_CWJAP_TRACE
#define APP_WIFI_CWJAP_TRACE     0
#endif
/* 1=CloudTask alive / wifi scan loop 心跳 */
#ifndef APP_RTOS_HEARTBEAT_DEBUG
#define APP_RTOS_HEARTBEAT_DEBUG 0
#endif
/* 1=仅 [TP] 触屏串口日志（PA9/PA10，需 APP_DEBUG_ON_USART6=0） */
#ifndef APP_TOUCH_UART_DEBUG
#define APP_TOUCH_UART_DEBUG     0
#endif
/* Host：ILI9341 显示 + FT6336(I2C)。1=裁掉电阻 SPI / GT9 触摸驱动以省 Flash */
#ifndef APP_TP_FT6336_CAP_ONLY
#define APP_TP_FT6336_CAP_ONLY   1
#endif
/* 竖屏时 X 是否镜像：1=width-x（旧板）；0=不镜像（ILI9341+FT6336 常见） */
#ifndef APP_TP_MIRROR_X
#define APP_TP_MIRROR_X           0
#endif
#ifndef APP_W25Q_DEBUG
#define APP_W25Q_DEBUG           0
#endif
#define APP_RS485_ROLE_MASTER    1u
#define APP_RS485_ROLE_SLAVE     2u
/* Build one board as master and the other as slave by changing this macro. */
#define APP_RS485_NODE_ROLE      APP_RS485_ROLE_MASTER
#define APP_RS485_UART_BAUD      115200u
#define APP_RS485_LOCAL_ADDR     ((APP_RS485_NODE_ROLE == APP_RS485_ROLE_MASTER) ? 0x01u : 0x02u)
#define APP_RS485_PEER_ADDR      ((APP_RS485_NODE_ROLE == APP_RS485_ROLE_MASTER) ? 0x02u : 0x01u)
#define APP_RS485_IS_SLAVE       ((APP_RS485_NODE_ROLE) == APP_RS485_ROLE_SLAVE)
#define APP_RS485_IS_MASTER      ((APP_RS485_NODE_ROLE) == APP_RS485_ROLE_MASTER)

/* 1: USART1 打印 [RS485] 排查；0: 关闭 */
#ifndef APP_HOST_RS485_DIAG
#define APP_HOST_RS485_DIAG      0
#endif
/* 1: 从机只采图，特征送主机 Search+用户表判定；0: 从机本地 Search */
#ifndef APP_FP_SLAVE_MATCH_VIA_HOST
#define APP_FP_SLAVE_MATCH_VIA_HOST 1
#endif
#ifndef APP_FP_SLAVE_MATCH_FALLBACK
#define APP_FP_SLAVE_MATCH_FALLBACK 1
#endif
#ifndef APP_FP_SLAVE_MATCH_RS485_MS
#define APP_FP_SLAVE_MATCH_RS485_MS 2000u
#endif

#ifndef APP_FP_MIRROR_DIAG
#define APP_FP_MIRROR_DIAG       0
#endif
/* 1: 首页指纹开锁流程串口日志（USART1 PA9）；其它诊断默认关闭 */
#ifndef APP_FP_UNLOCK_LOG
#define APP_FP_UNLOCK_LOG        0
#endif
#ifndef APP_HOST_NAV_DIAG
#define APP_HOST_NAV_DIAG        0
#endif
/* 0: 只打镜像结果/PING 失败/从机开锁上报与 RS485 错误；1: 增加每帧 dump、PING OK、布线长提示等 */
#ifndef APP_HOST_RS485_LOG_VERBOSE
#define APP_HOST_RS485_LOG_VERBOSE 0
#endif
/* 发完一帧后等待从机切换 DE/RE 再收应答 */
#ifndef APP_RS485_POST_TX_GAP_MS
#define APP_RS485_POST_TX_GAP_MS 5u
#endif
/* PING/DELETE 等小帧：发完后额外等待从机 DE 切换并回 ACK（USER_ADD 另有 20ms） */
#ifndef APP_RS485_SHORT_CMD_SETTLE_MS
#define APP_RS485_SHORT_CMD_SETTLE_MS 15u
#endif
#ifndef APP_FP_MIRROR_RS485_BACKOFF_MS
#define APP_FP_MIRROR_RS485_BACKOFF_MS 1000u
#endif
/* USER_ADD/DELETE/PING 失败时整帧重试次数 */
#ifndef APP_RS485_CMD_RETRY_MAX
#define APP_RS485_CMD_RETRY_MAX 3u
#endif
#ifndef APP_RS485_MASTER_POLL_MS
#define APP_RS485_MASTER_POLL_MS 50u
#endif
#ifndef APP_RS485_MIRROR_INTER_CMD_MS
#define APP_RS485_MIRROR_INTER_CMD_MS 25u
#endif
/* 指纹页镜像：页间间隔。曾成功用 280ms；加速改成 150ms 易与从机 StoreChar 撞车导致 0x18 */
#ifndef APP_FP_MIRROR_PAGE_GAP_MS
#define APP_FP_MIRROR_PAGE_GAP_MS 280u
#endif
#ifndef APP_FP_MIRROR_COMMIT_MIN_OK_MS
#define APP_FP_MIRROR_COMMIT_MIN_OK_MS 1200u
#endif
#ifndef APP_FP_MIRROR_COMMIT_TOUT_MS
#define APP_FP_MIRROR_COMMIT_TOUT_MS 7000u
#endif
#ifndef APP_HOME_POLL_INTERVAL_MS
#define APP_HOME_POLL_INTERVAL_MS 200u
#endif
#ifndef APP_FP_BOOT_GRACE_MS
#define APP_FP_BOOT_GRACE_MS 400u
#endif
#ifndef APP_FP_STA_PROBE_MS
#define APP_FP_STA_PROBE_MS APP_HOME_POLL_INTERVAL_MS
#endif
/* Search 得分低于此值视为误匹配，不开锁（AS608 正常匹配通常 >80） */
#ifndef APP_FP_MATCH_SCORE_MIN
#define APP_FP_MATCH_SCORE_MIN 60u
#endif
#ifndef APP_NFC_UNLOCK_CONFIRM_CNT
#define APP_NFC_UNLOCK_CONFIRM_CNT 2u
#endif
/* STA 连续为高多少次才采图/比对，抑制引脚毛刺误开锁 */
#ifndef APP_FP_FINGER_STA_CONFIRM_CNT
#define APP_FP_FINGER_STA_CONFIRM_CNT 2u
#endif
#ifndef APP_SLAVE_LOG_VERBOSE
#define APP_SLAVE_LOG_VERBOSE 0
#endif
/* 1: 云端/WiFi 诊断打到调试串口 USART6（与 APP_WIFI_UART_DEBUG 配合） */
#ifndef APP_CLOUD_UART_DEBUG
#define APP_CLOUD_UART_DEBUG     0
#endif
/* 1: 云端 MQTT/连接 固定短日志（已稳定，默认关） */
#ifndef APP_CLOUD_TRACE
#define APP_CLOUD_TRACE          1
#endif
/* 1: 时间同步固定短日志（USART 调试口，不占 printf Flash） */
#ifndef APP_TIME_TRACE
#define APP_TIME_TRACE           1
#endif
/* 1: WiFi 已连则 MQTT 长连，不再周期性 CIPCLOSE；离线开锁写 Flash，上线补传 */
#ifndef APP_CLOUD_PERSISTENT_MQTT
#define APP_CLOUD_PERSISTENT_MQTT  1
#endif
/* 1: 用户在 WiFi 页选网连上后走 MQTT 快路径（短 CIPSTART 超时/少 AT） */
#ifndef APP_CLOUD_FAST_AFTER_WIFI_JOIN
#define APP_CLOUD_FAST_AFTER_WIFI_JOIN  1
#endif
/* 1: 关闭 cloud_aliyun_at.c 内 printf（调试时由 APP_CLOUD_UART_DEBUG 强制打开） */
#ifndef APP_HOST_SILENCE_ALIYUN_TERMINAL
#if (APP_CLOUD_UART_DEBUG != 0)
#define APP_HOST_SILENCE_ALIYUN_TERMINAL 0
#else
#define APP_HOST_SILENCE_ALIYUN_TERMINAL 1
#endif
#endif

/* 默认管理员（screen_2 登录）初始字符串 */
#define ADMIN_DEFAULT_ACCOUNT "1"
#define ADMIN_DEFAULT_PASSWORD "1111"

#endif
