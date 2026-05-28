#ifndef APP_FP_HW_DIAG_H
#define APP_FP_HW_DIAG_H

#include <stdint.h>

/* Boot AS608 self-test; logs via FP_MIRROR_LOG (ASCII only on USART1). */
void app_fp_hw_boot_diag(uint8_t *fp_hw_inited);

#endif /* APP_FP_HW_DIAG_H */
