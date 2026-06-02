#include "app_user_ops.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_ccm_ram.h"
#include "app_host_diag.h"
#include "app_ui_nav.h"
#include "app_screen8_flow.h"
#include "app_screen.h"
#include "app_state.h"
#include "../Drivers/BSP/AS608/as608.h"
#include "../Drivers/BSP/W25Q16/bsp_w25q16.h"
#include "app_home_fp_poll.h"
#include "cloud_ota_service.h"
#include "user_auth_portable.h"
#include "stm32f4xx_hal.h"

#if (APP_RS485_ENABLE == 1)
#include "app_rs485_link.h"
#include "app_rs485_proto.h"
#if APP_RS485_IS_MASTER
#include "app_fp_mirror_tx.h"
#endif
#ifndef APP_RS485_MIRROR_CMD_TOUT_MS
/* Mirror payload + slave Flash save can exceed 120ms; host must wait for 0x82 reply. */
#define APP_RS485_MIRROR_CMD_TOUT_MS 800u
#endif
#endif

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
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

#if (APP_HOST_NAV_DIAG != 0)
#define USER_STORE_LOG(fmt, ...) NAV_LOG("[STORE] " fmt "\r\n", ##__VA_ARGS__)
#else
#define USER_STORE_LOG(fmt, ...) ((void)0)
#endif

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
/* Avoid large stack objects in StorageTask (can cause reboot during save). */
static APP_CCM_DATA app_user_store_t s_users_store_work;
static APP_CCM_DATA app_user_store_t s_users_store_verify;
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
    if(!bsp_w25q16_read(USER_STORE_EXT_ADDR, (uint8_t *)&s_users_store_verify, sizeof(s_users_store_verify))) {
        return false;
    }
    return (memcmp(&s_users_store_verify, ref, sizeof(s_users_store_verify)) == 0) ? true : false;
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
        if(!bsp_w25q16_write_page(addr, data + pos, chunk)) {
            return false;
        }
        pos += chunk;
    }
    return true;
}

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
#define FP_TPL_CACHE_MAGIC       (0x46505431u) /* "FPT1" */
#define FP_TPL_CACHE_BASE_ADDR   ((uint32_t)0x00180000u)
#define FP_TPL_CACHE_RECORD_SIZE (640u)
#define FP_TPL_CACHE_SECTOR_SIZE (4096u)
#define FP_TPL_CACHE_PER_SECTOR  (FP_TPL_CACHE_SECTOR_SIZE / FP_TPL_CACHE_RECORD_SIZE)

typedef struct {
    uint32_t magic;
    uint16_t page_id;
    uint16_t tpl_len;
    uint32_t hash;
    uint32_t reserved;
    uint8_t tpl[AS608_TEMPLATE_SIZE];
} fp_tpl_cache_record_t;

static uint8_t s_fp_tpl_cache_sector[FP_TPL_CACHE_SECTOR_SIZE];

static uint32_t fp_tpl_cache_record_addr(uint16_t page_id)
{
    uint32_t sector_idx = (uint32_t)page_id / FP_TPL_CACHE_PER_SECTOR;
    uint32_t slot_idx = (uint32_t)page_id % FP_TPL_CACHE_PER_SECTOR;

    return FP_TPL_CACHE_BASE_ADDR +
           (sector_idx * FP_TPL_CACHE_SECTOR_SIZE) +
           (slot_idx * FP_TPL_CACHE_RECORD_SIZE);
}

bool users_fp_template_cache_load(uint16_t page_id, uint8_t *tpl, uint16_t tpl_len)
{
    fp_tpl_cache_record_t rec;

    if(page_id >= 300u || tpl == NULL || tpl_len < AS608_TEMPLATE_SIZE) {
        return false;
    }
    if(!bsp_w25q16_init()) {
        return false;
    }
    if(!bsp_w25q16_read(fp_tpl_cache_record_addr(page_id), (uint8_t *)&rec, sizeof(rec))) {
        bsp_w25q16_end_session();
        return false;
    }
    bsp_w25q16_end_session();

    if(rec.magic != FP_TPL_CACHE_MAGIC ||
       rec.page_id != page_id ||
       rec.tpl_len != AS608_TEMPLATE_SIZE ||
       rec.hash != users_store_hash_blob(rec.tpl, AS608_TEMPLATE_SIZE)) {
        return false;
    }
    memcpy(tpl, rec.tpl, AS608_TEMPLATE_SIZE);
    return true;
}

