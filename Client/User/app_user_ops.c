#include "app_user_ops.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "../Drivers/BSP/AS608/as608.h"
#include "../Drivers/BSP/W25Q16/bsp_w25q16.h"
#include "app_home_fp_poll.h"
#include "cloud_ota_service.h"
#include "user_auth_portable.h"
#include "app_iwdg.h"
#include "stm32f4xx_hal.h"

#if (APP_RS485_ENABLE == 1)
#include "app_rs485_link.h"
#include "app_rs485_proto.h"
#if APP_RS485_IS_SLAVE
#include "app_slave_diag.h"
#endif
#endif

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
static uint8_t s_users_boot_save_pending;
#endif

extern user_cred_t g_users[APP_USER_MAX];
extern uint8_t g_user_count;
extern uint8_t g_default_admin_deleted;
extern uint8_t g_default_admin_is_admin_role;
extern char g_default_admin_account[13];
extern char g_default_admin_password[11];
extern uint8_t g_default_admin_has_nfc;
extern uint8_t g_default_admin_nfc_uid[4];
extern uint8_t g_default_admin_has_fp;
extern uint16_t g_default_admin_fp_page_id_1;
extern uint16_t g_default_admin_fp_page_id_2;
extern uint8_t g_fp_hw_inited;

#define USER_STORE_MAGIC   (0x55535231u) /* "USR1" */
#define USER_STORE_VERSION (2u) /* v2: users[64]；v1 仅 32 槽，上电自动迁移 */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t user_count;
    uint8_t default_admin_deleted;
    uint8_t default_admin_is_admin_role;
    uint8_t default_admin_has_nfc;
    uint8_t default_admin_nfc_uid[4];
    uint8_t default_admin_has_fp;
    uint8_t reserved0;
    uint16_t default_admin_fp_page_id_1;
    uint16_t default_admin_fp_page_id_2;
    char default_admin_account[13];
    char default_admin_password[11];
    user_cred_t users[32];
    uint32_t tail_marker;
} app_user_store_v1_t;
/* W25Q16 棣栨墖鍖?4KB锛屼粎瀛樹竴浠界敤鎴疯〃锛堥伩鍏嶆摝鍐欑墖鍐?Flash 鐮村潖 LVGL 璧勬簮锛?*/
#define USER_STORE_EXT_ADDR ((uint32_t)0x00000000u)

#define USER_STORE_LOG(fmt, ...) ((void)0)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t user_count;
    uint8_t default_admin_deleted;
    uint8_t default_admin_is_admin_role;
    uint8_t default_admin_has_nfc;
    uint8_t default_admin_nfc_uid[4];
    uint8_t default_admin_has_fp;
    uint8_t reserved0;
    uint16_t default_admin_fp_page_id_1;
    uint16_t default_admin_fp_page_id_2;
    char default_admin_account[13];
    char default_admin_password[11];
    user_cred_t users[APP_USER_MAX];
    uint32_t tail_marker;
} app_user_store_t;

#if (APP_USE_FREERTOS == 1)
static TaskHandle_t s_users_storage_task = NULL;
static volatile uint8_t s_users_storage_save_pending = 0u;
#endif

static void users_store_fill(app_user_store_t *s)
{
    memset(s, 0, sizeof(*s));
    s->magic = USER_STORE_MAGIC;
    s->version = USER_STORE_VERSION;
    s->user_count = g_user_count;
    s->default_admin_deleted = g_default_admin_deleted;
    s->default_admin_is_admin_role = g_default_admin_is_admin_role;
    s->default_admin_has_nfc = g_default_admin_has_nfc;
    memcpy(s->default_admin_nfc_uid, g_default_admin_nfc_uid, sizeof(s->default_admin_nfc_uid));
    s->default_admin_has_fp = g_default_admin_has_fp;
    s->default_admin_fp_page_id_1 = g_default_admin_fp_page_id_1;
    s->default_admin_fp_page_id_2 = g_default_admin_fp_page_id_2;
    strncpy(s->default_admin_account, g_default_admin_account, sizeof(s->default_admin_account) - 1u);
    strncpy(s->default_admin_password, g_default_admin_password, sizeof(s->default_admin_password) - 1u);
    memcpy(s->users, g_users, sizeof(s->users));
    s->tail_marker = (USER_STORE_MAGIC ^ 0xA5A55A5Au);
}

static uint8_t users_store_valid_v1(const app_user_store_v1_t *s)
{
    if(s->magic != USER_STORE_MAGIC) return 0u;
    if(s->version != 1u) return 0u;
    if(s->user_count > 32u) return 0u;
    if(s->tail_marker != (USER_STORE_MAGIC ^ 0xA5A55A5Au)) return 0u;
    return 1u;
}

