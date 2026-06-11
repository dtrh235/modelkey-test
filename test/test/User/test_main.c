/**
 * @file test_main.c
 * @brief 焊板一次性自检：W25Q + NFC + 触屏 + 开锁账号诊断，RS485 打印
 *
 * 接线与 Host 主工程一致：
 *   RS485: PC6=TX, PC7=RX, PC8=DE, 115200
 *   W25Q16: PA15=CS, PB3=SCK, PB4=MISO, PB5=MOSI
 *   MFRC522: PB12=SDA, PB13=SCK, PB14=MISO, PB15=MOSI, PD1=RST
 *   FT6336: PB6=SCL, PB7=SDA, PB1=INT, PE8=RST
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include <string.h>
#include "stm32f4xx_hal.h"

#include "app_config.h"
#include "app_nfc_hw.h"
#include "door.h"
#include "MFRC522.h"
#include "../../../Host/Drivers/BSP/W25Q16/bsp_w25q16.h"
#include "../../../Host/User/app_ccm_ram.h"
#include "../../../Host/User/app_rs485_link.h"
#include "test_rs485_log.h"
#include "test_touch_diag.h"
#include "test_unlock_diag.h"

#define TEST_W25Q_EXPECT_JEDEC   0xEF4015u
#define TEST_W25Q_SCRATCH_ADDR   0x00100000u
#define TEST_DETECT_SLICE_MS     120u
#define TEST_HEARTBEAT_MS        3000u
#define TEST_PROMPT_MS           2000u

static void test_print_banner(void)
{
    test_rs485_log_str("\r\n");
    test_rs485_log_str("========================================\r\n");
    test_rs485_log_str("[HW_TEST] modelkey board self-test\r\n");
    test_rs485_log_str("[HW_TEST] W25Q + NFC + TOUCH + AUTH diag\r\n");
    test_rs485_log_str("[HW_TEST] RS485 USART6 PC6/PC7/PC8 115200\r\n");
    test_rs485_log_str("========================================\r\n");
    test_rs485_logf("[HW_TEST] enables W25Q=%u NFC=%u TOUCH=%u AUTH=%u\r\n",
                    (unsigned)TEST_ENABLE_W25Q_HW,
                    (unsigned)TEST_ENABLE_NFC_HW,
                    (unsigned)TEST_ENABLE_TOUCH_HW,
                    (unsigned)TEST_ENABLE_UNLOCK_SW);
}

#if (TEST_ENABLE_W25Q_HW != 0)

static void test_w25q_run(void)
{
    uint32_t jedec;
    uint8_t buf_w[16];
    uint8_t buf_r[16];
    uint8_t i;
    uint8_t ok = 1u;

    jedec = bsp_w25q16_probe_jedec_id();
    test_rs485_logf("[W25Q] JEDEC=0x%06lX (expect 0x%06lX)\r\n",
                    (unsigned long)jedec, (unsigned long)TEST_W25Q_EXPECT_JEDEC);
    if(jedec != TEST_W25Q_EXPECT_JEDEC) {
        test_rs485_log_str("[W25Q] FAIL chip ID / wiring (PA15=CS PB3/4/5)\r\n");
        return;
    }
    test_rs485_log_str("[W25Q] JEDEC OK\r\n");

    if(!bsp_w25q16_init()) {
        test_rs485_log_str("[W25Q] FAIL init\r\n");
        return;
    }

    if(!bsp_w25q16_erase_sector_4k(TEST_W25Q_SCRATCH_ADDR)) {
        test_rs485_log_str("[W25Q] FAIL erase @0x00100000\r\n");
        bsp_w25q16_end_session();
        return;
    }

    for(i = 0u; i < sizeof(buf_w); i++) {
        buf_w[i] = (uint8_t)(0xA0u + i);
    }
    if(!bsp_w25q16_write_page(TEST_W25Q_SCRATCH_ADDR, buf_w, sizeof(buf_w))) {
        test_rs485_log_str("[W25Q] FAIL write\r\n");
        bsp_w25q16_end_session();
        return;
    }
    memset(buf_r, 0, sizeof(buf_r));
    if(!bsp_w25q16_read(TEST_W25Q_SCRATCH_ADDR, buf_r, sizeof(buf_r))) {
        test_rs485_log_str("[W25Q] FAIL read back\r\n");
        bsp_w25q16_end_session();
        return;
    }
    for(i = 0u; i < sizeof(buf_w); i++) {
        if(buf_r[i] != buf_w[i]) {
            ok = 0u;
            break;
        }
    }
    bsp_w25q16_end_session();

    if(ok != 0u) {
        test_rs485_log_str("[W25Q] PASS erase/write/read\r\n");
    } else {
        test_rs485_logf("[W25Q] FAIL mismatch byte %u\r\n", (unsigned)i);
    }

    if(bsp_w25q16_init()) {
        uint32_t magic = 0u;
        if(bsp_w25q16_read(0u, (uint8_t *)&magic, 4u)) {
            test_rs485_logf("[W25Q] @0 magic=0x%08lX (USR1=0x55535231 if used)\r\n",
                            (unsigned long)magic);
        }
        bsp_w25q16_end_session();
    }
}

#else

static void test_w25q_run(void)
{
    test_rs485_log_str("[W25Q] SKIP (TEST_ENABLE_W25Q_HW=0, 已确认硬件正常)\r\n");
}

#endif /* TEST_ENABLE_W25Q_HW */

#if (TEST_ENABLE_NFC_HW != 0)