bool users_fp_template_cache_valid(uint16_t page_id)
{
    fp_tpl_cache_record_t rec;

    if(page_id >= 300u) {
        return false;
    }
    if(!bsp_w25q16_init()) {
        return false;
    }
    if(!bsp_w25q16_read(fp_tpl_cache_record_addr(page_id), (uint8_t *)&rec, sizeof(rec))) {
        bsp_w25q16_end_session();
        return false;
    }
    bsp_w25q16_end_session();

    if(rec.magic != FP_TPL_CACHE_MAGIC ||
       rec.page_id != page_id ||
       rec.tpl_len != AS608_TEMPLATE_SIZE ||
       rec.hash != users_store_hash_blob(rec.tpl, AS608_TEMPLATE_SIZE)) {
        return false;
    }
    return true;
}

static bool users_fp_template_cache_write_record(uint16_t page_id, const fp_tpl_cache_record_t *rec)
{
    uint32_t rec_addr;
    uint32_t sector_addr;
    uint32_t off;
    bool ok = false;

    if(page_id >= 300u || rec == NULL) {
        return false;
    }
    rec_addr = fp_tpl_cache_record_addr(page_id);
    sector_addr = rec_addr & ~(FP_TPL_CACHE_SECTOR_SIZE - 1u);
    off = rec_addr - sector_addr;

    if(!bsp_w25q16_init()) {
        return false;
    }
    if(!bsp_w25q16_read(sector_addr, s_fp_tpl_cache_sector, sizeof(s_fp_tpl_cache_sector))) {
        goto finish;
    }
    memcpy(s_fp_tpl_cache_sector + off, rec, sizeof(*rec));
    if(!bsp_w25q16_erase_sector_4k(sector_addr)) {
        goto finish;
    }
    ok = users_store_write_blob(sector_addr, s_fp_tpl_cache_sector, sizeof(s_fp_tpl_cache_sector));

finish:
    bsp_w25q16_end_session();
    return ok;
}

bool users_fp_template_cache_save(uint16_t page_id, const uint8_t *tpl, uint16_t tpl_len)
{
    fp_tpl_cache_record_t rec;

    if(page_id >= 300u || tpl == NULL || tpl_len != AS608_TEMPLATE_SIZE) {
        return false;
    }
    memset(&rec, 0xFF, sizeof(rec));
    rec.magic = FP_TPL_CACHE_MAGIC;
    rec.page_id = page_id;
    rec.tpl_len = AS608_TEMPLATE_SIZE;
    rec.hash = users_store_hash_blob(tpl, AS608_TEMPLATE_SIZE);
    rec.reserved = 0u;
    memcpy(rec.tpl, tpl, AS608_TEMPLATE_SIZE);
    return users_fp_template_cache_write_record(page_id, &rec);
}

void users_fp_template_cache_delete(uint16_t page_id)
{
    fp_tpl_cache_record_t rec;

    if(page_id >= 300u) {
        return;
    }
    memset(&rec, 0xFF, sizeof(rec));
    (void)users_fp_template_cache_write_record(page_id, &rec);
}

#define USERS_MIRROR_JOB_NONE    0u
#define USERS_MIRROR_JOB_FULL    1u
#define USERS_MIRROR_JOB_ONE_ACC 2u
#define USERS_MIRROR_JOB_DELETE  3u

static volatile uint8_t s_users_mirror_job = USERS_MIRROR_JOB_NONE;
static char s_users_mirror_acc[13];

static void users_host_pack_default_admin_cred(user_cred_t *u)
{
    if(u == NULL) {
        return;
    }
    memset(u, 0, sizeof(*u));
    u->active = 1u;
    u->is_admin = g_default_admin_is_admin_role ? 1u : 0u;
    strncpy(u->acc, g_default_admin_account, sizeof(u->acc) - 1u);
    strncpy(u->pwd, g_default_admin_password, sizeof(u->pwd) - 1u);
    u->has_nfc = g_default_admin_has_nfc;
    memcpy(u->nfc_uid, g_default_admin_nfc_uid, sizeof(u->nfc_uid));
    u->has_fp = g_default_admin_has_fp;
    u->fp_page_id_1 = g_default_admin_fp_page_id_1;
    u->fp_page_id_2 = g_default_admin_fp_page_id_2;
}

