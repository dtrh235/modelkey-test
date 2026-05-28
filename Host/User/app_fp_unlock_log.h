#ifndef APP_FP_UNLOCK_LOG_H
#define APP_FP_UNLOCK_LOG_H

/* ��ҳָ�ƿ���ר�ô�����־���� usart_debug��USART6 PC6 �� USART1 PA9�� */

#include "app_config.h"
#include <stdio.h>
#include "./SYSTEM/usart/usart.h"

#ifndef APP_FP_UNLOCK_LOG
#define APP_FP_UNLOCK_LOG 1
#endif

#if (APP_FP_UNLOCK_LOG != 0)
#define FP_ULOG(...) do { \
        char _fpul[240]; \
        int _n = snprintf(_fpul, sizeof(_fpul) - 3, __VA_ARGS__); \
        if(_n < 0) { \
            _n = 0; \
        } \
        if((size_t)_n > sizeof(_fpul) - 3) { \
            _n = (int)(sizeof(_fpul) - 3); \
        } \
        _fpul[_n++] = '\r'; \
        _fpul[_n++] = '\n'; \
        _fpul[_n] = '\0'; \
        usart_debug_tx_str(_fpul); \
    } while(0)
#else
#define FP_ULOG(...) ((void)0)
#endif

#endif /* APP_FP_UNLOCK_LOG_H */
