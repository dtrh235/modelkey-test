#ifndef APP_UI_V3_KEYS_H
#define APP_UI_V3_KEYS_H

#include "app_config.h"

#if (APP_UI_V3_ENABLE == 1)

#include "./BSP/KEY/key.h"

uint8_t app_ui_v3_key_dispatch(KeyValue_t key);

#else

static inline uint8_t app_ui_v3_key_dispatch(KeyValue_t key)
{
    (void)key;
    return 0u;
}

#endif

#endif
