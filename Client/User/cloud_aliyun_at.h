#ifndef CLOUD_ALIYUN_AT_H
#define CLOUD_ALIYUN_AT_H

#include <stdint.h>

void cloud_aliyun_at_init(void);
void cloud_aliyun_at_poll_5ms(void);
uint8_t cloud_aliyun_at_is_online(void);
uint8_t cloud_aliyun_at_publish_property(const char *json_payload);

#endif