static uint8_t users_store_valid(const app_user_store_t *s)
{
    if(s->magic != USER_STORE_MAGIC) return 0u;
    if(s->version != USER_STORE_VERSION) return 0u;
    if(s->user_count > APP_USER_MAX) return 0u;
    if(s->tail_marker != (USER_STORE_MAGIC ^ 0xA5A55A5Au)) return 0u;
    return 1u;
}

static void users_store_apply_v1(const app_user_store_v1_t *s)
{
    uint8_t i;

    g_user_count = s->user_count;
    g_default_admin_deleted = s->default_admin_deleted;
    g_default_admin_is_admin_role = s->default_admin_is_admin_role;
    g_default_admin_has_nfc = s->default_admin_has_nfc;
    memcpy(g_default_admin_nfc_uid, s->default_admin_nfc_uid, sizeof(g_default_admin_nfc_uid));
    g_default_admin_has_fp = s->default_admin_has_fp;
    g_default_admin_fp_page_id_1 = s->default_admin_fp_page_id_1;
    g_default_admin_fp_page_id_2 = s->default_admin_fp_page_id_2;
    strncpy(g_default_admin_account, s->default_admin_account, sizeof(g_default_admin_account) - 1u);
    g_default_admin_account[sizeof(g_default_admin_account) - 1u] = '\0';
    strncpy(g_default_admin_password, s->default_admin_password, sizeof(g_default_admin_password) - 1u);
    g_default_admin_password[sizeof(g_default_admin_password) - 1u] = '\0';
    memcpy(g_users, s->users, sizeof(s->users));
    for(i = g_user_count; i < APP_USER_MAX; i++) {
        memset(&g_users[i], 0, sizeof(g_users[i]));
    }
}

static void users_store_apply(const app_user_store_t *s)
{
    uint8_t i;

    g_user_count = s->user_count;
    g_default_admin_deleted = s->default_admin_deleted;
    g_default_admin_is_admin_role = s->default_admin_is_admin_role;
    g_default_admin_has_nfc = s->default_admin_has_nfc;
    memcpy(g_default_admin_nfc_uid, s->default_admin_nfc_uid, sizeof(g_default_admin_nfc_uid));
    g_default_admin_has_fp = s->default_admin_has_fp;
    g_default_admin_fp_page_id_1 = s->default_admin_fp_page_id_1;
    g_default_admin_fp_page_id_2 = s->default_admin_fp_page_id_2;
    strncpy(g_default_admin_account, s->default_admin_account, sizeof(g_default_admin_account) - 1u);
    g_default_admin_account[sizeof(g_default_admin_account) - 1u] = '\0';
    strncpy(g_default_admin_password, s->default_admin_password, sizeof(g_default_admin_password) - 1u);
    g_default_admin_password[sizeof(g_default_admin_password) - 1u] = '\0';
    memcpy(g_users, s->users, sizeof(s->users));
    for(i = g_user_count; i < APP_USER_MAX; i++) {
        memset(&g_users[i], 0, sizeof(g_users[i]));
    }
}

static uint32_t users_store_hash_blob(const uint8_t *data, size_t len)
{
    uint32_t hash = 2166136261u;
    size_t i;

    for(i = 0u; i < len; i++) {
        hash ^= (uint32_t)data[i];
        hash *= 16777619u;
    }
    return hash;
}

size_t users_storage_blob_size(void)
{
    return sizeof(app_user_store_t);
}

bool users_storage_export_blob(uint8_t *out, size_t out_sz, size_t *out_len, uint32_t *out_hash)
{
    app_user_store_t st;

    if(out == NULL || out_len == NULL) return false;
    if(out_sz < sizeof(st)) return false;

    users_store_fill(&st);
    memcpy(out, &st, sizeof(st));
    *out_len = sizeof(st);
    if(out_hash != NULL) {
        *out_hash = users_store_hash_blob((const uint8_t *)&st, sizeof(st));
    }
    return true;
}

bool users_storage_import_blob(const uint8_t *blob, size_t blob_len, uint32_t *out_hash)
{
    const app_user_store_t *st = (const app_user_store_t *)blob;

    if(blob == NULL) return false;
    if(blob_len != sizeof(app_user_store_t)) return false;
    if(!users_store_valid(st)) return false;

    users_store_apply(st);
    if(!users_storage_save()) return false;

    if(out_hash != NULL) {
        *out_hash = users_store_hash_blob(blob, blob_len);
    }
    return true;
}

uint32_t users_storage_snapshot_hash(void)
{
    app_user_store_t st;

    users_store_fill(&st);
    return users_store_hash_blob((const uint8_t *)&st, sizeof(st));
}