/*
 * 从机开锁/录入与主机一致：所有 active 用户（含仅账号密码）均通过 0x02 下发。
 */
static void users_storage_mirror_overflow_to_slave(void)
{
    uint8_t i;

    if(app_rs485_link_ready() == 0u) {
        HOST_RS485_LOG("[RS485] mirror skip: link not ready\r\n");
        return;
    }
    app_rs485_master_poll_incoming(APP_RS485_MASTER_POLL_MS);
    HOST_RS485_LOG("[RS485][USER] full sync start cnt=%u\r\n", (unsigned)g_user_count);
    for(i = 0u; i < g_user_count; i++) {
        if(!g_users[i].active) {
            continue;
        }
        if(app_rs485_proto_slave_user_add(&g_users[i], APP_RS485_MIRROR_CMD_TOUT_MS)) {
            HOST_RS485_LOG("[RS485][USER] full OK slot=%u acc=%.12s nfc=%u fp_flag=%u\r\n",
                           (unsigned)i, g_users[i].acc, (unsigned)g_users[i].has_nfc,
                           (unsigned)g_users[i].has_fp);
            /* has_fp 仅表示用户录过指纹；实际是否同步看 AS608/缓存里该 page 是否有模板 */
            if(g_users[i].has_fp != 0u) {
                app_fp_mirror_tx_schedule_page(g_users[i].fp_page_id_1);
                if(g_users[i].fp_page_id_2 != g_users[i].fp_page_id_1) {
                    app_fp_mirror_tx_schedule_page(g_users[i].fp_page_id_2);
                }
            }
        } else {
            HOST_RS485_LOG("[RS485][USER] full FAIL slot=%u acc=%.12s\r\n",
                           (unsigned)i, g_users[i].acc);
        }
#if (APP_USE_FREERTOS == 1)
        vTaskDelay(pdMS_TO_TICKS(APP_RS485_MIRROR_INTER_CMD_MS));
#endif
        app_rs485_master_poll_incoming(12u);
    }
    if(!g_default_admin_deleted) {
        user_cred_t uadm;
        users_host_pack_default_admin_cred(&uadm);
        if(app_rs485_proto_slave_user_add(&uadm, APP_RS485_MIRROR_CMD_TOUT_MS)) {
            HOST_RS485_LOG("[RS485][USER] full OK default_admin acc=%.12s\r\n", uadm.acc);
            if(uadm.has_fp != 0u) {
                app_fp_mirror_tx_schedule_page(uadm.fp_page_id_1);
                if(uadm.fp_page_id_2 != uadm.fp_page_id_1) {
                    app_fp_mirror_tx_schedule_page(uadm.fp_page_id_2);
                }
            }
        } else {
            HOST_RS485_LOG("[RS485][USER] full FAIL default_admin acc=%.12s\r\n", uadm.acc);
        }
    }
}

void users_mirror_schedule_full(void)
{
    s_users_mirror_job = USERS_MIRROR_JOB_FULL;
}

void users_mirror_schedule_acc(const char *acc)
{
    if(s_users_mirror_job == USERS_MIRROR_JOB_FULL) {
        return;
    }
    if(acc != NULL) {
        strncpy(s_users_mirror_acc, acc, sizeof(s_users_mirror_acc) - 1u);
        s_users_mirror_acc[sizeof(s_users_mirror_acc) - 1u] = '\0';
    } else {
        s_users_mirror_acc[0] = '\0';
    }
    s_users_mirror_job = USERS_MIRROR_JOB_ONE_ACC;
}

void users_mirror_schedule_delete(const char *acc)
{
    if(acc != NULL) {
        strncpy(s_users_mirror_acc, acc, sizeof(s_users_mirror_acc) - 1u);
        s_users_mirror_acc[sizeof(s_users_mirror_acc) - 1u] = '\0';
    } else {
        s_users_mirror_acc[0] = '\0';
    }
    s_users_mirror_job = USERS_MIRROR_JOB_DELETE;
}

