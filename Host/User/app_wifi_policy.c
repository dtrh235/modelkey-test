#include "app_wifi_policy.h"

#include "app_wifi_remember.h"
#include "app_config.h"
#include "cloud_aliyun_at.h"

void app_wifi_policy_on_scan_done(void)
{
#if (APP_WIFI_AUTO_CONNECT_ENABLE == 1)
    app_wifi_remember_on_scan_done();
#else
    (void)0;
#endif
}

uint8_t app_wifi_policy_may_join_wifi(void)
{
    return 0u;
}

uint8_t app_wifi_policy_may_run_mqtt(void)
{
    return cloud_aliyun_at_wifi_link_ready();
}

uint8_t app_wifi_policy_should_rescan(void)
{
    return 0u;
}
