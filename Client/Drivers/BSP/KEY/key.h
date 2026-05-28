#ifndef __KEY_H
#define __KEY_H

#include "stm32f4xx_hal.h"

// ??? (PD4-PD7)
#define KEY_ROW0_PIN    GPIO_PIN_4
#define KEY_ROW1_PIN    GPIO_PIN_5
#define KEY_ROW2_PIN    GPIO_PIN_6
#define KEY_ROW3_PIN    GPIO_PIN_7
#define KEY_ROW_PORT    GPIOD

// ??? (PD8-PD11)
#define KEY_COL0_PIN    GPIO_PIN_8
#define KEY_COL1_PIN    GPIO_PIN_9
#define KEY_COL2_PIN    GPIO_PIN_10
#define KEY_COL3_PIN    GPIO_PIN_11
#define KEY_COL_PORT    GPIOD

// PA1 ????
#define PA1_PIN         GPIO_PIN_1
#define PA1_PORT        GPIOA

// ????
#define KEY_PRESSED     0
#define KEY_RELEASED    1

// ????? - ???????
typedef enum {
    KEY_NONE = 0xFF,
    
    // ?0?: 1 2 3 OK
    KEY_1 = 0,      // (0,0)
    KEY_2,          // (0,1)
    KEY_3,          // (0,2)
    KEY_OK,         // (0,3)
    
    // ?1?: 4 5 6 ESC
    KEY_4,          // (1,0)
    KEY_5,          // (1,1)
    KEY_6,          // (1,2)
    KEY_ESC,        // (1,3)
    
    // ?2?: 7 8 9 0
    KEY_7,          // (2,0)
    KEY_8,          // (2,1)
    KEY_9,          // (2,2)
    KEY_0,          // (2,3)
    
    // ?3?: LEFT UP DOWN RIGHT
    KEY_LEFT,       // (3,0)
    KEY_UP,         // (3,1)
    KEY_DOWN,       // (3,2)
    KEY_RIGHT       // (3,3)
} KeyValue_t;

// ????
void KEY_Init(void);
KeyValue_t KEY_Scan(void);
KeyValue_t KEY_Scan_WithDebounce(uint16_t debounce_ms);
void PA1_Toggle(void);
void PA1_Set(uint8_t level);

// ??????(????????,????key.c)
const char* KEY_GetName(KeyValue_t key);

#endif