static bool users_storage_verify_blob_matches(const app_user_store_t *ref)
{
    app_user_store_t rd;

    if(!bsp_w25q16_read(USER_STORE_EXT_ADDR, (uint8_t *)&rd, sizeof(rd))) {
        return false;
    }
    return (memcmp(&rd, ref, sizeof(rd)) == 0) ? true : false;
}

static bool users_store_write_blob(uint32_t base, const uint8_t *data, size_t len)
{
    size_t pos = 0u;

    while(pos < len) {
        uint32_t addr = base + (uint32_t)pos;
        uint32_t page_off = addr & 0xFFu;
        size_t chunk = (size_t)(256u - page_off);
        if(chunk > (len - pos)) {
            chunk = len - pos;
        }
#if APP_RS485_IS_SLAVE
        app_iwdg_feed();
#endif
        if(!bsp_w25q16_write_page(addr, data + pos, chunk)) {
            return false;
        }
        pos += chunk;
    }
    return true;
}

#if (APP_RS485_ENABLE == 1)
static void users_storage_mirror_overflow_to_slave(void)
{
    uint8_t i;

    if(app_rs485_link_ready() == 0u) {
        return;
    }
    if(g_user_count <= APP_USER_HOST_PRIMARY_SLOTS) {
        return;
    }
    for(i = APP_USER_HOST_PRIMARY_SLOTS; i < g_user_count; i++) {
        if(!g_users[i].active) {
            continue;
        }
        (void)app_rs485_proto_slave_user_add(&g_users[i], 120u);
    }
}
#endif

static bool users_storage_save_blocking(void)
{
    app_user_store_t st;
    bool ok = false;
    uint8_t attempt;

    if(sizeof(st) > 4096u) {
        goto finish;
    }
    if(!bsp_w25q16_init()) {
        USER_STORE_LOG("flash init fail");
        goto finish;
    }
    users_store_fill(&st);

    for(attempt = 0u; attempt < 3u && !ok; attempt++) {
        USER_STORE_LOG("save attempt=%u cnt=%u", (unsigned)(attempt + 1u), g_user_count);
#if APP_RS485_IS_SLAVE
        app_iwdg_feed();
#endif
        if(!bsp_w25q16_erase_sector_4k(USER_STORE_EXT_ADDR)) {
            USER_STORE_LOG("save erase fail");
            HAL_Delay(8);
            continue;
        }
#if APP_RS485_IS_SLAVE
        app_iwdg_feed();
#endif
        if(!users_store_write_blob(USER_STORE_EXT_ADDR, (const uint8_t *)&st, sizeof(st))) {
            USER_STORE_LOG("save write fail");
            HAL_Delay(8);
            continue;
        }
#if APP_RS485_IS_SLAVE
        app_iwdg_feed();
#endif
        ok = users_storage_verify_blob_matches(&st);
        if(!ok) {
            USER_STORE_LOG("save verify mismatch");
            HAL_Delay(8);
        } else {
            USER_STORE_LOG("save ok");
        }
    }
#if (APP_RS485_ENABLE == 1)
    if(ok && APP_RS485_IS_MASTER) {
        users_storage_mirror_overflow_to_slave();
    }
#endif
finish:
    bsp_w25q16_end_session();
    return ok;
}

#if (APP_USE_FREERTOS == 1)
static uint8_t users_storage_scheduler_running(void)
{
    /* During vTaskSuspendAll (e.g. RS485 proto), state is SUSPENDED not RUNNING; blocking save then
     * stalls the bus and can trip IWDG. Defer save whenever scheduler has started. */
    return (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) ? 1u : 0u;
}

static uint8_t users_storage_in_interrupt(void)
{
    return ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0u) ? 1u : 0u;
}

static void users_storage_schedule_save(void)
{
    s_users_storage_save_pending = 1u;
    if(s_users_storage_task != NULL) {
        xTaskNotifyGive(s_users_storage_task);
    }
}

static void users_storage_flush_pending(void)
{
    if(s_users_storage_save_pending == 0u) {
        return;
    }

    do {
        app_iwdg_feed();
        s_users_storage_save_pending = 0u;
        (void)users_storage_save_blocking();
    } while(s_users_storage_save_pending != 0u);
}

void users_storage_mark_boot_save_pending(void)
{
    s_users_boot_save_pending = 1u;
}

void users_storage_boot_commit_pending(void)
{
    if(s_users_boot_save_pending == 0u) {
        return;
    }
    s_users_boot_save_pending = 0u;
    (void)users_storage_save();
}
#endif

