#include "app_wifi_cfg.h"

#include <string.h>

#include "app_wifi_remember.h"

#define APP_WIFI_SSID_MAX   32u
#define APP_WIFI_PWD_MAX    64u

static char s_wifi_ssid[APP_WIFI_SSID_MAX + 1u];
static char s_wifi_pwd[APP_WIFI_PWD_MAX + 1u];
static uint8_t s_wifi_inited = 0u;
static volatile uint8_t s_wifi_reconnect_req = 0u;

static void wifi_cfg_trim(char *s)
{
    size_t n;
    char *p;

    if(s == NULL || s[0] == '\0') {
        return;
    }
    p = s;
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }
    if(p != s) {
        memmove(s, p, strlen(p) + 1u);
    }
    n = strlen(s);
    while(n > 0u) {
        char c = s[n - 1u];
        if(c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        s[n - 1u] = '\0';
        n--;
    }
}

void app_wifi_cfg_init_defaults(void)
{
    if(s_wifi_inited != 0u) {
        return;
    }
    s_wifi_ssid[0] = '\0';
    s_wifi_pwd[0] = '\0';
    app_wifi_remember_init();
    (void)app_wifi_remember_load_primary(s_wifi_ssid, (uint16_t)sizeof(s_wifi_ssid),
                                         s_wifi_pwd, (uint16_t)sizeof(s_wifi_pwd));
    s_wifi_inited = 1u;
}

const char *app_wifi_cfg_get_ssid(void)
{
    app_wifi_cfg_init_defaults();
    return s_wifi_ssid;
}

const char *app_wifi_cfg_get_password(void)
{
    app_wifi_cfg_init_defaults();
    return s_wifi_pwd;
}

void app_wifi_cfg_set(const char *ssid, const char *password)
{
    app_wifi_cfg_init_defaults();
    if(ssid != NULL) {
        strncpy(s_wifi_ssid, ssid, APP_WIFI_SSID_MAX);
        s_wifi_ssid[APP_WIFI_SSID_MAX] = '\0';
    }
    if(password != NULL) {
        strncpy(s_wifi_pwd, password, APP_WIFI_PWD_MAX);
        s_wifi_pwd[APP_WIFI_PWD_MAX] = '\0';
        wifi_cfg_trim(s_wifi_pwd);
    }
    if(ssid != NULL) {
        wifi_cfg_trim(s_wifi_ssid);
    }
    s_wifi_reconnect_req = 1u;
}

uint8_t app_wifi_cfg_request_reconnect(void)
{
    return s_wifi_reconnect_req;
}

void app_wifi_cfg_clear_reconnect_request(void)
{
    s_wifi_reconnect_req = 0u;
}
