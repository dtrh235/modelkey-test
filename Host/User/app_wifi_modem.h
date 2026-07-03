#ifndef APP_WIFI_MODEM_H
#define APP_WIFI_MODEM_H

/*
 * 硬件固定为 DX-WF24-A (BK7238)，无 ESP8266 分支。
 * 原生 AT+MQTTCONN，不走 AT+CIPSTART/CIPSEND 二进制 MQTT。
 */
#define MODEM_AT_CWMODE_STA       "AT+CWMODE=0\r\n"
#define MODEM_SKIP_ATE0           1
#define MODEM_SKIP_CIP_STACK      1
#define MODEM_USE_NATIVE_MQTT     1

#endif
