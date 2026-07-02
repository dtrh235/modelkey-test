#ifndef TEST_NFC_H
#define TEST_NFC_H

#include <stdint.h>

/* 1=MFRC522 VersionReg 0x91/0x92 */
uint8_t test_nfc_run_once(void);
void test_nfc_poll(void);

#endif /* TEST_NFC_H */
