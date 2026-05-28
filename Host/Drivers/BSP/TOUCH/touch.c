/**
 ****************************************************************************************************
 * @file        touch.c
 * @author      genbotter
 * @version     V1.0
 * @date        2023-04-23
 * @brief       ?????????????????
 * @license     Copyright (c) 2020-2032,  
 ****************************************************************************************************
 * @attention
 * 
 * ?????:ST-1 STM32F407?????
 *  
 *  
 * ??????:www.genbotter.com
 * ??????:makerbase.taobao.com
 * 
 ****************************************************************************************************
 */

#include "stdio.h"
#include "stdlib.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/TOUCH/ctiic.h"
#include "./BSP/24CXX/24cxx.h"
#include "./SYSTEM/delay/delay.h"
#include "app_config.h"
#if (APP_TOUCH_UART_DEBUG != 0)
#include "app_touch_diag.h"
#endif

#if !defined(APP_TP_FT6336_CAP_ONLY) || (APP_TP_FT6336_CAP_ONLY == 0)
static void tp_sda_output(void);
static void tp_sda_input(void);
static void tp_bus_idle(void);
static uint8_t tp_raw_valid(uint16_t x, uint16_t y);
static uint8_t tp_pen_down(void);
static void tp_write_byte(uint8_t data);
static uint16_t tp_read_ad(uint8_t cmd);
static uint16_t tp_read_xoy(uint8_t cmd);
static void tp_read_xy(uint16_t *x, uint16_t *y);
static uint8_t tp_read_xy2(uint16_t *x, uint16_t *y);
static uint8_t tp_scan(uint8_t mode);

static void tp_apply_fallback_cal(void)
{
    tp_dev.xfac = 0.074586f;
    tp_dev.yfac = 0.069513f;
    tp_dev.xc = 2046;
    tp_dev.yc = 1952;
}
#else
static void tp_apply_fallback_cal(void)
{
}
#endif

void tp_debug_sample(uint16_t *raw_x, uint16_t *raw_y, uint8_t *pen_high)
{
    if((tp_dev.touchtype & 0x80u) != 0u) {
        if(raw_x != NULL) {
            *raw_x = 0u;
        }
        if(raw_y != NULL) {
            *raw_y = 0u;
        }
        if(pen_high != NULL) {
            *pen_high = (FT5206_INT == 0) ? 0u : 1u;
        }
        return;
    }
#if !defined(APP_TP_FT6336_CAP_ONLY) || (APP_TP_FT6336_CAP_ONLY == 0)
    if(raw_x != NULL && raw_y != NULL) {
        tp_read_xy(raw_x, raw_y);
    }
    if(pen_high != NULL) {
        *pen_high = (T_PEN != 0) ? 1u : 0u;
    }
#endif
}

uint8_t tp_cap_chip_id_read(uint8_t *id)
{
    uint8_t v = 0u;

    if(id == NULL) {
        return 1u;
    }
    ft5206_rd_reg(FT5206_CHIP_ID_REG, &v, 1);
    *id = v;
    return 0u;
}

