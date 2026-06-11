#include "app_temp_password.h"

#include "stm32f4xx_hal.h"

#include <string.h>

typedef struct {
    char account[APP_TEMP_ACCOUNT_MAX + 1u];
    char password[APP_TEMP_PWD_MAX + 1u];
    uint32_t expires_hal_ms;
    uint8_t used;
} temp_password_slot_t;

static uint8_t temp_password_expired(uint32_t expires_hal_ms)
{
    uint32_t now = HAL_GetTick();
    return ((int32_t)(expires_hal_ms - now) <= 0) ? 1u : 0u;
}

static temp_password_slot_t s_slots[APP_TEMP_PASSWORD_MAX];

void app_temp_password_init(void)
{
    memset(s_slots, 0, sizeof(s_slots));
}

void app_temp_password_tick(void)
{
    uint8_t i;

    for(i = 0u; i < APP_TEMP_PASSWORD_MAX; i++) {
        if(s_slots[i].account[0] == '\0') {
            continue;
        }
        if(s_slots[i].used != 0u || temp_password_expired(s_slots[i].expires_hal_ms) != 0u) {
            memset(&s_slots[i], 0, sizeof(s_slots[i]));
        }
    }
}

static int temp_password_find_account(const char *account)
{
    uint8_t i;

    if(account == NULL || account[0] == '\0') {
        return -1;
    }
    for(i = 0u; i < APP_TEMP_PASSWORD_MAX; i++) {
        if(s_slots[i].account[0] != '\0' && strcmp(s_slots[i].account, account) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int temp_password_find_free(void)
{
    uint8_t i;

    for(i = 0u; i < APP_TEMP_PASSWORD_MAX; i++) {
        if(s_slots[i].account[0] == '\0') {
            return (int)i;
        }
    }
    return -1;
}

uint8_t app_temp_password_add(const char *account, const char *password, uint32_t valid_sec)
{
    int idx;
    size_t acc_len;
    size_t pwd_len;
    uint32_t valid_ms;

    if(account == NULL || password == NULL || account[0] == '\0' || password[0] == '\0') {
        return 0u;
    }
    acc_len = strlen(account);
    pwd_len = strlen(password);
    if(acc_len == 0u || acc_len > APP_TEMP_ACCOUNT_MAX || pwd_len < 4u || pwd_len > APP_TEMP_PWD_MAX) {
        return 0u;
    }
    if(valid_sec < 60u || valid_sec > 3600u) {
        valid_sec = 300u;
    }
    valid_ms = valid_sec * 1000u;

    app_temp_password_tick();

    idx = temp_password_find_account(account);
    if(idx < 0) {
        idx = temp_password_find_free();
    }
    if(idx < 0) {
        return 0u;
    }

    strncpy(s_slots[idx].account, account, APP_TEMP_ACCOUNT_MAX);
    s_slots[idx].account[APP_TEMP_ACCOUNT_MAX] = '\0';
    strncpy(s_slots[idx].password, password, APP_TEMP_PWD_MAX);
    s_slots[idx].password[APP_TEMP_PWD_MAX] = '\0';
    s_slots[idx].expires_hal_ms = HAL_GetTick() + valid_ms;
    s_slots[idx].used = 0u;
    return 1u;
}

uint8_t app_temp_password_revoke(const char *account)
{
    int idx;

    idx = temp_password_find_account(account);
    if(idx < 0) {
        return 0u;
    }
    memset(&s_slots[idx], 0, sizeof(s_slots[idx]));
    return 1u;
}

uint8_t app_temp_password_try_unlock(const char *account, const char *password)
{
    int idx;

    if(account == NULL || password == NULL) {
        return 0u;
    }

    app_temp_password_tick();
    idx = temp_password_find_account(account);
    if(idx < 0) {
        return 0u;
    }
    if(s_slots[idx].used != 0u || temp_password_expired(s_slots[idx].expires_hal_ms) != 0u) {
        memset(&s_slots[idx], 0, sizeof(s_slots[idx]));
        return 0u;
    }
    if(strcmp(password, s_slots[idx].password) != 0) {
        return 0u;
    }
    s_slots[idx].used = 1u;
    memset(&s_slots[idx], 0, sizeof(s_slots[idx]));
    return 1u;
}
