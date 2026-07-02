#ifndef APP_SCREEN8_ENROLL_H
#define APP_SCREEN8_ENROLL_H

#include <stdint.h>

void screen8_show_fp_enroll_popup(void);
uint8_t screen8_start_fp_enroll_hw(void);
void screen8_handle_fp_enroll(void);
void screen8_show_nfc_enroll_popup(void);
uint8_t screen8_start_nfc_enroll_hw(void);
void screen8_handle_nfc_enroll(void);

#endif
