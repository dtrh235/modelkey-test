#ifndef __TOUCH_H__
#define __TOUCH_H__

#include "./SYSTEM/sys/sys.h"
#include "app_config.h"
#include "./BSP/TOUCH/ft5206.h"
#if !defined(APP_TP_FT6336_CAP_ONLY) || (APP_TP_FT6336_CAP_ONLY == 0)
#include "./BSP/TOUCH/gt9xxx.h"
#endif


/******************************************************************************************/
/* 主机 ILI9341(SPI 显示) + FT6336 电容触摸(I2C，驱动复用 ft5206.c)
 * I2C SCL -> PB6
 * I2C SDA -> PB7
 * INT     -> PB1  (低电平=有触摸)
 * RST     -> PE8
 *
 * 下方 T_PEN/T_MOSI 等仅用于旧版电阻屏分支(非本屏)。
 */
#define T_PEN_GPIO_PORT                 GPIOB
#define T_PEN_GPIO_PIN                  GPIO_PIN_1
#define T_PEN_GPIO_CLK_ENABLE()         do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

#define T_CS_GPIO_PORT                  GPIOE
#define T_CS_GPIO_PIN                   GPIO_PIN_8
#define T_CS_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOE_CLK_ENABLE(); }while(0)

#define TP_SDA_SHARED_LINE              1
#define T_MOSI_GPIO_PORT                GPIOB
#define T_MOSI_GPIO_PIN                 GPIO_PIN_7
#define T_MOSI_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)
#define T_MISO_GPIO_PORT                T_MOSI_GPIO_PORT
#define T_MISO_GPIO_PIN                 T_MOSI_GPIO_PIN
#define T_MISO_GPIO_CLK_ENABLE()        T_MOSI_GPIO_CLK_ENABLE()

#define T_CLK_GPIO_PORT                 GPIOB
#define T_CLK_GPIO_PIN                  GPIO_PIN_6
#define T_CLK_GPIO_CLK_ENABLE()         do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

/******************************************************************************************/

#define T_PEN           HAL_GPIO_ReadPin(T_PEN_GPIO_PORT, T_PEN_GPIO_PIN)
#define T_MISO          HAL_GPIO_ReadPin(T_MISO_GPIO_PORT, T_MISO_GPIO_PIN)

#define T_MOSI(x)     do{ x ? \
                          HAL_GPIO_WritePin(T_MOSI_GPIO_PORT, T_MOSI_GPIO_PIN, GPIO_PIN_SET) : \
                          HAL_GPIO_WritePin(T_MOSI_GPIO_PORT, T_MOSI_GPIO_PIN, GPIO_PIN_RESET); \
                      }while(0)

#define T_CLK(x)      do{ x ? \
                          HAL_GPIO_WritePin(T_CLK_GPIO_PORT, T_CLK_GPIO_PIN, GPIO_PIN_SET) : \
                          HAL_GPIO_WritePin(T_CLK_GPIO_PORT, T_CLK_GPIO_PIN, GPIO_PIN_RESET); \
                      }while(0)

#define T_CS(x)       do{ x ? \
                          HAL_GPIO_WritePin(T_CS_GPIO_PORT, T_CS_GPIO_PIN, GPIO_PIN_SET) : \
                          HAL_GPIO_WritePin(T_CS_GPIO_PORT, T_CS_GPIO_PIN, GPIO_PIN_RESET); \
                      }while(0)


#define TP_PRES_DOWN    0x8000
#define TP_CATH_PRES    0x4000
#define CT_MAX_TOUCH    10

typedef struct
{
    uint8_t (*init)(void);
    uint8_t (*scan)(uint8_t);
    void (*adjust)(void);
    uint16_t x[CT_MAX_TOUCH];
    uint16_t y[CT_MAX_TOUCH];
    uint16_t sta;
    float xfac;
    float yfac;
    short xc;
    short yc;
    uint8_t touchtype;
} _m_tp_dev;

extern _m_tp_dev tp_dev;

uint8_t tp_init(void);
void tp_adjust(void);
void tp_save_adjust_data(void);
uint8_t tp_get_adjust_data(void);
void tp_draw_big_point(uint16_t x, uint16_t y, uint16_t color);
void tp_debug_sample(uint16_t *raw_x, uint16_t *raw_y, uint8_t *pen_high);
void tp_ensure_cal(void);
void tp_debug_probe_miso_pins(void);
uint8_t tp_cap_chip_id_read(uint8_t *id);
void tp_cap_diag_verbose(void);

#endif
