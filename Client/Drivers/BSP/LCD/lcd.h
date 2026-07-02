#ifndef __LCD_H
#define __LCD_H

#include "stdlib.h"
#include "./SYSTEM/sys/sys.h"
#include "stm32f4xx_hal.h"

/******************************************************************************************/
/* ILI9341 SPI1: SCK/MOSI/MISO + CS/DC/RST (SPI1 ?? WR/RD/16??) */

#define LCD_SPI                         SPI1
#define LCD_SPI_CLK_ENABLE()            do { __HAL_RCC_SPI1_CLK_ENABLE(); } while (0)

#define LCD_SPI_SCK_GPIO_PORT           GPIOA
#define LCD_SPI_SCK_GPIO_PIN            GPIO_PIN_5
#define LCD_SPI_SCK_GPIO_AF             GPIO_AF5_SPI1
#define LCD_SPI_SCK_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOA_CLK_ENABLE(); } while (0)

#define LCD_SPI_MISO_GPIO_PORT          GPIOA
#define LCD_SPI_MISO_GPIO_PIN           GPIO_PIN_6
#define LCD_SPI_MISO_GPIO_AF            GPIO_AF5_SPI1
#define LCD_SPI_MISO_GPIO_CLK_ENABLE()  do { __HAL_RCC_GPIOA_CLK_ENABLE(); } while (0)

#define LCD_SPI_MOSI_GPIO_PORT          GPIOA
#define LCD_SPI_MOSI_GPIO_PIN           GPIO_PIN_7
#define LCD_SPI_MOSI_GPIO_AF            GPIO_AF5_SPI1
#define LCD_SPI_MOSI_GPIO_CLK_ENABLE()  do { __HAL_RCC_GPIOA_CLK_ENABLE(); } while (0)

#define LCD_RST_GPIO_PORT               GPIOE
#define LCD_RST_GPIO_PIN                GPIO_PIN_4
#define LCD_RST_GPIO_CLK_ENABLE()       do { __HAL_RCC_GPIOE_CLK_ENABLE(); } while (0)

#define LCD_DC_GPIO_PORT                GPIOE
#define LCD_DC_GPIO_PIN                 GPIO_PIN_6
#define LCD_DC_GPIO_CLK_ENABLE()        do { __HAL_RCC_GPIOE_CLK_ENABLE(); } while (0)

#define LCD_CS_GPIO_PORT                GPIOE
#define LCD_CS_GPIO_PIN                 GPIO_PIN_7
#define LCD_CS_GPIO_CLK_ENABLE()        do { __HAL_RCC_GPIOE_CLK_ENABLE(); } while (0)

/* SPI1 SCK：PCLK2=84MHz，/4≈21MHz */
#define LCD_SPI_BAUDRATE_PRESCALER      SPI_BAUDRATEPRESCALER_4

/******************************************************************************************/

/* LCD????????? */
typedef struct
{
    uint16_t width;     /* LCD ???? */
    uint16_t height;    /* LCD ??? */
    uint16_t id;        /* LCD ID */
    uint8_t dir;        /* ?????????????????0????????1???????? */
    uint16_t wramcmd;   /* ?????gram??? */
    uint16_t setxcmd;   /* ????x??????? */
    uint16_t setycmd;   /* ????y??????? */
} _lcd_dev;

/* LCD???? */
extern _lcd_dev lcddev; /* ????LCD??????? */

extern SPI_HandleTypeDef g_lcd_spi;

/* LCD?????????????? */
extern uint32_t  g_point_color;     /* ????? */
extern uint32_t  g_back_color;      /* ???????.??????? */

/* SPI ILI9341 ????????????????? */
#define LCD_BL(x)   ((void)0)

/******************************************************************************************/
/* LCD?????????? ???? */

/* ???????? */
#define L2R_U2D         0           /* ??????,??????? */
#define L2R_D2U         1           /* ??????,??????? */
#define R2L_U2D         2           /* ???????,??????? */
#define R2L_D2U         3           /* ???????,??????? */

