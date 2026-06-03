#ifndef APP_SLAVE_DIAG_H
#define APP_SLAVE_DIAG_H

#include "app_config.h"
#include "app_iwdg.h"
#include <stdio.h>
#include "SYSTEM/usart/usart.h"

#ifndef APP_SLAVE_USART1_DEBUG
#define APP_SLAVE_USART1_DEBUG 0
#endif

/* USART1 debug: ASCII-only strings (no Chinese in format strings). */
#if (APP_SLAVE_USART1_DEBUG != 0)
#define SLAVE_DBG_LOG(...) do { \
        char _slv_dbg[224]; \
        int _slv_n = (int)snprintf(_slv_dbg, sizeof(_slv_dbg) - 3, __VA_ARGS__); \
        if(_slv_n < 0) { _slv_n = 0; } \
        if((size_t)_slv_n > sizeof(_slv_dbg) - 3) { _slv_n = (int)(sizeof(_slv_dbg) - 3); } \
        _slv_dbg[_slv_n++] = '\r'; \
        _slv_dbg[_slv_n++] = '\n'; \
        _slv_dbg[_slv_n] = '\0'; \
        app_iwdg_feed(); \
        usart_debug_tx_str(_slv_dbg); \
        app_iwdg_feed(); \
    } while(0)
#else
#define SLAVE_DBG_LOG(...) ((void)0)
#endif

#ifndef APP_SLAVE_UNLOCK_CLOUD_TRACE
#define APP_SLAVE_UNLOCK_CLOUD_TRACE 0
#endif
#if (APP_SLAVE_UNLOCK_CLOUD_TRACE != 0)
#define SLAVE_UNLOCK_CLOUD_LOG(...) do { \
        char _slv_uc[224]; \
        int _slv_uc_n = (int)snprintf(_slv_uc, sizeof(_slv_uc) - 3, __VA_ARGS__); \
        if(_slv_uc_n < 0) { _slv_uc_n = 0; } \
        if((size_t)_slv_uc_n > sizeof(_slv_uc) - 3) { _slv_uc_n = (int)(sizeof(_slv_uc) - 3); } \
        _slv_uc[_slv_uc_n++] = '\r'; \
        _slv_uc[_slv_uc_n++] = '\n'; \
        _slv_uc[_slv_uc_n] = '\0'; \
        app_iwdg_feed(); \
        usart_debug_tx_str(_slv_uc); \
        app_iwdg_feed(); \
    } while(0)
#else
#define SLAVE_UNLOCK_CLOUD_LOG(...) ((void)0)
#endif

#ifndef APP_SLAVE_NFC_UNLOCK_TRACE
#define APP_SLAVE_NFC_UNLOCK_TRACE  0
#endif
#if (APP_SLAVE_NFC_UNLOCK_TRACE != 0)
#define SLAVE_NFC_LOG(...) SLAVE_UNLOCK_CLOUD_LOG("[SLV][NFC] " __VA_ARGS__)
#else
#define SLAVE_NFC_LOG(...) ((void)0)
#endif

#endif /* APP_SLAVE_DIAG_H */
