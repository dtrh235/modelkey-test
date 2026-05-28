
#include "stdlib.h"
#include "lcd.h"
#include "lcdfont.h"
#include "./SYSTEM/usart/usart.h"
#include "SYSTEM/delay/delay.h"


/* lcd_ex.c??????LCD????IC??????????????????,???lcd.c,??.c???
 * ????????????????,???lcd.c?????,???????include?????????.(?????
 * ??????????????.c???!!???????!)
 */
#include "./BSP/LCD/lcd_ex.c"


SPI_HandleTypeDef g_lcd_spi;

/* LCD?????????????? */
uint32_t g_point_color = 0xF800;    /* ??????? */
uint32_t g_back_color  = 0xFFFF;    /* ????? */

/* ????LCD??????? */
_lcd_dev lcddev;

static void lcd_write_ram_end(void);

static void lcd_cs_low(void)
{
    HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, GPIO_PIN_RESET);
}

static void lcd_cs_high(void)
{
    HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, GPIO_PIN_SET);
}

static void lcd_dc_cmd(void)
{
    HAL_GPIO_WritePin(LCD_DC_GPIO_PORT, LCD_DC_GPIO_PIN, GPIO_PIN_RESET);
}

static void lcd_dc_data(void)
{
    HAL_GPIO_WritePin(LCD_DC_GPIO_PORT, LCD_DC_GPIO_PIN, GPIO_PIN_SET);
}

static void lcd_wr_pixel565(uint16_t rgb)
{
    uint8_t b[2] = { (uint8_t)(rgb >> 8), (uint8_t)(rgb & 0xFF) };

    HAL_SPI_Transmit(&g_lcd_spi, b, 2, HAL_MAX_DELAY);
}

static void lcd_gpio_rst_dc_cs_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    LCD_RST_GPIO_CLK_ENABLE();
    LCD_DC_GPIO_CLK_ENABLE();
    LCD_CS_GPIO_CLK_ENABLE();

    gpio_init_struct.Pin = LCD_RST_GPIO_PIN | LCD_DC_GPIO_PIN | LCD_CS_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOE, &gpio_init_struct);

    HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_PORT, LCD_DC_GPIO_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_SET);
}

static void lcd_spi_pins_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    LCD_SPI_SCK_GPIO_CLK_ENABLE();

    gpio_init_struct.Pin = LCD_SPI_SCK_GPIO_PIN | LCD_SPI_MISO_GPIO_PIN | LCD_SPI_MOSI_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init_struct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);
}

static void lcd_spi_periph_init(void)
{
    g_lcd_spi.Instance = LCD_SPI;
    g_lcd_spi.Init.Mode = SPI_MODE_MASTER;
    g_lcd_spi.Init.Direction = SPI_DIRECTION_2LINES;
    g_lcd_spi.Init.DataSize = SPI_DATASIZE_8BIT;
    g_lcd_spi.Init.CLKPolarity = SPI_POLARITY_LOW;
    g_lcd_spi.Init.CLKPhase = SPI_PHASE_1EDGE;
    g_lcd_spi.Init.NSS = SPI_NSS_SOFT;
    g_lcd_spi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    g_lcd_spi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    g_lcd_spi.Init.TIMode = SPI_TIMODE_DISABLE;
    g_lcd_spi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    g_lcd_spi.Init.CRCPolynomial = 7;
    HAL_SPI_Init(&g_lcd_spi);
}

static void lcd_hard_reset(void)
{
    HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_RESET);
    delay_ms(10);
    HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_SET);
    delay_ms(120);
}

static void lcd_read_id_spi(void)
{
    uint8_t cmd = 0xD3;
    uint8_t rb[4];

    lcd_cs_low();
    lcd_dc_cmd();
    HAL_SPI_Transmit(&g_lcd_spi, &cmd, 1, HAL_MAX_DELAY);
    lcd_dc_data();
    HAL_SPI_Receive(&g_lcd_spi, rb, 4, HAL_MAX_DELAY);
    lcd_cs_high();

    lcddev.id = ((uint16_t)rb[2] << 8) | rb[3];
    if (lcddev.id != 0x9341)
    {
        lcddev.id = 0x9341;
    }
}

