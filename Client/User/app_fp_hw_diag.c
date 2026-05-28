#include "app_fp_hw_diag.h"

#include "app_fp_mirror_diag.h"
#include "app_home_fp_poll.h"
#include "app_state.h"
#include "app_config.h"
#include "../Drivers/BSP/AS608/as608.h"
#include "./SYSTEM/usart/usart.h"
#if (APP_RS485_ENABLE == 1)
#include "app_rs485_link.h"
#endif
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
#include "app_slave_fp_sync.h"
#endif

void app_fp_hw_boot_diag(uint8_t *fp_hw_inited)
{
    uint16_t tpl_n = 0u;

#if (APP_FP_MIRROR_DIAG == 0)
    (void)fp_hw_inited;
    (void)tpl_n;
    return;
#else
    if(fp_hw_inited == NULL) {
        return;
    }

    FP_MIRROR_LOG("[FP] diag: USART1 PA9 dbg 115200, USART3 AS608 PB10/PB11 57600 inst=0x%08lX",
                  (unsigned long)(g_uart1_handle.Instance != NULL ? g_uart1_handle.Instance : 0));
#if (APP_RS485_ENABLE == 1)
    FP_MIRROR_LOG("[FP] RS485 USART6 link_ready=%u role=SLAVE local=%02X peer=%02X",
                  (unsigned)app_rs485_link_ready(),
                  (unsigned)APP_RS485_LOCAL_ADDR, (unsigned)APP_RS485_PEER_ADDR);
#endif

    HAL_Delay(400);

    if(app_fp_hw_try_handshake_retries(fp_hw_inited, 4u, 150u) != 0u) {
        (void)app_fp_chip_tpl_count(&tpl_n, fp_hw_inited);
        FP_MIRROR_LOG("[FP] module OK addr=0x%08lX chip_tpl=%u (bound pages via RS485 0x05)",
                      (unsigned long)AS608Addr, (unsigned)tpl_n);
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
        if(app_slave_fp_write_ever_ok() != 0u) {
            FP_MIRROR_LOG("[FP] slave mirror status: template write has occurred");
        }
#endif

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
        {
            uint8_t i;

            if(g_default_admin_has_fp != 0u) {
                FP_MIRROR_LOG("[FP] bind admin %s pages=%u,%u",
                              g_default_admin_account,
                              (unsigned)g_default_admin_fp_page_id_1,
                              (unsigned)g_default_admin_fp_page_id_2);
            }
            for(i = 0u; i < g_user_count; i++) {
                if(!g_users[i].active || !g_users[i].has_fp) {
                    continue;
                }
                FP_MIRROR_LOG("[FP] bind user %s pages=%u,%u",
                              g_users[i].acc,
                              (unsigned)g_users[i].fp_page_id_1,
                              (unsigned)g_users[i].fp_page_id_2);
            }
        }
#endif
    } else {
        FP_MIRROR_LOG("[FP] module OFF rx=%u (check AS608 power/wiring PB10=TX PB11=RX)",
                      (unsigned)as608_rx_widx_get());
    }
#endif
}
