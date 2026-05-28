#ifndef CLOUD_LEGACY_ADAPTER_H
#define CLOUD_LEGACY_ADAPTER_H

/* Compatibility API names aligned with folder-1 integration sequence */
void tcp_mqtt_init(void);
void tcp_mqtt_while(void);
void ota_firmware_get(void);
void dht11_data(void);

#endif
