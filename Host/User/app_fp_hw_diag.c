#include "app_fp_hw_diag.h"

#include "app_fp_unlock_log.h"
#include "app_config.h"
#include "app_home_fp_poll.h"
#include "../Drivers/BSP/AS608/as608.h"

void app_fp_hw_boot_diag(uint8_t *fp_hw_inited)
{
    uint16_t tpl_n = 0u;

    if(fp_hw_inited == NULL) {
        return;
    }

#if (APP_FP_UNLOCK_LOG == 0)
    return;
#else
#if (APP_DEBUG_ON_USART6 != 0)
    FP_ULOG("[FP] unlock log on USART6 PC6 115200");
#else
    FP_ULOG("[FP] unlock log on USART1 PA9/PA10 115200");
#endif

    HAL_Delay(400);

    if(app_fp_hw_try_handshake_retries(fp_hw_inited, 4u, 150u) != 0u) {
        if(app_fp_chip_tpl_count(&tpl_n, fp_hw_inited) != 0u) {
            FP_ULOG("[FP] boot: module OK, templates in chip=%u", (unsigned)tpl_n);
        } else {
            FP_ULOG("[FP] boot: module OK addr=0x%08X", (unsigned)AS608Addr);
        }
    } else {
        FP_ULOG("[FP] boot: module OFF (check power and USART3 PB10/PB11)");
    }
#endif
}
