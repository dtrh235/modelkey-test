#include "app_unlock_uart4.h"

#include "app_config.h"
#include "./SYSTEM/sys/sys.h"

static UART_HandleTypeDef s_huart4;
static uint8_t s_uart4_inited = 0u;

void app_unlock_uart4_init(void)
{
    if(s_uart4_inited != 0u) {
        return;
    }

    s_huart4.Instance = UART4;
    s_huart4.Init.BaudRate = APP_RS485_UART_BAUD;
    s_huart4.Init.WordLength = UART_WORDLENGTH_8B;
    s_huart4.Init.StopBits = UART_STOPBITS_1;
    s_huart4.Init.Parity = UART_PARITY_NONE;
    s_huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_huart4.Init.Mode = UART_MODE_TX_RX;

    if(HAL_UART_Init(&s_huart4) == HAL_OK) {
        s_uart4_inited = 1u;
    }
}

void app_unlock_uart4_poll(void)
{
}

void app_unlock_uart4_on_unlock_ok(const char *account, const char *method)
{
    uint8_t unlock_flag = (uint8_t)'1';

    (void)account;
    (void)method;

    if(s_uart4_inited == 0u) {
        app_unlock_uart4_init();
    }
    if(s_uart4_inited == 0u) {
        return;
    }

    (void)HAL_UART_Transmit(&s_huart4, &unlock_flag, 1u, 20u);
}
