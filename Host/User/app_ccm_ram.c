#include "app_ccm_ram.h"

void app_ccm_ram_init(void)
{
    __HAL_RCC_CCMDATARAMEN_CLK_ENABLE();
}
