#include "key.h"
#include "./SYSTEM/delay/delay.h"

static const uint16_t KEY_ROW_PINS[4] = {
    KEY_ROW0_PIN, KEY_ROW1_PIN, KEY_ROW2_PIN, KEY_ROW3_PIN
};
static const uint16_t KEY_COL_PINS[4] = {
    KEY_COL0_PIN, KEY_COL1_PIN, KEY_COL2_PIN, KEY_COL3_PIN
};

/* row/col -> key mapping
 * row0: 1 2 3 OK
 * row1: 4 5 6 ESC
 * row2: 7 8 9 0
 * row3: LEFT UP DOWN RIGHT
 */
static const KeyValue_t KEY_MAP[4][4] = {
    { KEY_1,    KEY_2,   KEY_3,    KEY_OK   },
    { KEY_4,    KEY_5,   KEY_6,    KEY_ESC  },
    { KEY_7,    KEY_8,   KEY_9,    KEY_0    },
    { KEY_LEFT, KEY_UP,  KEY_DOWN, KEY_RIGHT}
};

void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* rows: push-pull output */
    GPIO_InitStructure.Pin = KEY_ROW0_PIN | KEY_ROW1_PIN | KEY_ROW2_PIN | KEY_ROW3_PIN;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStructure.Pull = GPIO_NOPULL;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY_ROW_PORT, &GPIO_InitStructure);
    HAL_GPIO_WritePin(KEY_ROW_PORT, KEY_ROW0_PIN | KEY_ROW1_PIN | KEY_ROW2_PIN | KEY_ROW3_PIN, GPIO_PIN_SET);

    /* cols: pull-up input */
    GPIO_InitStructure.Pin = KEY_COL0_PIN | KEY_COL1_PIN | KEY_COL2_PIN | KEY_COL3_PIN;
    GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
    GPIO_InitStructure.Pull = GPIO_PULLUP;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY_COL_PORT, &GPIO_InitStructure);

    /* PA1: output */
    GPIO_InitStructure.Pin = PA1_PIN;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStructure.Pull = GPIO_NOPULL;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PA1_PORT, &GPIO_InitStructure);
    HAL_GPIO_WritePin(PA1_PORT, PA1_PIN, GPIO_PIN_RESET);
}

KeyValue_t KEY_Scan(void)
{
    uint8_t row, col;

    for(row = 0; row < 4; row++) {
        HAL_GPIO_WritePin(KEY_ROW_PORT, KEY_ROW0_PIN | KEY_ROW1_PIN | KEY_ROW2_PIN | KEY_ROW3_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(KEY_ROW_PORT, KEY_ROW_PINS[row], GPIO_PIN_RESET);
        delay_us(50);

        for(col = 0; col < 4; col++) {
            if(HAL_GPIO_ReadPin(KEY_COL_PORT, KEY_COL_PINS[col]) == GPIO_PIN_RESET) {
                HAL_GPIO_WritePin(KEY_ROW_PORT, KEY_ROW0_PIN | KEY_ROW1_PIN | KEY_ROW2_PIN | KEY_ROW3_PIN, GPIO_PIN_SET);
                return KEY_MAP[row][col];
            }
        }
    }

    HAL_GPIO_WritePin(KEY_ROW_PORT, KEY_ROW0_PIN | KEY_ROW1_PIN | KEY_ROW2_PIN | KEY_ROW3_PIN, GPIO_PIN_SET);
    return KEY_NONE;
}

KeyValue_t KEY_Scan_WithDebounce(uint16_t debounce_ms)
{
    static KeyValue_t candidate = KEY_NONE;
    static KeyValue_t last_reported = KEY_NONE;
    static uint32_t candidate_start_ms = 0u;
    KeyValue_t now_key = KEY_Scan();
    uint32_t now_ms = HAL_GetTick();

    if(now_key == KEY_NONE) {
        candidate = KEY_NONE;
        last_reported = KEY_NONE;
        return KEY_NONE;
    }

    if(now_key != candidate) {
        candidate = now_key;
        candidate_start_ms = now_ms;
        return KEY_NONE;
    }

    if((now_ms - candidate_start_ms) < debounce_ms) {
        return KEY_NONE;
    }

    if(last_reported == candidate) {
        return KEY_NONE;
    }

    last_reported = candidate;
    return candidate;
}

void PA1_Toggle(void)
{
    HAL_GPIO_TogglePin(PA1_PORT, PA1_PIN);
}

void PA1_Set(uint8_t level)
{
    HAL_GPIO_WritePin(PA1_PORT, PA1_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

const char* KEY_GetName(KeyValue_t key)
{
    switch(key) {
        case KEY_0:     return "0";
        case KEY_1:     return "1";
        case KEY_2:     return "2";
        case KEY_3:     return "3";
        case KEY_4:     return "4";
        case KEY_5:     return "5";
        case KEY_6:     return "6";
        case KEY_7:     return "7";
        case KEY_8:     return "8";
        case KEY_9:     return "9";
        case KEY_OK:    return "OK";
        case KEY_ESC:   return "ESC";
        case KEY_LEFT:  return "LEFT";
        case KEY_UP:    return "UP";
        case KEY_DOWN:  return "DOWN";
        case KEY_RIGHT: return "RIGHT";
        default:        return "NONE";
    }
}