bool users_storage_load(void)
{
    app_user_store_t st;
    bool ok = false;

    if(sizeof(st) > 4096u) {
        goto finish;
    }
    app_iwdg_feed();
    if(!bsp_w25q16_init()) {
        USER_STORE_LOG("flash init fail");
        goto finish;
    }
    app_iwdg_feed();
    if(!bsp_w25q16_read(USER_STORE_EXT_ADDR, (uint8_t *)&st, sizeof(st))) {
        USER_STORE_LOG("load read fail");
        goto finish;
    }
    app_iwdg_feed();
    if(users_store_valid(&st)) {
        users_store_apply(&st);
        USER_STORE_LOG("load ok cnt=%u admin_nfc=%u admin_fp=%u admin_uid=%02X%02X%02X%02X admin_fp_p=%u/%u admin_del=%u",
                       g_user_count, g_default_admin_has_nfc, g_default_admin_has_fp,
                       g_default_admin_nfc_uid[0], g_default_admin_nfc_uid[1],
                       g_default_admin_nfc_uid[2], g_default_admin_nfc_uid[3],
                       (unsigned)g_default_admin_fp_page_id_1,
                       (unsigned)g_default_admin_fp_page_id_2,
                       g_default_admin_deleted);
        {
            uint8_t i;
            for(i = 0u; i < g_user_count; i++) {
                USER_STORE_LOG("  u[%u] act=%u acc='%s' has_nfc=%u uid=%02X%02X%02X%02X has_fp=%u fp_p=%u/%u",
                               i, g_users[i].active, g_users[i].acc, g_users[i].has_nfc,
                               g_users[i].nfc_uid[0], g_users[i].nfc_uid[1],
                               g_users[i].nfc_uid[2], g_users[i].nfc_uid[3],
                               g_users[i].has_fp,
                               (unsigned)g_users[i].fp_page_id_1,
                               (unsigned)g_users[i].fp_page_id_2);
            }
        }
        ok = true;
        goto finish;
    }
    {
        app_user_store_v1_t st1;

        memcpy(&st1, &st, sizeof(st1));
        if(users_store_valid_v1(&st1)) {
            bsp_w25q16_end_session();
            users_store_apply_v1(&st1);
            (void)users_storage_save_blocking();
            ok = true;
            return ok;
        }
    }
    USER_STORE_LOG("load invalid magic=%08lX ver=%lu cnt=%u tail=%08lX", (unsigned long)st.magic, (unsigned long)st.version, st.user_count, (unsigned long)st.tail_marker);
finish:
    bsp_w25q16_end_session();
    return ok;
}

bool users_storage_save(void)
{
#if (APP_USE_FREERTOS == 1)
    if(users_storage_scheduler_running() != 0u &&
       users_storage_in_interrupt() == 0u &&
       xTaskGetCurrentTaskHandle() != s_users_storage_task) {
        users_storage_schedule_save();
        return true;
    }
#endif
    return users_storage_save_blocking();
}

void users_storage_request_save(void)
{
#if (APP_USE_FREERTOS == 1)
    if(users_storage_scheduler_running() != 0u && users_storage_in_interrupt() == 0u) {
        users_storage_schedule_save();
        return;
    }
#endif
    (void)users_storage_save_blocking();
}

void users_storage_task_register_current(void)
{
#if (APP_USE_FREERTOS == 1)
    s_users_storage_task = xTaskGetCurrentTaskHandle();
#endif
}

void users_storage_task_wait_and_flush(uint32_t wait_ms)
{
#if (APP_USE_FREERTOS == 1)
    if(s_users_storage_task == NULL) {
        users_storage_task_register_current();
    }

    if(s_users_storage_save_pending == 0u) {
        TickType_t wait_end = xTaskGetTickCount() + pdMS_TO_TICKS(wait_ms);
        while(xTaskGetTickCount() < wait_end) {
#if APP_RS485_IS_SLAVE
            app_iwdg_feed();
#endif
            if(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20u)) != 0u) {
                break;
            }
        }
    }
    users_storage_flush_pending();
#else
    (void)wait_ms;
#endif
}

uint8_t fp_delete_template_by_page(uint16_t page_id)
{
    uint8_t en;
    if(!app_fp_hw_try_handshake(&g_fp_hw_inited)) return 0u;
    en = GZ_DeletChar(page_id, 1u);
    return (en == 0x00u) ? 1u : 0u;
}

static void app_user_pack_admin(user_auth_admin_t *admin)
{
    memset(admin, 0, sizeof(*admin));
    admin->deleted = g_default_admin_deleted;
    admin->is_admin_role = g_default_admin_is_admin_role;
    strncpy(admin->account, g_default_admin_account, sizeof(admin->account) - 1u);
    strncpy(admin->password, g_default_admin_password, sizeof(admin->password) - 1u);
    admin->has_nfc = g_default_admin_has_nfc;
    memcpy(admin->nfc_uid, g_default_admin_nfc_uid, sizeof(admin->nfc_uid));
    admin->has_fp = g_default_admin_has_fp;
    admin->fp_page_id_1 = g_default_admin_fp_page_id_1;
    admin->fp_page_id_2 = g_default_admin_fp_page_id_2;
}