/**
 * @brief       LCD??????
 * @param       data: ??????????
 * @retval      ??
 */
void lcd_wr_data(volatile uint16_t data)
{
    uint8_t d = (uint8_t)data;

    lcd_cs_low();
    lcd_dc_data();
    HAL_SPI_Transmit(&g_lcd_spi, &d, 1, HAL_MAX_DELAY);
    lcd_cs_high();
}

/**
 * @brief       LCD??????????/???????
 * @param       regno: ????????/???
 * @retval      ??
 */
void lcd_wr_regno(volatile uint16_t regno)
{
    uint8_t r = (uint8_t)regno;

    lcd_cs_low();
    lcd_dc_cmd();
    HAL_SPI_Transmit(&g_lcd_spi, &r, 1, HAL_MAX_DELAY);
    lcd_cs_high();
}

/**
 * @brief       LCD???????
 * @param       regno:????????/???
 * @param       data:??????????
 * @retval      ??
 */
void lcd_write_reg(uint16_t regno, uint16_t data)
{
    uint8_t r = (uint8_t)regno;
    uint8_t d = (uint8_t)data;

    lcd_cs_low();
    lcd_dc_cmd();
    HAL_SPI_Transmit(&g_lcd_spi, &r, 1, HAL_MAX_DELAY);
    lcd_dc_data();
    HAL_SPI_Transmit(&g_lcd_spi, &d, 1, HAL_MAX_DELAY);
    lcd_cs_high();
}

/**
 * @brief       LCD???????,???????????mdk -O1?????????????????
 * @param       t:????????
 * @retval      ??
 */
static void lcd_opt_delay(uint32_t i)
{
    while (i--) ;
}

/**
 * @brief       LCD??????(SPI)
 * @param       ??
 * @retval      ???????????
 */
static uint16_t lcd_rd_data(void)
{
    uint8_t d = 0;

    lcd_opt_delay(2);
    HAL_SPI_Receive(&g_lcd_spi, &d, 1, HAL_MAX_DELAY);
    return d;
}

/**
 * @brief       ?????GRAM
 * @param       ??
 * @retval      ??
 */
void lcd_write_ram_prepare(void)
{
    uint8_t cmd = (uint8_t)lcddev.wramcmd;

    lcd_cs_low();
    lcd_dc_cmd();
    HAL_SPI_Transmit(&g_lcd_spi, &cmd, 1, HAL_MAX_DELAY);
    lcd_dc_data();
}

static void lcd_write_ram_end(void)
{
    lcd_cs_high();
}

/**
 * @brief       ?????????????
 * @param       x,y:????
 * @retval      ???????(32?????,???????LTDC)
 */
uint32_t lcd_read_point(uint16_t x, uint16_t y)
{
    uint16_t r = 0, g = 0, b = 0;

    if (x >= lcddev.width || y >= lcddev.height)
    {
        return 0;
    }

    lcd_set_cursor(x, y);

    lcd_cs_low();
    lcd_dc_cmd();
    {
        uint8_t c = 0x2E;
        HAL_SPI_Transmit(&g_lcd_spi, &c, 1, HAL_MAX_DELAY);
    }
    lcd_dc_data();

    r = lcd_rd_data();
    r = lcd_rd_data();
    b = lcd_rd_data();
    g = r & 0xFF;
    g <<= 8;
    lcd_cs_high();

    return (((r >> 11) << 11) | ((g >> 10) << 5) | (b >> 11));
}

/**
 * @brief       LCD???????
 * @param       ??
 * @retval      ??
 */
void lcd_display_on(void)
{
    lcd_wr_regno(0x29);
}

