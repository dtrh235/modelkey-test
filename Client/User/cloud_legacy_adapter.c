#include "cloud_legacy_adapter.h"
#include "cloud_ota_service.h"
#include "cloud_aliyun_at.h"

void tcp_mqtt_init(void)
{
    cloud_ota_service_init();
    cloud_aliyun_at_init();
}

void tcp_mqtt_while(void)
{
    cloud_ota_service_poll_5ms();
    cloud_aliyun_at_poll_5ms();
}

void ota_firmware_get(void)
{
    /* Reserved for periodic active OTA query in next migration step */
}

void dht11_data(void)
{
    /* Reserved for cloud property reporting compatibility */
}