int users_find_index_by_acc(const char *acc)
{
    return user_auth_find_index_by_acc(g_users, g_user_count, acc);
}

int users_find_index_by_nfc_uid(const uint8_t uid[4])
{
    return user_auth_find_index_by_nfc_uid(g_users, g_user_count, uid);
}

bool users_bind_nfc_by_acc(const char *acc, const uint8_t uid[4])
{
    user_auth_admin_t admin;
    int idx_snap;
    user_cred_t usr_snap;
    uint8_t have_usr_snap = 0u;
    uint8_t sav_has_nfc;
    uint8_t sav_uid[4];

    idx_snap = users_find_index_by_acc(acc);
    if(idx_snap >= 0) {
        memcpy(&usr_snap, &g_users[idx_snap], sizeof(usr_snap));
        have_usr_snap = 1u;
    }
    sav_has_nfc = g_default_admin_has_nfc;
    memcpy(sav_uid, g_default_admin_nfc_uid, sizeof(sav_uid));

    app_user_pack_admin(&admin);
    if(!user_auth_bind_nfc_by_acc(g_users, g_user_count, &admin, acc, uid)) return false;

    g_default_admin_has_nfc = admin.has_nfc;
    memcpy(g_default_admin_nfc_uid, admin.nfc_uid, sizeof(g_default_admin_nfc_uid));

    if(!users_storage_save()) {
        g_default_admin_has_nfc = sav_has_nfc;
        memcpy(g_default_admin_nfc_uid, sav_uid, sizeof(g_default_admin_nfc_uid));
        if(have_usr_snap != 0u && idx_snap >= 0) {
            memcpy(&g_users[idx_snap], &usr_snap, sizeof(usr_snap));
        }
        return false;
    }
    return true;
}

bool users_fp_page_owned_by_other(const char *acc, uint16_t page_id)
{
    uint8_t i;
    if(!g_default_admin_deleted && g_default_admin_has_fp &&
       strcmp(acc, g_default_admin_account) != 0 &&
       (g_default_admin_fp_page_id_1 == page_id || g_default_admin_fp_page_id_2 == page_id)) {
        return true;
    }
    for(i = 0u; i < g_user_count; i++) {
        if(!g_users[i].active || !g_users[i].has_fp) continue;
        if(strcmp(g_users[i].acc, acc) == 0) continue;
        if(g_users[i].fp_page_id_1 == page_id || g_users[i].fp_page_id_2 == page_id) return true;
    }
    return false;
}

bool users_bind_fp_by_acc(const char *acc, uint16_t page_id)
{
    user_auth_admin_t admin;
    int idx_snap;
    user_cred_t usr_snap;
    uint8_t have_usr_snap = 0u;
    uint8_t sav_has_fp;
    uint16_t sav_p1;
    uint16_t sav_p2;

    idx_snap = users_find_index_by_acc(acc);
    if(idx_snap >= 0) {
        memcpy(&usr_snap, &g_users[idx_snap], sizeof(usr_snap));
        have_usr_snap = 1u;
    }
    sav_has_fp = g_default_admin_has_fp;
    sav_p1 = g_default_admin_fp_page_id_1;
    sav_p2 = g_default_admin_fp_page_id_2;

    app_user_pack_admin(&admin);
    if(!user_auth_bind_fp_by_acc(g_users, g_user_count, &admin, acc, page_id)) return false;

    g_default_admin_has_fp = admin.has_fp;
    g_default_admin_fp_page_id_1 = admin.fp_page_id_1;
    g_default_admin_fp_page_id_2 = admin.fp_page_id_2;

    if(!users_storage_save()) {
        g_default_admin_has_fp = sav_has_fp;
        g_default_admin_fp_page_id_1 = sav_p1;
        g_default_admin_fp_page_id_2 = sav_p2;
        if(have_usr_snap != 0u && idx_snap >= 0) {
            memcpy(&g_users[idx_snap], &usr_snap, sizeof(usr_snap));
        }
        return false;
    }
    return true;
}

uint8_t users_has_fp_by_acc(const char *acc, uint16_t *page_id_out)
{
    user_auth_admin_t admin;
    app_user_pack_admin(&admin);
    return user_auth_has_fp_by_acc(g_users, g_user_count, &admin, acc, page_id_out);
}

uint8_t users_match_fp_page(uint16_t page_id)
{
    return user_auth_match_fp_page(g_users, g_user_count,
                                   page_id,
                                   g_default_admin_deleted,
                                   g_default_admin_has_fp,
                                   g_default_admin_fp_page_id_1,
                                   g_default_admin_fp_page_id_2);
}