/**
 * @brief       LCD??????
 * @param       ??
 * @retval      ??
 */
void lcd_display_off(void)
{
    lcd_wr_regno(0x28);
}

/**
 * @brief       ???????????(??RGB??????)
 * @param       x,y: ????
 * @retval      ??
 */
void lcd_set_cursor(uint16_t x, uint16_t y)
{
    lcd_wr_regno(lcddev.setxcmd);
    lcd_wr_data(x >> 8);
    lcd_wr_data(x & 0xFF);
    lcd_wr_regno(lcddev.setycmd);
    lcd_wr_data(y >> 8);
    lcd_wr_data(y & 0xFF);
}

/**
 * @brief       ????LCD???????????(??RGB??????)
 *   @note      ILI9341 SPI: ????????L2R_U2D????
 * @param       dir:0~7,????8??????(????G???lcd.h)
 * @retval      ??
 */
void lcd_scan_dir(uint8_t dir)
{
    uint16_t regval = 0;
    uint16_t temp;

    if (lcddev.dir == 1)
    {
        switch (dir)
        {
            case 0: dir = 6; break;
            case 1: dir = 7; break;
            case 2: dir = 4; break;
            case 3: dir = 5; break;
            case 4: dir = 1; break;
            case 5: dir = 0; break;
            case 6: dir = 3; break;
            case 7: dir = 2; break;
        }
    }

    switch (dir)
    {
        case L2R_U2D:
            regval |= (0 << 7) | (0 << 6) | (0 << 5);
            break;
        case L2R_D2U:
            regval |= (1 << 7) | (0 << 6) | (0 << 5);
            break;
        case R2L_U2D:
            regval |= (0 << 7) | (1 << 6) | (0 << 5);
            break;
        case R2L_D2U:
            regval |= (1 << 7) | (1 << 6) | (0 << 5);
            break;
        case U2D_L2R:
            regval |= (0 << 7) | (0 << 6) | (1 << 5);
            break;
        case U2D_R2L:
            regval |= (0 << 7) | (1 << 6) | (1 << 5);
            break;
        case D2U_L2R:
            regval |= (1 << 7) | (0 << 6) | (1 << 5);
            break;
        case D2U_R2L:
            regval |= (1 << 7) | (1 << 6) | (1 << 5);
            break;
    }

    regval |= 0x08;
    lcd_write_reg(0x36, regval);

    if (regval & 0x20)
    {
        if (lcddev.width < lcddev.height)
        {
            temp = lcddev.width;
            lcddev.width = lcddev.height;
            lcddev.height = temp;
        }
    }
    else
    {
        if (lcddev.width > lcddev.height)
        {
            temp = lcddev.width;
            lcddev.width = lcddev.height;
            lcddev.height = temp;
        }
    }

    lcd_wr_regno(lcddev.setxcmd);
    lcd_wr_data(0);
    lcd_wr_data(0);
    lcd_wr_data((lcddev.width - 1) >> 8);
    lcd_wr_data((lcddev.width - 1) & 0xFF);
    lcd_wr_regno(lcddev.setycmd);
    lcd_wr_data(0);
    lcd_wr_data(0);
    lcd_wr_data((lcddev.height - 1) >> 8);
    lcd_wr_data((lcddev.height - 1) & 0xFF);
}

/**
 * @brief       ????
 * @param       x,y: ????
 * @param       color: ??????(32?????,???????LTDC)
 * @retval      ??
 */
void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color)
{
    lcd_set_cursor(x, y);
    lcd_write_ram_prepare();
    lcd_wr_pixel565((uint16_t)color);
    lcd_write_ram_end();
}

/**
 * @brief       SSD1963????????????????
 * @param       pwm: ??????,0~100.??????.
 * @retval      ??
 */
void lcd_ssd_backlight_set(uint8_t pwm)
{
    (void)pwm;
}