static void test_print_nfc_hw_status(void)
{
    uint8_t ver;

    app_nfc_hw_init_once();
    ver = Read_MFRC522(0x37u);
    test_rs485_logf("[NFC] MFRC522 ver=0x%02X (OK=0x91/0x92) ready=%u reset_ret=%u(0x26=OK)\r\n",
                    (unsigned)ver,
                    (unsigned)app_nfc_hw_ready(),
                    (unsigned)app_nfc_last_reset_ret());
    if(ver != 0x91u && ver != 0x92u) {
        test_rs485_log_str("[NFC] FAIL SPI/power/RST (PB12/13/14/15 PD1)\r\n");
    } else {
        uint8_t tag_type[2];
        uint8_t tx = Read_MFRC522(0x14u);
        uint8_t rf = Read_MFRC522(0x26u);
        char req_st = MFRC522_Request(PICC_REQIDL, tag_type);
        test_rs485_logf("[NFC] chip OK TxCtrl=0x%02X RFCfg=0x%02X probe_req=0x%02X\r\n",
                        (unsigned)tx, (unsigned)rf, (unsigned)req_st);
        test_rs485_log_str("[NFC] place card on reader\r\n");
    }
    Flash_ReadID();
    test_rs485_logf("[NFC] internal flash card_count=%u\r\n", (unsigned)card_count);
}

static void test_log_uid(const char *tag, const uint8_t uid[4])
{
    test_rs485_logf("%s UID=%02X%02X%02X%02X\r\n",
                    tag,
                    (unsigned)uid[0], (unsigned)uid[1],
                    (unsigned)uid[2], (unsigned)uid[3]);
}

static void test_handle_enroll_result(uint8_t flash_ret, const uint8_t uid[4])
{
    switch(flash_ret) {
    case 0u:
        test_log_uid("[NFC] ENROLL OK", uid);
        test_rs485_logf("[NFC] saved total=%u\r\n", (unsigned)card_count);
        break;
    case 1u:
        test_rs485_log_str("[NFC] ENROLL FAIL storage full\r\n");
        break;
    case 2u:
        test_rs485_log_str("[NFC] ENROLL SKIP already exists\r\n");
        test_log_uid("[NFC] DUP", uid);
        break;
    default:
        test_rs485_logf("[NFC] ENROLL FAIL ret=%u\r\n", (unsigned)flash_ret);
        break;
    }
}

#else

static void test_print_nfc_hw_status(void)
{
    test_rs485_log_str("[NFC] SKIP (TEST_ENABLE_NFC_HW=0, 已确认硬件正常)\r\n");
}

#endif /* TEST_ENABLE_NFC_HW */

int main(void)
{
#if (TEST_ENABLE_NFC_HW != 0)
    uint8_t uid[4];
    uint8_t detect;
    uint8_t flash_ret;
#endif
    uint32_t last_hb = 0u;
    uint32_t last_prompt = 0u;
    uint32_t now;

    HAL_Init();
    app_ccm_ram_init();
    sys_stm32_clock_init(336, 8, 2, 7);
    delay_init(168);
    test_rs485_log_init();
    delay_ms(20);

    test_print_banner();
    if(app_rs485_link_ready() != 0u) {
        test_rs485_log_str("[HW_TEST] RS485OK\r\n");
    } else {
        test_rs485_log_str("[HW_TEST] RS485 FAIL (check PC6/7/8)\r\n");
    }

    test_w25q_run();

#if (TEST_ENABLE_UNLOCK_SW != 0)
    test_unlock_diag_run();
#else
    test_rs485_log_str("[AUTH] SKIP (TEST_ENABLE_UNLOCK_SW=0)\r\n");
#endif

#if (TEST_ENABLE_TOUCH_HW != 0)
    test_touch_diag_run_once();
#else
    test_rs485_log_str("[TP] SKIP (TEST_ENABLE_TOUCH_HW=0, 已确认硬件正常)\r\n");
#endif

#if (TEST_ENABLE_NFC_HW != 0)
    test_print_nfc_hw_status();
    MFRC522_AntennaOn();
#endif

    test_rs485_log_str("\r\n[HW_TEST] one-shot done, entering poll loop...\r\n");

    last_hb = HAL_GetTick();
    last_prompt = last_hb;

    while(1) {
        now = HAL_GetTick();

#if (TEST_ENABLE_TOUCH_HW != 0)
        test_touch_diag_poll();
#endif

#if (TEST_ENABLE_NFC_HW != 0)
        if((now - last_prompt) >= TEST_PROMPT_MS) {
            last_prompt = now;
            test_rs485_log_str("[NFC] waiting card...\r\n");
        }

        detect = NFC_Enroll_Detect(uid, TEST_DETECT_SLICE_MS);
        if(detect != 0u) {
            if(uid[0] == 0u && uid[1] == 0u && uid[2] == 0u && uid[3] == 0u) {
                test_rs485_log_str("[NFC] invalid UID 00000000\r\n");
            } else {
                test_log_uid("[NFC] CARD", uid);
                flash_ret = Flash_AddID(uid);
                test_handle_enroll_result(flash_ret, uid);
            }
            delay_ms(800);
            last_prompt = HAL_GetTick();
        }
#endif

        if((now - last_hb) >= TEST_HEARTBEAT_MS) {
            last_hb = now;
#if (TEST_ENABLE_NFC_HW != 0)
            test_rs485_logf("[HW_TEST] alive nfc_ready=%u cards=%u\r\n",
                            (unsigned)app_nfc_hw_ready(),
                            (unsigned)card_count);
#else
            test_rs485_log_str("[HW_TEST] alive\r\n");
#endif
        }
    }
}