const char *users_get_acc_by_fp_page(uint16_t page_id)
{
    return user_auth_get_acc_by_fp_page(g_users, g_user_count,
                                        page_id,
                                        g_default_admin_deleted,
                                        g_default_admin_has_fp,
                                        g_default_admin_fp_page_id_1,
                                        g_default_admin_fp_page_id_2,
                                        g_default_admin_account);
}

void users_clear_fp_by_acc(const char *acc, uint8_t clear_hw)
{
    user_auth_admin_t admin;
    int idx_snap;
    user_cred_t usr_snap;
    uint8_t have_usr_snap = 0u;
    uint8_t sav_has_fp;
    uint16_t sav_p1;
    uint16_t sav_p2;

    idx_snap = users_find_index_by_acc(acc);
    if(idx_snap >= 0) {
        memcpy(&usr_snap, &g_users[idx_snap], sizeof(usr_snap));
        have_usr_snap = 1u;
    }
    sav_has_fp = g_default_admin_has_fp;
    sav_p1 = g_default_admin_fp_page_id_1;
    sav_p2 = g_default_admin_fp_page_id_2;

    app_user_pack_admin(&admin);
    user_auth_clear_fp_by_acc(g_users, g_user_count, &admin, acc, clear_hw, fp_delete_template_by_page);
    g_default_admin_has_fp = admin.has_fp;
    g_default_admin_fp_page_id_1 = admin.fp_page_id_1;
    g_default_admin_fp_page_id_2 = admin.fp_page_id_2;

    if(!users_storage_save()) {
        g_default_admin_has_fp = sav_has_fp;
        g_default_admin_fp_page_id_1 = sav_p1;
        g_default_admin_fp_page_id_2 = sav_p2;
        if(have_usr_snap != 0u && idx_snap >= 0) {
            memcpy(&g_users[idx_snap], &usr_snap, sizeof(usr_snap));
        }
    }
}

void users_clear_nfc_by_acc(const char *acc)
{
    user_auth_admin_t admin;
    int idx_snap;
    user_cred_t usr_snap;
    uint8_t have_usr_snap = 0u;
    uint8_t sav_has_nfc;
    uint8_t sav_uid[4];

    idx_snap = users_find_index_by_acc(acc);
    if(idx_snap >= 0) {
        memcpy(&usr_snap, &g_users[idx_snap], sizeof(usr_snap));
        have_usr_snap = 1u;
    }
    sav_has_nfc = g_default_admin_has_nfc;
    memcpy(sav_uid, g_default_admin_nfc_uid, sizeof(sav_uid));

    app_user_pack_admin(&admin);
    user_auth_clear_nfc_by_acc(g_users, g_user_count, &admin, acc);
    g_default_admin_has_nfc = admin.has_nfc;
    memcpy(g_default_admin_nfc_uid, admin.nfc_uid, sizeof(g_default_admin_nfc_uid));

    if(!users_storage_save()) {
        g_default_admin_has_nfc = sav_has_nfc;
        memcpy(g_default_admin_nfc_uid, sav_uid, sizeof(g_default_admin_nfc_uid));
        if(have_usr_snap != 0u && idx_snap >= 0) {
            memcpy(&g_users[idx_snap], &usr_snap, sizeof(usr_snap));
        }
    }
}

bool admin_credentials_match(const char *acc, const char *pwd)
{
    return user_auth_admin_credentials_match(g_users, g_user_count,
                                             g_default_admin_account,
                                             g_default_admin_password,
                                             g_default_admin_is_admin_role,
                                             acc, pwd);
}

bool unlock_credentials_match(const char *acc, const char *pwd)
{
    return user_auth_unlock_credentials_match(g_users, g_user_count,
                                              g_default_admin_account,
                                              g_default_admin_password,
                                              acc, pwd);
}

bool users_try_register(const char *acc, const char *pwd, bool is_admin)
{
    if(g_user_count >= APP_USER_MAX) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_ADD_FAIL, acc);
        return false;
    }
    if(users_find_index_by_acc(acc) >= 0) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_ADD_FAIL, acc);
        return false;
    }
    if(!g_default_admin_deleted && strcmp(acc, g_default_admin_account) == 0) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_ADD_FAIL, acc);
        return false;
    }
#if (APP_RS485_ENABLE == 1)
    if(APP_RS485_IS_MASTER && g_user_count >= APP_USER_HOST_PRIMARY_SLOTS) {
        if(app_rs485_link_ready() == 0u || !app_rs485_proto_ping_peer(120u)) {
            cloud_ota_service_report_event(CLOUD_EVT_USER_ADD_FAIL, acc);
            return false;
        }
    }