#define U2D_L2R         4           /* ???????,?????? */
#define U2D_R2L         5           /* ???????,??????? */
#define D2U_L2R         6           /* ???????,?????? */
#define D2U_R2L         7           /* ???????,??????? */

#define DFT_SCAN_DIR    L2R_U2D     /* ?????????? */

/* ?????????? */
#define WHITE           0xFFFF      /* ??? */
#define BLACK           0x0000      /* ??? */
#define RED             0xF800      /* ??? */
#define GREEN           0x07E0      /* ??? */
#define BLUE            0x001F      /* ??? */ 
#define MAGENTA         0xF81F      /* ????/???? = BLUE + RED */
#define YELLOW          0xFFE0      /* ??? = GREEN + RED */
#define CYAN            0x07FF      /* ??? = GREEN + BLUE */  

/* ???????? */
#define BROWN           0xBC40      /* ??? */
#define BRRED           0xFC07      /* ???? */
#define GRAY            0x8430      /* ??? */ 
#define DARKBLUE        0x01CF      /* ????? */
#define LIGHTBLUE       0x7D7C      /* ???? */ 
#define GRAYBLUE        0x5458      /* ????? */ 
#define LIGHTGREEN      0x841F      /* ???? */  
#define LGRAY           0xC618      /* ????(PANNEL),???????? */ 
#define LGRAYBLUE       0xA651      /* ??????(????????) */ 
#define LBBLUE          0x2B12      /* ??????(??????????) */ 

/******************************************************************************************/
/* SSD1963???????????(lcd_ex.c ?????????????????????,?? SPI ILI9341 ????????) */

#define SSD_HOR_RESOLUTION      800
#define SSD_VER_RESOLUTION      480

#define SSD_HOR_PULSE_WIDTH     1
#define SSD_HOR_BACK_PORCH      46
#define SSD_HOR_FRONT_PORCH     210

#define SSD_VER_PULSE_WIDTH     1
#define SSD_VER_BACK_PORCH      23
#define SSD_VER_FRONT_PORCH     22

#define SSD_HT          (SSD_HOR_RESOLUTION + SSD_HOR_BACK_PORCH + SSD_HOR_FRONT_PORCH)
#define SSD_HPS         (SSD_HOR_BACK_PORCH)
#define SSD_VT          (SSD_VER_RESOLUTION + SSD_VER_BACK_PORCH + SSD_VER_FRONT_PORCH)
#define SSD_VPS         (SSD_VER_BACK_PORCH)

/******************************************************************************************/
/* ???????? */

void lcd_wr_data(volatile uint16_t data);            /* LCD?????? */
void lcd_wr_regno(volatile uint16_t regno);          /* LCD??????????/??? */
void lcd_write_reg(uint16_t regno, uint16_t data);   /* LCD?????????? */

void lcd_init(void);                        /* ?????LCD */
void lcd_display_on(void);                  /* ????? */ 
void lcd_display_off(void);                 /* ????? */
void lcd_scan_dir(uint8_t dir);             /* ???????????? */ 
void lcd_display_dir(uint8_t dir);          /* ?????????????? */ 
void lcd_ssd_backlight_set(uint8_t pwm);    /* SSD1963 ???????(ILI9341 SPI????,?????) */ 

void lcd_write_ram_prepare(void);                           /* ?????GRAM */ 
void lcd_set_cursor(uint16_t x, uint16_t y);                /* ??????? */ 
uint32_t lcd_read_point(uint16_t x, uint16_t y);            /* ????(32?????,????LTDC) */
void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color);/* ????(32?????,????LTDC) */

void lcd_clear(uint16_t color);                                                             /* LCD???? */
void lcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color);                   /* ??????? */
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);                  /* ??? */
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);                  /* ?????? */
void lcd_set_window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height);             /* ??????? */
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color);          /* ?????????(32?????,????LTDC) */
void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);   /* ????????? */
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);     /* ????? */
void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);/* ?????? */

void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color);                       /* ????????? */
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color);                     /* ??????? */
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color);      /* ?????????? */
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color);   /* ???????? */

#endif