static bool users_mirror_one_acc_to_slave(const char *acc)
{
    int idx;
    user_cred_t uadm;

    if(acc == NULL || acc[0] == '\0' || app_rs485_link_ready() == 0u) {
        return false;
    }

    idx = users_find_index_by_acc(acc);
    if(idx >= 0) {
        if(!g_users[idx].active) {
            return false;
        }
        return app_rs485_proto_slave_user_add(&g_users[idx], APP_RS485_MIRROR_CMD_TOUT_MS);
    }

    if(!g_default_admin_deleted && strcmp(acc, g_default_admin_account) == 0) {
        users_host_pack_default_admin_cred(&uadm);
        return app_rs485_proto_slave_user_add(&uadm, APP_RS485_MIRROR_CMD_TOUT_MS);
    }
    return false;
}

void users_mirror_run_pending(void)
{
    uint8_t job;
    char acc[13];

    job = s_users_mirror_job;
    if(job == USERS_MIRROR_JOB_NONE) {
        return;
    }

    if(app_ui_nav_is_busy() != 0u) {
        return;
    }

    if(app_rs485_link_ready() == 0u) {
        return;
    }
    s_users_mirror_job = USERS_MIRROR_JOB_NONE;

    if(job == USERS_MIRROR_JOB_FULL) {
#if APP_RS485_IS_MASTER
        if(app_fp_mirror_tx_busy() != 0u) {
            s_users_mirror_job = USERS_MIRROR_JOB_FULL;
            return;
        }
#endif
        users_storage_mirror_overflow_to_slave();
        return;
    }

    strncpy(acc, s_users_mirror_acc, sizeof(acc) - 1u);
    acc[sizeof(acc) - 1u] = '\0';

    if(job == USERS_MIRROR_JOB_ONE_ACC) {
        bool ok = users_mirror_one_acc_to_slave(acc);
        HOST_RS485_LOG("[RS485][USER] push acc=%.12s ok=%u\r\n", acc, (unsigned)(ok ? 1u : 0u));
        if(ok) {
            /* 用户下发与指纹模板同步分开：传输进行中也要排队新录入的指纹 */
            app_fp_mirror_tx_schedule_acc(acc);
        }
    } else if(job == USERS_MIRROR_JOB_DELETE) {
        bool ok = app_rs485_proto_slave_user_delete(acc, APP_RS485_MIRROR_CMD_TOUT_MS);
        HOST_RS485_LOG("[RS485][USER] del acc=%.12s ok=%u\r\n", acc, (unsigned)(ok ? 1u : 0u));
    }
}

void users_mirror_to_slave_on_link(void)
{
    users_storage_mirror_overflow_to_slave();
}
#endif

static bool users_storage_save_blocking(void)
{
    bool ok = false;
    uint8_t attempt;

    if(sizeof(s_users_store_work) > 4096u) {
        goto finish;
    }
    if(!bsp_w25q16_init()) {
        USER_STORE_LOG("flash init fail");
        goto finish;
    }
    users_store_fill(&s_users_store_work);

    for(attempt = 0u; attempt < 3u && !ok; attempt++) {
        USER_STORE_LOG("save attempt=%u cnt=%u", (unsigned)(attempt + 1u), g_user_count);
        if(!bsp_w25q16_erase_sector_4k(USER_STORE_EXT_ADDR)) {
            USER_STORE_LOG("save erase fail");
            HAL_Delay(8);
            continue;
        }
        if(!users_store_write_blob(USER_STORE_EXT_ADDR, (const uint8_t *)&s_users_store_work, sizeof(s_users_store_work))) {
            USER_STORE_LOG("save write fail");
            HAL_Delay(8);
            continue;
        }
        ok = users_storage_verify_blob_matches(&s_users_store_work);
        if(!ok) {
            USER_STORE_LOG("save verify mismatch");
            HAL_Delay(8);
        } else {
            USER_STORE_LOG("save ok");
        }
    }
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
    USER_STORE_LOG("sched pending cnt=%u", (unsigned)g_user_count);
    if(s_users_storage_task != NULL) {
        xTaskNotifyGive(s_users_storage_task);
    }
}

