#ifndef APP_WIFI_MODEM_H
#define APP_WIFI_MODEM_H

#include "app_config.h"

/* 1=DX-WF24-A (BK7238)；0=ESP8266 AT */
#ifndef APP_WIFI_MODEM_WF24
#define APP_WIFI_MODEM_WF24  1
#endif

#if (APP_WIFI_MODEM_WF24 != 0)
#define MODEM_AT_CWMODE_STA       "AT+CWMODE=0\r\n"
#define MODEM_SKIP_ATE0           1
#define MODEM_SKIP_CIP_STACK      1
#define MODEM_USE_NATIVE_MQTT     1
#else
#define MODEM_AT_CWMODE_STA       "AT+CWMODE=1\r\n"
#define MODEM_SKIP_ATE0           0
#define MODEM_SKIP_CIP_STACK      0
#define MODEM_USE_NATIVE_MQTT     0
#endif

#endif