#if (APP_TOUCH_UART_DEBUG != 0)
static void tp_cap_gpio_rst_int_init(void)
{
    static uint8_t s_gpio_done = 0u;
    GPIO_InitTypeDef gpio_init_struct;

    if(s_gpio_done != 0u) {
        return;
    }
    s_gpio_done = 1u;

    FT5206_RST_GPIO_CLK_ENABLE();
    FT5206_INT_GPIO_CLK_ENABLE();

    gpio_init_struct.Pin = FT5206_RST_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(FT5206_RST_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = FT5206_INT_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(FT5206_INT_GPIO_PORT, &gpio_init_struct);
}

static void tp_cap_diag_one_bus(uint8_t bus_id, GPIO_TypeDef *scl_port, uint16_t scl_pin,
                                GPIO_TypeDef *sda_port, uint16_t sda_pin, const char *name)
{
    uint8_t ack70;
    uint8_t ack28;
    uint8_t id;
    uint8_t td;
    uint8_t a0;
    uint8_t a1;
    uint8_t a2;

    ct_iic_bind(scl_port, scl_pin, sda_port, sda_pin, bus_id);
    ct_iic_init();

    ack70 = ct_iic_probe_wr(0x70u);
    ack28 = ct_iic_probe_wr(0x28u);

    FT5206_RST(0);
    delay_ms(20);
    FT5206_RST(1);
    delay_ms(50);

    id = 0u;
    td = 0u;
    a0 = 1u;
    a1 = 1u;
    a2 = 1u;
    (void)ct_iic_rd_reg_trace(0x70u, 0x71u, 0xA3u, &id, 1u, &a0, &a1, &a2);
    (void)ct_iic_rd_reg_trace(0x70u, 0x71u, 0x02u, &td, 1u, &a0, &a1, &a2);

    TOUCH_LOG("[TP] %s SCL=%u SDA=%u ack70=%u ack28=%u INT=%u\r\n",
              name,
              (unsigned)ct_iic_pin_read_scl(),
              (unsigned)ct_iic_pin_read_sda(),
              (unsigned)ack70,
              (unsigned)ack28,
              (unsigned)(FT5206_INT == 0 ? 0u : 1u));
    TOUCH_LOG("[TP] %s reg A3=0x%02X TD02=0x%02X ack_rd=%u,%u,%u\r\n",
              name,
              (unsigned)id,
              (unsigned)td,
              (unsigned)a0,
              (unsigned)a1,
              (unsigned)a2);
}

void tp_cap_diag_verbose(void)
{
    uint8_t v;

    tp_cap_gpio_rst_int_init();

    TOUCH_LOG("[TP] diag lcd=0x%04X bus=%s (ack 0=OK 1=fail)\r\n",
              (unsigned)lcddev.id,
              ct_iic_bus_name());
    TOUCH_LOG("[TP] RST(PE8)=%u INT(PB1)=%u (0=touch)\r\n",
              (unsigned)(HAL_GPIO_ReadPin(FT5206_RST_GPIO_PORT, FT5206_RST_GPIO_PIN) == GPIO_PIN_SET ? 1u : 0u),
              (unsigned)(FT5206_INT == 0 ? 0u : 1u));

    tp_cap_diag_one_bus(0u, GPIOB, GPIO_PIN_6, GPIOB, GPIO_PIN_7, "SCL6_SDA7");
    tp_cap_diag_one_bus(0u, GPIOB, GPIO_PIN_7, GPIOB, GPIO_PIN_6, "SCL7_SDA6");

    ct_iic_bind(GPIOB, GPIO_PIN_6, GPIOB, GPIO_PIN_7, 0u);
    ct_iic_init();

    (void)tp_dev.scan(0);
    (void)tp_cap_chip_id_read(&v);
    TOUCH_LOG("[TP] active=%s sta=0x%04X xy=%u,%u id=0x%02X\r\n",
              ct_iic_bus_name(),
              (unsigned)tp_dev.sta,
              (unsigned)tp_dev.x[0],
              (unsigned)tp_dev.y[0],
              (unsigned)v);

    if(ct_iic_probe_wr(0x70u) != 0u) {
        TOUCH_LOG("[TP] FAIL: no ACK on PB6/PB7\r\n");
    } else if(v == 0x51u) {
        TOUCH_LOG("[TP] OK: FT6336 id=0x51\r\n");
    } else {
        TOUCH_LOG("[TP] OK: I2C ACK id(A3)=0x%02X (touch may still work)\r\n", (unsigned)v);
    }
    TOUCH_LOG("[TP] hint: press screen expect [TP] down\r\n");
}
#endif

void tp_ensure_cal(void)
{
    if((tp_dev.touchtype & 0x80u) != 0u) {
        return;
    }
    if(tp_dev.xfac == 0.0f || tp_dev.yfac == 0.0f) {
        tp_apply_fallback_cal();
    }
}

#if (APP_TOUCH_UART_DEBUG != 0) && (!defined(APP_TP_FT6336_CAP_ONLY) || (APP_TP_FT6336_CAP_ONLY == 0))

static uint16_t tp_probe_read_on_pin(GPIO_TypeDef *port, uint16_t pin, uint8_t cmd)
{
    uint8_t count;
    uint16_t num = 0;
    GPIO_InitTypeDef gpio_init_struct;

    gpio_init_struct.Pin = pin;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(port, &gpio_init_struct);

    T_CLK(0);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    T_CS(0);
    tp_write_byte(cmd);
    delay_us(10);
    T_CLK(0);
    delay_us(2);
    T_CLK(1);
    delay_us(2);
    T_CLK(0);

    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(port, &gpio_init_struct);
    delay_us(2);

    for(count = 0; count < 16; count++) {
        num <<= 1;
        T_CLK(0);
        delay_us(1);
        T_CLK(1);
        if(HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) {
            num++;
        }
    }
    num >>= 4;
    T_CS(1);
    tp_sda_output();
    tp_bus_idle();
    return num;
}

void tp_debug_probe_miso_pins(void)
{
    uint16_t v_pb7;
    uint16_t v_pb2;
    uint16_t v_pb14;

    if((tp_dev.touchtype & 0x80u) != 0u) {
        return;
    }

    v_pb7 = tp_probe_read_on_pin(GPIOB, GPIO_PIN_7, 0xD0);
    v_pb2 = tp_probe_read_on_pin(GPIOB, GPIO_PIN_2, 0xD0);
    v_pb14 = tp_probe_read_on_pin(GPIOB, GPIO_PIN_14, 0xD0);

    TOUCH_LOG("[TP] probe X@PB7=%u PB2=%u PB14=%u (ok:200-3800)\r\n",
              (unsigned)v_pb7, (unsigned)v_pb2, (unsigned)v_pb14);
}

#elif (APP_TOUCH_UART_DEBUG != 0)
void tp_debug_probe_miso_pins(void)
{
}
#endif


_m_tp_dev tp_dev =
{
    tp_init,
#if defined(APP_TP_FT6336_CAP_ONLY) && (APP_TP_FT6336_CAP_ONLY != 0)
    ft5206_scan,
#else
    tp_scan,
#endif
    tp_adjust,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

#if !defined(APP_TP_FT6336_CAP_ONLY) || (APP_TP_FT6336_CAP_ONLY == 0)
/* T_SDI/T_SDO ????? PB7 ??????????????????????????????? */
static void tp_sda_output(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    gpio_init_struct.Pin = T_MOSI_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(T_MOSI_GPIO_PORT, &gpio_init_struct);
}

static void tp_sda_input(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    gpio_init_struct.Pin = T_MISO_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(T_MISO_GPIO_PORT, &gpio_init_struct);
}

static void tp_bus_idle(void)
{
    T_CS(1);
    T_CLK(0);
    tp_sda_output();
    T_MOSI(1);
}

/* SPI 读数有效：非全 0 / 非全 1（4095） */
static uint8_t tp_raw_valid(uint16_t x, uint16_t y)
{
    if(x == 0u && y == 0u) {
        return 0u;
    }
    if(x >= 4080u && y >= 4080u) {
        return 0u;
    }
    return 1u;
}

static uint8_t tp_pen_down(void)
{
    if(T_PEN == 0) {
        return 1u;
    }
    return 0u;
}

/**
 * @brief       SPI??????
 *   @note      ??????IC????1 byte????
 * @param       data: ??????????
 * @retval      ??
 */
static void tp_write_byte(uint8_t data)
{
    uint8_t count = 0;

    tp_sda_output();

    for (count = 0; count < 8; count++)
    {
        if (data & 0x80)    /* ????1 */
        {
            T_MOSI(1);
        }
        else                /* ????0 */
        {
            T_MOSI(0);
        }

        data <<= 1;
        T_CLK(0);
        delay_us(1);
        T_CLK(1);           /* ?????????? */
    }
}

/**
 * @brief       SPI??????
 *   @note      ???????IC???adc?
 * @param       cmd: ???
 * @retval      ???????????,ADC?(12bit)
 */
static uint16_t tp_read_ad(uint8_t cmd)
{
    uint8_t count = 0;
    uint16_t num = 0;
    
    T_CLK(0);           /* ????????? */
    T_MOSI(0);          /* ?????????? */
    T_CS(0);            /* ?????????IC */
    tp_write_byte(cmd); /* ?????????? */
    delay_us(10);
    T_CLK(0);
    delay_us(2);
    T_CLK(1);
    delay_us(2);
    T_CLK(0);

    tp_sda_input();
    delay_us(2);

    for (count = 0; count < 16; count++)    /* ????16??????,?????12?????? */
    {
        num <<= 1;
        T_CLK(0);       /* ????????? */
        delay_us(1);
        T_CLK(1);

        if (T_MISO) num++;
    }

    num >>= 4;          /* ?????12??????. */
    T_CS(1);            /* ????? */
    tp_sda_output();
    return num;
}

/* ?????????????? ?????? ????????? */
#define TP_READ_TIMES   5       /* ??????? */
#define TP_LOST_VAL     1       /* ????? */

/**
 * @brief       ???????????(x????y)
 *   @note      ???????TP_READ_TIMES??????,??????????????????,
 *              ?????????????TP_LOST_VAL????, ?????
 *              ???????????: TP_READ_TIMES > 2*TP_LOST_VAL ??????
 *
 * @param       cmd : ???
 *   @arg       0XD0: ???X??????(@??????,????????Y???.)
 *   @arg       0X90: ???Y??????(@??????,????????X???.)
 *
 * @retval      ???????????(??????), ADC?(12bit)
 */
static uint16_t tp_read_xoy(uint8_t cmd)
{
    uint16_t i, j;
    uint16_t buf[TP_READ_TIMES];
    uint16_t sum = 0;
    uint16_t temp;

    for (i = 0; i < TP_READ_TIMES; i++)     /* ????TP_READ_TIMES?????? */
    {
        buf[i] = tp_read_ad(cmd);
    }

    for (i = 0; i < TP_READ_TIMES - 1; i++) /* ????????????? */
    {
        for (j = i + 1; j < TP_READ_TIMES; j++)
        {
            if (buf[i] > buf[j])   /* ???????? */
            {
                temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }

    sum = 0;

    for (i = TP_LOST_VAL; i < TP_READ_TIMES - TP_LOST_VAL; i++)   /* ???????????? */
    {
        sum += buf[i];  /* ???????????????????. */
    }

    temp = sum / (TP_READ_TIMES - 2 * TP_LOST_VAL); /* ????? */
    return temp;
}

/**
 * @brief       ???x, y????
 * @param       x,y: ????????????
 * @retval      ??
 */
static void tp_read_xy(uint16_t *x, uint16_t *y)
{
    uint16_t xval, yval;

    if (tp_dev.touchtype & 0X01)    /* X,Y??????????? */
    {
        xval = tp_read_xoy(0X90);   /* ???X??????AD?, ???????????? */
        yval = tp_read_xoy(0XD0);   /* ???Y??????AD? */
    }
    else                            /* X,Y???????????? */
    {
        xval = tp_read_xoy(0XD0);   /* ???X??????AD? */
        yval = tp_read_xoy(0X90);   /* ???Y??????AD? */
    }

    *x = xval;
    *y = yval;
}

/* ???????????X,Y???????????????????? */
#define TP_ERR_RANGE    50      /* ????? */

/**
 * @brief       ???????2??????IC????, ?????
 *   @note      ????2???????????IC,??????????????????ERR_RANGE,????
 *              ????,????????????,???????????.?????????????????.
 *
 * @param       x,y: ????????????
 * @retval      0, ???; 1, ???;
 */
static uint8_t tp_read_xy2(uint16_t *x, uint16_t *y)
{
    uint16_t x1, y1;
    uint16_t x2, y2;

    tp_read_xy(&x1, &y1);   /* ???????????? */
    tp_read_xy(&x2, &y2);   /* ???????????? */

    /* ?????????????+-TP_ERR_RANGE?? */
    if (((x2 <= x1 && x1 < x2 + TP_ERR_RANGE) || (x1 <= x2 && x2 < x1 + TP_ERR_RANGE)) &&
            ((y2 <= y1 && y1 < y2 + TP_ERR_RANGE) || (y1 <= y2 && y2 < y1 + TP_ERR_RANGE)))
    {
        *x = (x1 + x2) / 2;
        *y = (y1 + y2) / 2;
        return 1;
    }

    return 0;
}

/******************************************************************************************/
/* ??LCD????????????, ?????????? */

/**
 * @brief       ????????????????(????)
 * @param       x,y   : ????
 * @param       color : ???
 * @retval      ??
 */
static void tp_draw_touch_point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_draw_line(x - 12, y, x + 13, y, color); /* ???? */
    lcd_draw_line(x, y - 12, x, y + 13, color); /* ???? */
    lcd_draw_point(x + 1, y + 1, color);
    lcd_draw_point(x - 1, y + 1, color);
    lcd_draw_point(x + 1, y - 1, color);
    lcd_draw_point(x - 1, y - 1, color);
    lcd_draw_circle(x, y, 6, color);            /* ??????? */
}

/**
 * @brief       ????????(2*2???)
 * @param       x,y   : ????
 * @param       color : ???
 * @retval      ??
 */
void tp_draw_big_point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_draw_point(x, y, color);       /* ????? */
    lcd_draw_point(x + 1, y, color);
    lcd_draw_point(x, y + 1, color);
    lcd_draw_point(x + 1, y + 1, color);
}

/******************************************************************************************/

/**
 * @brief       ???????????
 * @param       mode: ??????
 *   @arg       0, ???????;
 *   @arg       1, ????????(?????????????)
 *
 * @retval      0, ?????????; 1, ??????????;
 */
static uint8_t tp_scan(uint8_t mode)
{
    uint16_t raw_x = 0u;
    uint16_t raw_y = 0u;
    uint8_t pressed = tp_pen_down();

    if(!pressed) {
        tp_read_xy(&raw_x, &raw_y);
        if(tp_raw_valid(raw_x, raw_y)) {
            pressed = 1u;
        }
    }

    if (pressed)
    {
        if (mode)       /* ???????????, ??????? */
        {
            tp_read_xy2(&tp_dev.x[0], &tp_dev.y[0]);
        }
        else if (tp_read_xy2(&tp_dev.x[0], &tp_dev.y[0]))     /* ??????????, ?????? */
        {
            /* ??X?? ????????????????????(?????LCD????????X?????) */
            tp_dev.x[0] = (signed short)(tp_dev.x[0] - tp_dev.xc) / tp_dev.xfac + lcddev.width / 2;

            /* ??Y?? ????????????????????(?????LCD????????Y?????) */
            tp_dev.y[0] = (signed short)(tp_dev.y[0] - tp_dev.yc) / tp_dev.yfac + lcddev.height / 2;
        }

        if ((tp_dev.sta & TP_PRES_DOWN) == 0)   /* ??????????? */
        {
            tp_dev.sta = TP_PRES_DOWN | TP_CATH_PRES;   /* ???????? */
            tp_dev.x[CT_MAX_TOUCH - 1] = tp_dev.x[0];   /* ?????????????????? */
            tp_dev.y[CT_MAX_TOUCH - 1] = tp_dev.y[0];
        }
    }
    else
    {
        if (tp_dev.sta & TP_PRES_DOWN)      /* ?????????? */
        {
            tp_dev.sta &= ~TP_PRES_DOWN;    /* ????????? */
        }
        else     /* ????????????? */
        {
            tp_dev.x[CT_MAX_TOUCH - 1] = 0;
            tp_dev.y[CT_MAX_TOUCH - 1] = 0;
            tp_dev.x[0] = 0xFFFF;
            tp_dev.y[0] = 0xFFFF;
        }
    }

    return tp_dev.sta & TP_PRES_DOWN; /* ????????????? */
}

#endif /* !APP_TP_FT6336_CAP_ONLY */

/* TP_SAVE_ADDR_BASE??????????????????????EEPROM?????????(??????)
 * ????? : 13???.
 */
#define TP_SAVE_ADDR_BASE   40

/**
 * @brief       ???????????
 *   @note      ??????????EEPROM???????(24C02),???????TP_SAVE_ADDR_BASE.
 *              ???????13???
 * @param       ??
 * @retval      ??
 */
void tp_save_adjust_data(void)
{
    uint8_t *p = (uint8_t *)&tp_dev.xfac;   /* ??????? */

    /* p???tp_dev.xfac????, p+4????tp_dev.yfac????
     * p+8????tp_dev.xoff????,p+10,????tp_dev.yoff????
     * ??????12?????(4??????)
     * p+12????????????????????????????(0X0A)
     * ??p[12]????0X0A. ???????????.
     */
    at24cxx_write(TP_SAVE_ADDR_BASE, p, 12);                /* ????12?????????(xfac,yfac,xc,yc) */
    at24cxx_write_one_byte(TP_SAVE_ADDR_BASE + 12, 0X0A);   /* ???????? */
}

/**
 * @brief       ?????????EEPROM?????????
 * @param       ??
 * @retval      0?????????????????
 *              1????????????
 */
uint8_t tp_get_adjust_data(void)
{
    uint8_t *p = (uint8_t *)&tp_dev.xfac;
    uint8_t temp = 0;

    /* ????????????????tp_dev.xfac????????????, ????????,???????????????
     * ???????tp_dev.xfac??????, ????????????????, ?????????????????
     * ??????. ??????????????????(????????)?????/???(????????).
     */
    at24cxx_read(TP_SAVE_ADDR_BASE, p, 12);                 /* ???12??????? */
    temp = at24cxx_read_one_byte(TP_SAVE_ADDR_BASE + 12);   /* ??????????? */

    if (temp == 0X0A)
    {
        return 1;
    }

    return 0;
}

#if !defined(APP_TP_FT6336_CAP_ONLY) || (APP_TP_FT6336_CAP_ONLY == 0)
char *const TP_REMIND_MSG_TBL = "Please use the stylus click the cross on the screen.The cross will always move until the screen adjustment is completed.";

/**
 * @brief       ?????????(????????)
 * @param       xy[5][2]: 5???????????
 * @param       px,py   : x,y????????????(????1???)
 * @retval      ??
 */
static void tp_adjust_info_show(uint16_t xy[5][2], double px, double py)
{
    uint8_t i;
    char sbuf[20];

    for (i = 0; i < 5; i++)   /* ???5??????????? */
    {
        sprintf(sbuf, "x%d:%d", i + 1, xy[i][0]);
        lcd_show_string(40, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
        sprintf(sbuf, "y%d:%d", i + 1, xy[i][1]);
        lcd_show_string(40 + 80, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
    }

    /* ???X/Y???????????? */
    lcd_fill(40, 160 + (i * 20), lcddev.width - 1, 16, WHITE);  /* ???????px,py??? */
    sprintf(sbuf, "px:%0.2f", px);
    sbuf[7] = 0; /* ????????? */
    lcd_show_string(40, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
    sprintf(sbuf, "py:%0.2f", py);
    sbuf[7] = 0; /* ????????? */
    lcd_show_string(40 + 80, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
}

/**
 * @brief       ?????????????
 *   @note      ???????????(???????????)
 *              ?????????x??/y?????????xfac/yfac???????????????(xc,yc)??4??????
 *              ??????: ????????AD????????????,??????0~4095.
 *                        ???????LCD?????????, ?????LCD?????????.
 *
 * @param       ??
 * @retval      ??
 */
void tp_adjust(void)
{
    uint16_t pxy[5][2];     /* ??????????? */
    uint8_t  cnt = 0;
    short s1, s2, s3, s4;   /* 4??????????? */
    double px, py;          /* X,Y?????????????,????????????????? */
    uint16_t outtime = 0;
    cnt = 0;

    lcd_clear(WHITE);       /* ???? */
    lcd_show_string(40, 40, 160, 100, 16, TP_REMIND_MSG_TBL, RED); /* ????????? */
    tp_draw_touch_point(20, 20, RED);   /* ????1 */
    tp_dev.sta = 0;         /* ??????????? */

    while (1)               /* ???????10???????????,???????? */
    {
        tp_dev.scan(1);     /* ??????????? */

        if ((tp_dev.sta & 0xc000) == TP_CATH_PRES)   /* ?????????????(????????????.) */
        {
            outtime = 0;
            tp_dev.sta &= ~TP_CATH_PRES;    /* ???????????????????. */

            pxy[cnt][0] = tp_dev.x[0];      /* ????X???????? */
            pxy[cnt][1] = tp_dev.y[0];      /* ????Y???????? */
            cnt++;

            switch (cnt)
            {
                case 1:
                    tp_draw_touch_point(20, 20, WHITE);                 /* ?????1 */
                    tp_draw_touch_point(lcddev.width - 20, 20, RED);    /* ????2 */
                    break;

                case 2:
                    tp_draw_touch_point(lcddev.width - 20, 20, WHITE);  /* ?????2 */
                    tp_draw_touch_point(20, lcddev.height - 20, RED);   /* ????3 */
                    break;

                case 3:
                    tp_draw_touch_point(20, lcddev.height - 20, WHITE); /* ?????3 */
                    tp_draw_touch_point(lcddev.width - 20, lcddev.height - 20, RED);    /* ????4 */
                    break;

                case 4:
                    lcd_clear(WHITE);   /* ???????????, ??????? */
                    tp_draw_touch_point(lcddev.width / 2, lcddev.height / 2, RED);  /* ????5 */
                    break;

                case 5:     /* ???5?????????? */
                    s1 = pxy[1][0] - pxy[0][0]; /* ??2??????1?????X????????????(AD?) */
                    s3 = pxy[3][0] - pxy[2][0]; /* ??4??????3?????X????????????(AD?) */
                    s2 = pxy[3][1] - pxy[1][1]; /* ??4??????2?????Y????????????(AD?) */
                    s4 = pxy[2][1] - pxy[0][1]; /* ??3??????1?????Y????????????(AD?) */

                    px = (double)s1 / s3;       /* X????????? */
                    py = (double)s2 / s4;       /* Y????????? */

                    if (px < 0) px = -px;       /* ?????????? */
                    if (py < 0) py = -py;       /* ?????????? */

                    if (px < 0.95 || px > 1.05 || py < 0.95 || py > 1.05 ||     /* ????????? */
                            abs(s1) > 4095 || abs(s2) > 4095 || abs(s3) > 4095 || abs(s4) > 4095 ||  /* ????????, ?????????? */
                            abs(s1) == 0 || abs(s2) == 0 || abs(s3) == 0 || abs(s4) == 0             /* ????????, ????0 */
                       )
                    {
                        cnt = 0;
                        tp_draw_touch_point(lcddev.width / 2, lcddev.height / 2, WHITE); /* ?????5 */
                        tp_draw_touch_point(20, 20, RED);   /* ???????1 */
                        tp_adjust_info_show(pxy, px, py);   /* ?????????,?????????? */
                        continue;
                    }

                    tp_dev.xfac = (float)(s1 + s3) / (2 * (lcddev.width - 40));
                    tp_dev.yfac = (float)(s2 + s4) / (2 * (lcddev.height - 40));

                    tp_dev.xc = pxy[4][0];      /* X??,???????????? */
                    tp_dev.yc = pxy[4][1];      /* Y??,???????????? */

                    lcd_clear(WHITE);   /* ???? */
                    lcd_show_string(35, 110, lcddev.width, lcddev.height, 16, "Touch Screen Adjust OK!", BLUE); /* ??????? */
                    delay_ms(1000);
                    tp_save_adjust_data();

                    lcd_clear(WHITE);   /* ???? */
                    return; /* ??????? */
            }
        }

        delay_ms(10);
        outtime++;

        if (outtime > 1000)
        {
            tp_get_adjust_data();
            break;
        }
    }

}

#else
void tp_adjust(void)
{
}
#endif

uint8_t tp_init(void)
{
#if defined(APP_TP_FT6336_CAP_ONLY) && (APP_TP_FT6336_CAP_ONLY != 0)
    tp_dev.touchtype = 0;
    tp_dev.touchtype |= lcddev.dir & 0X01;
    (void)ft5206_init();
    tp_dev.scan = ft5206_scan;
    tp_dev.touchtype |= 0X80;
    return 0;
#else
    GPIO_InitTypeDef gpio_init_struct;

    tp_dev.touchtype = 0;
    tp_dev.touchtype |= lcddev.dir & 0X01;

    if (lcddev.id == 0x7796)
    {
        if (gt9xxx_init() == 0)
        {
            tp_dev.scan = gt9xxx_scan;
            tp_dev.touchtype |= 0X80;
            return 0;
        }
    }

    if(lcddev.id == 0x9341)
    {
        (void)ft5206_init();
        tp_dev.scan = ft5206_scan;
        tp_dev.touchtype |= 0X80;
        return 0;
    }

    if (lcddev.id == 0X5510 || lcddev.id == 0X9806 || lcddev.id == 0X4342 || lcddev.id == 0X4384 || lcddev.id == 0X1018)
    {
        gt9xxx_init();
        tp_dev.scan = gt9xxx_scan;
        tp_dev.touchtype |= 0X80;
        return 0;
    }
    else if (lcddev.id == 0X1963 || lcddev.id == 0X7084 || lcddev.id == 0X7016)
    {
        if (!ft5206_init())
        {
            tp_dev.scan = ft5206_scan;
        }
        else
        {
            gt9xxx_init();
            tp_dev.scan = gt9xxx_scan;
        }
        tp_dev.touchtype |= 0X80;
        return 0;
    }
    else
    {
        T_PEN_GPIO_CLK_ENABLE();
        T_CS_GPIO_CLK_ENABLE();
        T_MISO_GPIO_CLK_ENABLE();
        T_MOSI_GPIO_CLK_ENABLE();
        T_CLK_GPIO_CLK_ENABLE();

        gpio_init_struct.Pin = T_PEN_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_INPUT;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(T_PEN_GPIO_PORT, &gpio_init_struct);

        tp_sda_output();

        gpio_init_struct.Pin = T_CLK_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(T_CLK_GPIO_PORT, &gpio_init_struct);

        gpio_init_struct.Pin = T_CS_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
        gpio_init_struct.Pull = GPIO_NOPULL;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(T_CS_GPIO_PORT, &gpio_init_struct);

        tp_bus_idle();

        tp_read_xy(&tp_dev.x[0], &tp_dev.y[0]);
        at24cxx_init();

        if(tp_get_adjust_data() == 0) {
            if(at24cxx_check() == 0) {
                lcd_clear(WHITE);
                tp_adjust();
                tp_save_adjust_data();
            }
        }

        tp_ensure_cal();
    }

    return 1;
#endif
}