/**
 * @brief       ????LCD???????
 * @param       dir:0,????; 1,????
 * @retval      ??
 */
void lcd_display_dir(uint8_t dir)
{
    lcddev.dir = dir;

    if (dir == 0)
    {
        lcddev.width = 240;
        lcddev.height = 320;
        lcddev.wramcmd = 0x2C;
        lcddev.setxcmd = 0x2A;
        lcddev.setycmd = 0x2B;
    }
    else
    {
        lcddev.width = 320;
        lcddev.height = 240;
        lcddev.wramcmd = 0x2C;
        lcddev.setxcmd = 0x2A;
        lcddev.setycmd = 0x2B;
    }

    lcd_scan_dir(DFT_SCAN_DIR);
}

/**
 * @brief       ???????(??RGB??????), ?????????????????????????(sx,sy).
 * @param       sx,sy:???????????(?????)
 * @param       width,height:??????????,???????0!!
 * @retval      ??
 */
void lcd_set_window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    uint16_t twidth = sx + width - 1;
    uint16_t theight = sy + height - 1;

    lcd_wr_regno(lcddev.setxcmd);
    lcd_wr_data(sx >> 8);
    lcd_wr_data(sx & 0xFF);
    lcd_wr_data(twidth >> 8);
    lcd_wr_data(twidth & 0xFF);
    lcd_wr_regno(lcddev.setycmd);
    lcd_wr_data(sy >> 8);
    lcd_wr_data(sy & 0xFF);
    lcd_wr_data(theight >> 8);
    lcd_wr_data(theight & 0xFF);
}

/**
 * @brief       ?????LCD (SPI1 + ILI9341)
 * @param       ??
 * @retval      ??
 */
void lcd_init(void)
{
    lcd_gpio_rst_dc_cs_init();
    lcd_spi_pins_init();
    LCD_SPI_CLK_ENABLE();
    lcd_spi_periph_init();
    lcd_hard_reset();
    lcd_read_id_spi();

    lcd_ex_ili9341_reginit();

    lcd_display_dir(0);
    LCD_BL(1);
    lcd_clear(WHITE);
}

/**
 * @brief       ????????
 * @param       color: ??????????
 * @retval      ??
 */
void lcd_clear(uint16_t color)
{
    uint32_t index = 0;
    uint32_t totalpoint = (uint32_t)lcddev.width * (uint32_t)lcddev.height;

    lcd_set_cursor(0, 0);
    lcd_write_ram_prepare();

    for (index = 0; index < totalpoint; index++)
    {
        lcd_wr_pixel565(color);
    }

    lcd_write_ram_end();
}

/**
 * @brief       ???????????????????
 * @param       (sx,sy),(ex,ey):?????????????,????????:(ex - sx + 1) * (ey - sy + 1)
 * @param       color:  ????????(32?????,???????LTDC)
 * @retval      ??
 */
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    uint16_t i, j;
    uint16_t xlen = (uint16_t)(ex - sx + 1);
    uint16_t c = (uint16_t)color;

    for (i = sy; i <= ey; i++)
    {
        lcd_set_cursor(sx, i);
        lcd_write_ram_prepare();

        for (j = 0; j < xlen; j++)
        {
            lcd_wr_pixel565(c);
        }

        lcd_write_ram_end();
    }
}

/**
 * @brief       ??????????????????????
 * @param       (sx,sy),(ex,ey):?????????????,????????:(ex - sx + 1) * (ey - sy + 1)
 * @param       color: ????????????????
 * @retval      ??
 */
void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint16_t height, width;
    uint16_t i, j;

    width = (uint16_t)(ex - sx + 1);
    height = (uint16_t)(ey - sy + 1);

    for (i = 0; i < height; i++)
    {
        lcd_set_cursor(sx, (uint16_t)(sy + i));
        lcd_write_ram_prepare();
        for (j = 0; j < width; j++)
        {
            lcd_wr_pixel565(color[i * width + j]);
        }
        lcd_write_ram_end();
    }
}