#endif
    strncpy(g_users[g_user_count].acc, acc, sizeof(g_users[0].acc) - 1u);
    g_users[g_user_count].acc[sizeof(g_users[0].acc) - 1u] = '\0';
    strncpy(g_users[g_user_count].pwd, pwd, sizeof(g_users[0].pwd) - 1u);
    g_users[g_user_count].pwd[sizeof(g_users[0].pwd) - 1u] = '\0';
    g_users[g_user_count].is_admin = is_admin ? 1u : 0u;
    g_users[g_user_count].active = 1u;
    g_users[g_user_count].has_nfc = 0u;
    memset(g_users[g_user_count].nfc_uid, 0, sizeof(g_users[g_user_count].nfc_uid));
    g_users[g_user_count].has_fp = 0u;
    g_users[g_user_count].fp_page_id_1 = 0xFFFFu;
    g_users[g_user_count].fp_page_id_2 = 0xFFFFu;
    g_user_count++;
    if(!users_storage_save()) {
        g_user_count--;
        memset(&g_users[g_user_count], 0, sizeof(g_users[0]));
        cloud_ota_service_report_event(CLOUD_EVT_USER_ADD_FAIL, acc);
        return false;
    }
    cloud_ota_service_report_event(CLOUD_EVT_USER_ADD_OK, acc);
    return true;
}

bool users_try_delete_by_acc(const char *acc)
{
    int idx = users_find_index_by_acc(acc);
    if(idx < 0) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_DELETE_FAIL, acc);
        return false;
    }
    g_users[idx].active = 0u;
    g_users[idx].has_nfc = 0u;
    memset(g_users[idx].nfc_uid, 0, sizeof(g_users[idx].nfc_uid));
    if(g_users[idx].has_fp) {
        if(g_users[idx].fp_page_id_1 != 0xFFFFu) {
            (void)fp_delete_template_by_page(g_users[idx].fp_page_id_1);
        }
        if(g_users[idx].fp_page_id_2 != 0xFFFFu &&
           g_users[idx].fp_page_id_2 != g_users[idx].fp_page_id_1) {
            (void)fp_delete_template_by_page(g_users[idx].fp_page_id_2);
        }
    }
    g_users[idx].has_fp = 0u;
    g_users[idx].fp_page_id_1 = 0xFFFFu;
    g_users[idx].fp_page_id_2 = 0xFFFFu;
    (void)users_storage_save();
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
    (void)app_rs485_proto_slave_user_delete(acc, 120u);
#endif
    cloud_ota_service_report_event(CLOUD_EVT_USER_DELETE_OK, acc);
    return true;
}

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
#ifndef APP_SLAVE_MIRROR_SAVE_DEBOUNCE_MS
/* 镜像只改 RAM；延后写 Flash，避免上电镜像后立刻擦写触发 IWDG=1、刷卡还要等 Flash */
#define APP_SLAVE_MIRROR_SAVE_DEBOUNCE_MS 5000u
#endif

static uint32_t s_slave_mirror_save_deadline_ms;

static void users_slave_mirror_defer_save(void)
{
    s_slave_mirror_save_deadline_ms = HAL_GetTick() + APP_SLAVE_MIRROR_SAVE_DEBOUNCE_MS;
}

void users_slave_mirror_save_tick(void)
{
    if(s_slave_mirror_save_deadline_ms == 0u) {
        return;
    }
    if((int32_t)(HAL_GetTick() - s_slave_mirror_save_deadline_ms) < 0) {
        return;
    }
    s_slave_mirror_save_deadline_ms = 0u;
    users_storage_request_save();
}

static void users_slave_mirror_sync_default_admin(const user_cred_t *u)
{
    if(u == NULL || u->acc[0] == '\0') {
        return;
    }
    if(strcmp(u->acc, g_default_admin_account) != 0) {
        return;
    }
    g_default_admin_deleted = (u->active != 0u) ? 0u : 1u;
    g_default_admin_is_admin_role = u->is_admin ? 1u : 0u;
    strncpy(g_default_admin_password, u->pwd, sizeof(g_default_admin_password) - 1u);
    g_default_admin_password[sizeof(g_default_admin_password) - 1u] = '\0';
    g_default_admin_has_nfc = u->has_nfc;
    memcpy(g_default_admin_nfc_uid, u->nfc_uid, sizeof(g_default_admin_nfc_uid));
    g_default_admin_has_fp = u->has_fp;
    g_default_admin_fp_page_id_1 = u->fp_page_id_1;
    g_default_admin_fp_page_id_2 = u->fp_page_id_2;
}

