/**
 * @file test_main.c
 * @brief NFC ???????????MFRC522 ???? + RS485(USART6) ??????????
 *
 * ??????? Host ?????????????
 *   RS485: PC6=TX(DI), PC7=RX(RO), PC8=DE/RE????????????, 115200 8N1
 *   MFRC522: PB12-15 SPI, PD1 RST
 *
 * ?????????USB-RS485 ?????? A/B???????? 115200??????? [NFC_TEST] ????????
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "stm32f4xx_hal.h"

#include "app_config.h"
#include "app_nfc_hw.h"
#include "door.h"
#include "MFRC522.h"
#include "test_rs485_log.h"

#define TEST_DETECT_SLICE_MS   40u
#define TEST_HEARTBEAT_MS      3000u
#define TEST_PROMPT_MS         2000u

static void test_print_banner(void)
{
    test_rs485_log_str("\r\n");
    test_rs485_log_str("========================================\r\n");
    test_rs485_log_str("[NFC_TEST] NFC enroll hardware test\r\n");
    test_rs485_log_str("[NFC_TEST] RS485 USART6 PC6/PC7/PC8 115200\r\n");
    test_rs485_log_str("[NFC_TEST] Place card on reader to enroll\r\n");
    test_rs485_log_str("========================================\r\n");
}

static void test_print_nfc_hw_status(void)
{
    uint8_t ver;

    app_nfc_hw_init_once();
    ver = Read_MFRC522(0x37u);
    test_rs485_logf("[NFC_TEST] MFRC522 ver=0x%02X ready=%u reset_ret=%u\r\n",
                    (unsigned)ver,
                    (unsigned)app_nfc_hw_ready(),
                    (unsigned)app_nfc_last_reset_ret());
    Flash_ReadID();
    test_rs485_logf("[NFC_TEST] flash card_count=%u\r\n", (unsigned)card_count);
}

static void test_log_uid(const char *tag, const uint8_t uid[4])
{
    test_rs485_logf("%s UID=%02X %02X %02X %02X\r\n",
                    tag,
                    (unsigned)uid[0], (unsigned)uid[1],
                    (unsigned)uid[2], (unsigned)uid[3]);
}

static void test_handle_enroll_result(uint8_t flash_ret, const uint8_t uid[4])
{
    switch(flash_ret) {
    case 0u:
        test_log_uid("[NFC_TEST] ENROLL OK", uid);
        test_rs485_logf("[NFC_TEST] saved total=%u\r\n", (unsigned)card_count);
        break;
    case 1u:
        test_rs485_log_str("[NFC_TEST] ENROLL FAIL storage full\r\n");
        break;
    case 2u:
        test_rs485_log_str("[NFC_TEST] ENROLL SKIP card already exists\r\n");
        test_log_uid("[NFC_TEST] DUP", uid);
        break;
    default:
        test_rs485_logf("[NFC_TEST] ENROLL FAIL flash_ret=%u\r\n", (unsigned)flash_ret);
        break;
    }
}

int main(void)
{
    uint8_t uid[4];
    uint8_t detect;
    uint8_t flash_ret;
    uint32_t last_hb = 0u;
    uint32_t last_prompt = 0u;
    uint32_t now;

    HAL_Init();
    sys_stm32_clock_init(336, 8, 2, 7);
    delay_init(168);
    test_rs485_log_init();

    test_print_banner();
    test_print_nfc_hw_status();
    MFRC522_AntennaOn();

    last_hb = HAL_GetTick();
    last_prompt = last_hb;

    while(1) {
        now = HAL_GetTick();

        if((now - last_prompt) >= TEST_PROMPT_MS) {
            last_prompt = now;
            test_rs485_log_str("[NFC_TEST] waiting card...\r\n");
        }

        detect = NFC_Enroll_Detect(uid, TEST_DETECT_SLICE_MS);
        if(detect != 0u) {
            if(uid[0] == 0u && uid[1] == 0u && uid[2] == 0u && uid[3] == 0u) {
                test_rs485_log_str("[NFC_TEST] detect invalid UID=00000000\r\n");
            } else {
                test_log_uid("[NFC_TEST] CARD", uid);
                flash_ret = Flash_AddID(uid);
                test_handle_enroll_result(flash_ret, uid);
            }
            delay_ms(800);
            last_prompt = HAL_GetTick();
        }

        if((now - last_hb) >= TEST_HEARTBEAT_MS) {
            last_hb = now;
            test_rs485_logf("[NFC_TEST] alive nfc_ready=%u cards=%u\r\n",
                            (unsigned)app_nfc_hw_ready(),
                            (unsigned)card_count);
        }
    }
}