static uint8_t users_storage_should_defer_flush(void)
{
    /* 与 3 Lvgl Porting 相同：在 StorageTask 写 W25Q；仅弹窗/切屏瞬间推迟 */
    if(screen8_add_user_success_popup_active() != 0u) {
        return 1u;
    }
    if(app_ui_nav_is_busy() != 0u) {
        return 1u;
    }
    return 0u;
}

static void users_storage_flush_pending(void)
{
    if(s_users_storage_save_pending == 0u) {
        return;
    }
    if(users_storage_should_defer_flush() != 0u) {
        if(s_users_storage_task != NULL) {
            xTaskNotifyGive(s_users_storage_task);
        }
        return;
    }

    while(s_users_storage_save_pending != 0u) {
        s_users_storage_save_pending = 0u;
        if(users_storage_save_blocking()) {
            USER_STORE_LOG("flush ok cnt=%u", (unsigned)g_user_count);
            continue;
        }
        s_users_storage_save_pending = 1u;
        USER_STORE_LOG("flush fail cnt=%u (retry later)", (unsigned)g_user_count);
        if(s_users_storage_task != NULL) {
            xTaskNotifyGive(s_users_storage_task);
        }
        break;
    }
}

void users_storage_kick_flush(void)
{
    if(s_users_storage_save_pending != 0u && s_users_storage_task != NULL) {
        xTaskNotifyGive(s_users_storage_task);
    }
}

uint8_t users_storage_try_commit_scr3_from_gui(void)
{
    users_storage_kick_flush();
    return (s_users_storage_save_pending == 0u) ? 1u : 0u;
}

void users_storage_scr3_idle_service(void)
{
    users_storage_kick_flush();
}
#else
void users_storage_kick_flush(void)
{
}

uint8_t users_storage_try_commit_scr3_from_gui(void)
{
    return users_storage_save_blocking() ? 1u : 0u;
}

void users_storage_scr3_idle_service(void)
{
}
#endif

bool users_storage_load(void)
{
    app_user_store_t st;
    bool ok = false;

    if(sizeof(st) > 4096u) {
        goto finish;
    }
    if(!bsp_w25q16_init()) {
        USER_STORE_LOG("flash init fail (see [W25Q] lines above)");
        goto finish;
    }
    USER_STORE_LOG("flash init ok jedec=0x%06lX", (unsigned long)bsp_w25q16_read_jedec_id());
    if(!bsp_w25q16_read(USER_STORE_EXT_ADDR, (uint8_t *)&st, sizeof(st))) {
        USER_STORE_LOG("load read fail");
        goto finish;
    }
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
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));
    }
    users_storage_flush_pending();
#else
    (void)wait_ms;
#endif
}

uint8_t fp_delete_template_by_page(uint16_t page_id)
{
    uint8_t en;
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
    users_fp_template_cache_delete(page_id);
#endif
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
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
    users_mirror_schedule_acc(acc);
#endif
    return true;
}

uint8_t users_fp_page_in_use(uint16_t page_id)
{
    uint8_t i;

    if(page_id >= 300u) {
        return 0u;
    }
    if(!g_default_admin_deleted && g_default_admin_has_fp) {
        if(g_default_admin_fp_page_id_1 == page_id || g_default_admin_fp_page_id_2 == page_id) {
            return 1u;
        }
    }
    for(i = 0u; i < g_user_count; i++) {
        if(!g_users[i].active || !g_users[i].has_fp) {
            continue;
        }
        if(g_users[i].fp_page_id_1 == page_id || g_users[i].fp_page_id_2 == page_id) {
            return 1u;
        }
    }
    return 0u;
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

uint8_t users_fp_alloc_enroll_pages(uint16_t *page1_out, uint16_t *page2_out)
{
    uint16_t p;
    uint16_t first = 0xFFFFu;
    uint16_t second = 0xFFFFu;

    if(page1_out == NULL || page2_out == NULL) {
        return 0u;
    }

    for(p = 0u; p < 300u; p++) {
        if(users_fp_page_in_use(p) != 0u) {
            continue;
        }
        if(first == 0xFFFFu) {
            first = p;
            continue;
        }
        second = p;
        break;
    }

    if(first == 0xFFFFu || second == 0xFFFFu) {
        return 0u;
    }

    *page1_out = first;
    *page2_out = second;
    return 1u;
}

bool users_bind_fp_by_acc_ex(const char *acc, uint16_t page_id, uint8_t schedule_mirror)
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
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
    if(schedule_mirror != 0u) {
        users_mirror_schedule_acc(acc);
        app_fp_mirror_tx_schedule_acc(acc);
    }
#endif
    return true;
}

