#ifndef APP_FP_MIRROR_DIAG_H
#define APP_FP_MIRROR_DIAG_H

/* USART1 debug: use ASCII-only format strings (avoid GBK/UTF-8 mojibake in terminal). */

#include "app_config.h"
#include <stdio.h>
#include "./SYSTEM/usart/usart.h"
#include "app_iwdg.h"

#ifndef APP_FP_MIRROR_DIAG
#define APP_FP_MIRROR_DIAG 1
#endif

#if (APP_FP_MIRROR_DIAG != 0)
#define FP_MIRROR_LOG(...) do { \
        char _fpl[220]; \
        int _fn = snprintf(_fpl, sizeof(_fpl) - 3, __VA_ARGS__); \
        if(_fn < 0) { _fn = 0; } \
        if((size_t)_fn > sizeof(_fpl) - 3) { _fn = (int)(sizeof(_fpl) - 3); } \
        _fpl[_fn++] = '\r'; \
        _fpl[_fn++] = '\n'; \
        _fpl[_fn] = '\0'; \
        app_iwdg_feed(); \
        usart_debug_tx_str(_fpl); \
        app_iwdg_feed(); \
    } while(0)
#else
#define FP_MIRROR_LOG(...) ((void)0)
#endif

#endif /* APP_FP_MIRROR_DIAG_H */