static bool users_slave_mirror_delete_default_admin(const char *acc)
{
    int idx;

    if(acc == NULL || strcmp(acc, g_default_admin_account) != 0) {
        return false;
    }
    g_default_admin_deleted = 1u;
    g_default_admin_has_nfc = 0u;
    memset(g_default_admin_nfc_uid, 0, sizeof(g_default_admin_nfc_uid));
    if(g_default_admin_has_fp) {
        if(g_default_admin_fp_page_id_1 != 0xFFFFu) {
            (void)fp_delete_template_by_page(g_default_admin_fp_page_id_1);
        }
        if(g_default_admin_fp_page_id_2 != 0xFFFFu &&
           g_default_admin_fp_page_id_2 != g_default_admin_fp_page_id_1) {
            (void)fp_delete_template_by_page(g_default_admin_fp_page_id_2);
        }
    }
    g_default_admin_has_fp = 0u;
    g_default_admin_fp_page_id_1 = 0xFFFFu;
    g_default_admin_fp_page_id_2 = 0xFFFFu;
    users_slave_mirror_defer_save();
    idx = users_find_index_by_acc(acc);
    if(idx >= 0) {
        g_users[idx].active = 0u;
        g_users[idx].has_nfc = 0u;
        memset(g_users[idx].nfc_uid, 0, sizeof(g_users[idx].nfc_uid));
        g_users[idx].has_fp = 0u;
        g_users[idx].fp_page_id_1 = 0xFFFFu;
        g_users[idx].fp_page_id_2 = 0xFFFFu;
    }
    return true;
}

bool users_slave_mirror_apply_user(const user_cred_t *u)
{
    int idx;

    if(u == NULL) {
        return false;
    }
    users_slave_mirror_sync_default_admin(u);
    idx = users_find_index_by_acc(u->acc);
    if(idx >= 0) {
        g_users[idx] = *u;
    } else {
        if(g_user_count >= APP_USER_MAX) {
            SLAVE_DBG_LOG("[SLV][USER] add FAIL full acc=%.12s cnt=%u", u->acc, (unsigned)g_user_count);
            return false;
        }
        memcpy(&g_users[g_user_count], u, sizeof(*u));
        g_user_count++;
    }
    users_slave_mirror_defer_save();
    idx = users_find_index_by_acc(u->acc);
    SLAVE_DBG_LOG("[SLV][USER] add acc=%.12s idx=%d nfc=%u fp=%u admin=%u",
                  u->acc, idx, (unsigned)u->has_nfc, (unsigned)u->has_fp, (unsigned)u->is_admin);
    return true;
}

bool users_slave_mirror_delete_by_acc(const char *acc)
{
    int idx;
    bool ok;

    if(acc != NULL && strcmp(acc, g_default_admin_account) == 0) {
        ok = users_slave_mirror_delete_default_admin(acc);
        SLAVE_DBG_LOG("[SLV][USER] del default_admin ok=%u", (unsigned)(ok ? 1u : 0u));
        return ok;
    }

    idx = users_find_index_by_acc(acc);
    if(idx < 0) {
        SLAVE_DBG_LOG("[SLV][USER] del miss acc=%s", acc != NULL ? acc : "");
        return false;
    }
    g_users[idx].active = 0u;
    g_users[idx].has_nfc = 0u;
    memset(g_users[idx].nfc_uid, 0, sizeof(g_users[idx].nfc_uid));
    if(g_users[idx].has_fp) {
        if(g_users[idx].fp_page_id_1 != 0xFFFFu) {
            (void)fp_delete_template_by_page(g_users[idx].fp_page_id_1);
        }
        if(g_users[idx].fp_page_id_2 != 0xFFFFu &&
           g_users[idx].fp_page_id_2 != g_users[idx].fp_page_id_1) {
            (void)fp_delete_template_by_page(g_users[idx].fp_page_id_2);
        }
    }
    g_users[idx].has_fp = 0u;
    g_users[idx].fp_page_id_1 = 0xFFFFu;
    g_users[idx].fp_page_id_2 = 0xFFFFu;
    users_slave_mirror_defer_save();
    ok = true;
    SLAVE_DBG_LOG("[SLV][USER] del acc=%s idx=%d", acc, idx);
    return ok;
}
#endif

bool default_admin_deleted(void)
{
    return g_default_admin_deleted ? true : false;
}

bool admin_credentials_match_with_delete(const char *acc, const char *pwd)
{
    if(strcmp(acc, g_default_admin_account) == 0 && default_admin_deleted()) {
        return false;
    }
    return admin_credentials_match(acc, pwd);
}

bool unlock_credentials_match_with_delete(const char *acc, const char *pwd)
{
    if(strcmp(acc, g_default_admin_account) == 0 && default_admin_deleted()) {
        return false;
    }
    return unlock_credentials_match(acc, pwd);
}
