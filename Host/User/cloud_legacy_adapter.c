#include "cloud_legacy_adapter.h"
#include "cloud_ota_service.h"
#include "cloud_aliyun_at.h"
#include "app_wifi_scan.h"
#include "app_screen.h"
#include "app_config.h"
#include "stm32f4xx_hal.h"

void tcp_mqtt_init(void)
{
    cloud_ota_service_init();
    cloud_aliyun_at_init();
}

void tcp_mqtt_while(void)
{
    if(app_wifi_exclusive_mode() != 0u) {
        return;
    }

    cloud_uart2_pump_rx(20u);

    if(cloud_aliyun_at_poll_allowed(0u) != 0u && cloud_uart2_ui_busy() == 0u) {
        cloud_aliyun_at_poll_5ms();
        cloud_ota_service_poll_5ms();
    }
}

void dht11_data(void)
{
}