bool users_bind_fp_by_acc(const char *acc, uint16_t page_id)
{
    return users_bind_fp_by_acc_ex(acc, page_id, 1u);
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
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
    if(g_user_count >= APP_USER_HOST_PRIMARY_SLOTS) {
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
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
    users_mirror_schedule_acc(acc);
#endif
    return true;
}

uint8_t users_admin_count(void)
{
    uint8_t i;
    uint8_t n = 0u;

    if(!g_default_admin_deleted && g_default_admin_is_admin_role) {
        n++;
    }
    for(i = 0u; i < g_user_count; i++) {
        if(g_users[i].active && g_users[i].is_admin) {
            n++;
        }
    }
    return n;
}

uint8_t users_has_loginable_admin(void)
{
    uint8_t i;

    if(g_default_admin_deleted == 0u &&
       g_default_admin_account[0] != '\0' &&
       g_default_admin_is_admin_role != 0u) {
        return 1u;
    }
    for(i = 0u; i < g_user_count; i++) {
        if(g_users[i].active != 0u && g_users[i].is_admin != 0u) {
            return 1u;
        }
    }
    return 0u;
}

void users_ensure_default_admin_if_none(void)
{
    int idx;

    if(users_has_loginable_admin() != 0u) {
        return;
    }

    idx = users_find_index_by_acc(ADMIN_DEFAULT_ACCOUNT);
    if(idx >= 0) {
        g_users[idx].active = 1u;
        g_users[idx].is_admin = 1u;
        if(g_users[idx].pwd[0] == '\0') {
            strncpy(g_users[idx].pwd, ADMIN_DEFAULT_PASSWORD, sizeof(g_users[idx].pwd) - 1u);
            g_users[idx].pwd[sizeof(g_users[idx].pwd) - 1u] = '\0';
        }
        (void)users_storage_save();
        return;
    }

    g_default_admin_deleted = 1u;
    g_default_admin_is_admin_role = 0u;
    g_default_admin_account[0] = '\0';
    g_default_admin_password[0] = '\0';
    (void)users_try_register(ADMIN_DEFAULT_ACCOUNT, ADMIN_DEFAULT_PASSWORD, true);
}

uint8_t users_migrate_default_admin_to_users(void)
{
    int idx;

    if(g_default_admin_deleted != 0u) {
        return 0u;
    }
    if(g_default_admin_account[0] == '\0') {
        g_default_admin_deleted = 1u;
        g_default_admin_is_admin_role = 0u;
        return 1u;
    }

    idx = users_find_index_by_acc(g_default_admin_account);
    if(idx < 0) {
        if(g_user_count >= APP_USER_MAX) {
            return 0u;
        }
        idx = (int)g_user_count;
        memset(&g_users[idx], 0, sizeof(g_users[idx]));
        strncpy(g_users[idx].acc, g_default_admin_account, sizeof(g_users[idx].acc) - 1u);
        g_users[idx].acc[sizeof(g_users[idx].acc) - 1u] = '\0';
        strncpy(g_users[idx].pwd, g_default_admin_password, sizeof(g_users[idx].pwd) - 1u);
        g_users[idx].pwd[sizeof(g_users[idx].pwd) - 1u] = '\0';
        g_users[idx].active = 1u;
        g_user_count++;
    }

    g_users[idx].is_admin = 1u;
    g_users[idx].has_nfc = g_default_admin_has_nfc;
    memcpy(g_users[idx].nfc_uid, g_default_admin_nfc_uid, sizeof(g_users[idx].nfc_uid));
    g_users[idx].has_fp = g_default_admin_has_fp;
    g_users[idx].fp_page_id_1 = g_default_admin_fp_page_id_1;
    g_users[idx].fp_page_id_2 = g_default_admin_fp_page_id_2;

    g_default_admin_deleted = 1u;
    g_default_admin_is_admin_role = 0u;
    g_default_admin_has_nfc = 0u;
    memset(g_default_admin_nfc_uid, 0, sizeof(g_default_admin_nfc_uid));
    g_default_admin_has_fp = 0u;
    g_default_admin_fp_page_id_1 = 0xFFFFu;
    g_default_admin_fp_page_id_2 = 0xFFFFu;
    g_default_admin_account[0] = '\0';
    g_default_admin_password[0] = '\0';
    return 1u;
}

bool users_try_delete_by_acc(const char *acc)
{
    int idx;

    if(acc == NULL || acc[0] == '\0') {
        cloud_ota_service_report_event(CLOUD_EVT_USER_DELETE_FAIL, acc);
        return false;
    }

    /* 旧 Flash：默认管理员仅存虚拟槽，未迁入 users[] */
    if(!g_default_admin_deleted && strcmp(acc, g_default_admin_account) == 0) {
        if(g_default_admin_is_admin_role != 0u && users_admin_count() <= 1u) {
            cloud_ota_service_report_event(CLOUD_EVT_USER_DELETE_FAIL, acc);
            return false;
        }
        users_clear_fp_by_acc(acc, 1u);
        users_clear_nfc_by_acc(acc);
        g_default_admin_deleted = 1u;
        g_default_admin_is_admin_role = 0u;
        g_default_admin_has_nfc = 0u;
        memset(g_default_admin_nfc_uid, 0, sizeof(g_default_admin_nfc_uid));
        g_default_admin_has_fp = 0u;
        g_default_admin_fp_page_id_1 = 0xFFFFu;
        g_default_admin_fp_page_id_2 = 0xFFFFu;
        g_default_admin_account[0] = '\0';
        g_default_admin_password[0] = '\0';
        idx = users_find_index_by_acc(acc);
        if(idx >= 0) {
            g_users[idx].active = 0u;
        }
        (void)users_storage_save();
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
        users_mirror_schedule_delete(acc);
#endif
        cloud_ota_service_report_event(CLOUD_EVT_USER_DELETE_OK, acc);
        return true;
    }

    idx = users_find_index_by_acc(acc);
    if(idx < 0) {
        cloud_ota_service_report_event(CLOUD_EVT_USER_DELETE_FAIL, acc);
        return false;
    }
    if(g_users[idx].is_admin != 0u && users_admin_count() <= 1u) {
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
    users_mirror_schedule_delete(acc);
#endif
    cloud_ota_service_report_event(CLOUD_EVT_USER_DELETE_OK, acc);
    return true;
}

bool default_admin_deleted(void)
{
    return g_default_admin_deleted ? true : false;
}

bool admin_credentials_match_with_delete(const char *acc, const char *pwd)
{
    int idx;

    if(acc == NULL || pwd == NULL || acc[0] == '\0') {
        return false;
    }

    idx = users_find_index_by_acc(acc);
    if(idx >= 0 && g_users[idx].is_admin != 0u &&
       strcmp(pwd, g_users[idx].pwd) == 0) {
        return true;
    }

    if(!g_default_admin_deleted &&
       g_default_admin_account[0] != '\0' &&
       g_default_admin_is_admin_role != 0u &&
       strcmp(acc, g_default_admin_account) == 0 &&
       strcmp(pwd, g_default_admin_password) == 0) {
        return true;
    }

    return false;
}

bool unlock_credentials_match_with_delete(const char *acc, const char *pwd)
{
    int idx;

    if(acc == NULL || pwd == NULL || acc[0] == '\0') {
        return false;
    }

    idx = users_find_index_by_acc(acc);
    if(idx >= 0 && strcmp(pwd, g_users[idx].pwd) == 0) {
        return true;
    }

    if(!g_default_admin_deleted &&
       g_default_admin_account[0] != '\0' &&
       strcmp(acc, g_default_admin_account) == 0 &&
       strcmp(pwd, g_default_admin_password) == 0) {
        return true;
    }

    return false;
}