/**
 * @brief       ????
 * @param       x1,y1: ???????
 * @param       x2,y2: ???????
 * @param       color: ??????
 * @retval      ??
 */
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;

    delta_x = (int)x2 - (int)x1;
    delta_y = (int)y2 - (int)y1;
    row = (int)x1;
    col = (int)y1;

    if (delta_x > 0)
    {
        incx = 1;
    }
    else if (delta_x == 0)
    {
        incx = 0;
    }
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }

    if (delta_y > 0)
    {
        incy = 1;
    }
    else if (delta_y == 0)
    {
        incy = 0;
    }
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }

    if (delta_x > delta_y)
    {
        distance = delta_x;
    }
    else
    {
        distance = delta_y;
    }

    for (t = 0; t <= (uint16_t)(distance + 1); t++)
    {
        lcd_draw_point((uint16_t)row, (uint16_t)col, color);
        xerr += delta_x;
        yerr += delta_y;

        if (xerr > distance)
        {
            xerr -= distance;
            row += incx;
        }

        if (yerr > distance)
        {
            yerr -= distance;
            col += incy;
        }
    }
}

/**
 * @brief       ??????
 * @param       x,y   : ???????
 * @param       len   : ?????
 * @param       color : ?????????
 * @retval      ??
 */
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    if ((len == 0) || (x > lcddev.width) || (y > lcddev.height))
    {
        return;
    }

    lcd_fill(x, y, (uint16_t)(x + len - 1), y, color);
}

/**
 * @brief       ??????
 * @param       x1,y1: ???????
 * @param       x2,y2: ???????
 * @param       color: ?????????
 * @retval      ??
 */
void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    lcd_draw_line(x1, y1, x2, y1, color);
    lcd_draw_line(x1, y1, x1, y2, color);
    lcd_draw_line(x1, y2, x2, y2, color);
    lcd_draw_line(x2, y1, x2, y2, color);
}

/**
 * @brief       ???
 * @param       x0,y0 : ?????????
 * @param       r     : ??
 * @param       color : ??????
 * @retval      ??
 */
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int a, b;
    int di;

    a = 0;
    b = r;
    di = 3 - (r << 1);

    while (a <= b)
    {
        lcd_draw_point((uint16_t)(x0 + a), (uint16_t)(y0 - b), color);
        lcd_draw_point((uint16_t)(x0 + b), (uint16_t)(y0 - a), color);
        lcd_draw_point((uint16_t)(x0 + b), (uint16_t)(y0 + a), color);
        lcd_draw_point((uint16_t)(x0 + a), (uint16_t)(y0 + b), color);
        lcd_draw_point((uint16_t)(x0 - a), (uint16_t)(y0 + b), color);
        lcd_draw_point((uint16_t)(x0 - b), (uint16_t)(y0 + a), color);
        lcd_draw_point((uint16_t)(x0 - a), (uint16_t)(y0 - b), color);
        lcd_draw_point((uint16_t)(x0 - b), (uint16_t)(y0 - a), color);
        a++;

        if (di < 0)
        {
            di += 4 * a + 6;
        }
        else
        {
            di += 10 + 4 * (a - b);
            b--;
        }
    }
}

/**
 * @brief       ???????
 * @param       x,y  : ?????????
 * @param       r    : ??
 * @param       color: ??????
 * @retval      ??
 */
void lcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color)
{
    uint32_t i;
    uint32_t imax = ((uint32_t)r * 707) / 1000 + 1;
    uint32_t sqmax = (uint32_t)r * (uint32_t)r + (uint32_t)r / 2;
    uint32_t xr = r;

    lcd_draw_hline((uint16_t)(x - r), y, (uint16_t)(2 * r), color);

    for (i = 1; i <= imax; i++)
    {
        if ((i * i + xr * xr) > sqmax)
        {
            if (xr > imax)
            {
                lcd_draw_hline((uint16_t)(x - i + 1), (uint16_t)(y + xr), (uint16_t)(2 * (i - 1)), color);
                lcd_draw_hline((uint16_t)(x - i + 1), (uint16_t)(y - xr), (uint16_t)(2 * (i - 1)), color);
            }

            xr--;
        }

        lcd_draw_hline((uint16_t)(x - xr), (uint16_t)(y + i), (uint16_t)(2 * xr), color);
        lcd_draw_hline((uint16_t)(x - xr), (uint16_t)(y - i), (uint16_t)(2 * xr), color);
    }
}

/**
 * @brief       ??????????????????
 * @param       x,y  : ????
 * @param       chr  : ?????????:" "--->"~"
 * @param       size : ??????? 12/16/24/32
 * @param       mode : ??????(1); ???????(0);
 * @param       color : ????????;
 * @retval      ??
 */
void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t temp, t1, t;
    uint16_t y0 = y;
    uint8_t csize = 0;
    uint8_t *pfont = 0;

    csize = (uint8_t)((size / 8 + ((size % 8) ? 1 : 0)) * (size / 2));
    chr = chr - ' ';

    switch (size)
    {
        case 12:
            pfont = (uint8_t *)asc2_1206[(uint8_t)chr];
            break;
        case 16:
            pfont = (uint8_t *)asc2_1608[(uint8_t)chr];
            break;
        case 24:
            pfont = (uint8_t *)asc2_2412[(uint8_t)chr];
            break;
        case 32:
            pfont = (uint8_t *)asc2_3216[(uint8_t)chr];
            break;
        default:
            return;
    }

    for (t = 0; t < csize; t++)
    {
        temp = pfont[t];

        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)
            {
                lcd_draw_point(x, y, color);
            }
            else if (mode == 0)
            {
                lcd_draw_point(x, y, g_back_color);
            }

            temp <<= 1;
            y++;

            if (y >= lcddev.height)
            {
                return;
            }

            if ((y - y0) == size)
            {
                y = y0;
                x++;

                if (x >= lcddev.width)
                {
                    return;
                }

                break;
            }
        }
    }
}

static uint32_t lcd_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;

    while (n--)
    {
        result *= m;
    }

    return result;
}

void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (uint8_t)((num / lcd_pow(10, (uint8_t)(len - t - 1))) % 10);

        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                lcd_show_char((uint16_t)(x + (size / 2) * t), y, ' ', size, 0, color);
                continue;
            }
            else
            {
                enshow = 1;
            }
        }

        lcd_show_char((uint16_t)(x + (size / 2) * t), y, (char)(temp + '0'), size, 0, color);
    }
}

void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (uint8_t)((num / lcd_pow(10, (uint8_t)(len - t - 1))) % 10);

        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                if (mode & 0x80)
                {
                    lcd_show_char((uint16_t)(x + (size / 2) * t), y, '0', size, mode & 0x01, color);
                }
                else
                {
                    lcd_show_char((uint16_t)(x + (size / 2) * t), y, ' ', size, mode & 0x01, color);
                }

                continue;
            }
            else
            {
                enshow = 1;
            }
        }

        lcd_show_char((uint16_t)(x + (size / 2) * t), y, (char)(temp + '0'), size, mode & 0x01, color);
    }
}

void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color)
{
    uint16_t x0 = x;

    width = (uint16_t)(width + x);
    height = (uint16_t)(height + y);

    while ((*p <= '~') && (*p >= ' '))
    {
        if (x >= width)
        {
            x = x0;
            y = (uint16_t)(y + size);
        }

        if (y >= height)
        {
            break;
        }

        lcd_show_char(x, y, *p, size, 0, color);
        x = (uint16_t)(x + size / 2);
        p++;
    }
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    (void)hspi;
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
    (void)hspi;
}
