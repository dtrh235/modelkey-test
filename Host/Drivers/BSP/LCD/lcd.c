
#include "stdlib.h"
#include "lcd.h"
#include "lcdfont.h"
#include "./SYSTEM/usart/usart.h"
#include "SYSTEM/delay/delay.h"
#include "app_config.h"
#include <stdio.h>
#include "./BSP/W25Q16/bsp_w25q16.h"


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

#if (APP_BOOT_STAGE_LOG != 0)
static void lcd_boot_diag_log(const char *stage)
{
    char buf[120];
    uint32_t pa5_moder;
    uint32_t pa5_afr;
    uint32_t cr1;

    pa5_moder = (GPIOA->MODER >> 10) & 3u;
    pa5_afr = (GPIOA->AFR[0] >> 20) & 0xFu;
    cr1 = READ_REG(SPI1->CR1);
    snprintf(buf, sizeof(buf),
             "[LCD] %s PA5 MODER=%lu AFR=%lu idle=%u SPI1_CR1=0x%lX SPE=%u CPOL=%u\r\n",
             (stage != NULL) ? stage : "?",
             (unsigned long)pa5_moder, (unsigned long)pa5_afr,
             (unsigned)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5),
             (unsigned long)cr1,
             (unsigned)((cr1 & SPI_CR1_SPE) != 0u),
             (unsigned)((cr1 & SPI_CR1_CPOL) != 0u));
    usart_debug_tx_str(buf);
    snprintf(buf, sizeof(buf),
             "[LCD] pins PA7=%u PE7_CS=%u PE6_DC=%u id=0x%04X w=%u h=%u\r\n",
             (unsigned)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7),
             (unsigned)HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_7),
             (unsigned)HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_6),
             (unsigned)lcddev.id,
             (unsigned)lcddev.width,
             (unsigned)lcddev.height);
    usart_debug_tx_str(buf);
}
#else
#define lcd_boot_diag_log(stage) ((void)0)
#endif

static void lcd_cs_low(void)
{
    HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, GPIO_PIN_RESET);
}

static void lcd_cs_high(void)
{
    HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, GPIO_PIN_SET);
}

/* CS 在整个 init/开窗/刷屏期间保持低（afiskon/stm32-ili9341 做法；逐字节拉高 CS 会白屏） */
static uint8_t s_lcd_cs_hold;

static void lcd_cs_release(void)
{
    if (s_lcd_cs_hold == 0u) {
        lcd_cs_high();
    }
}

void lcd_spi_cs_hold_begin(void)
{
    if (s_lcd_cs_hold == 0u) {
        lcd_cs_low();
    }
    s_lcd_cs_hold++;
}

void lcd_spi_cs_hold_end(void)
{
    if (s_lcd_cs_hold > 0u) {
        s_lcd_cs_hold--;
        if (s_lcd_cs_hold == 0u) {
            lcd_cs_high();
        }
    }
}

static void lcd_spi_wait_idle(void)
{
    while ((SPI1->SR & SPI_SR_BSY) != 0u) {
    }
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
    lcd_spi_wait_idle();
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
    LCD_SPI_MISO_GPIO_CLK_ENABLE();

    gpio_init_struct.Pin = LCD_SPI_SCK_GPIO_PIN | LCD_SPI_MISO_GPIO_PIN | LCD_SPI_MOSI_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init_struct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);
}

static uint32_t lcd_spi_prescaler_to_hal(uint32_t div)
{
    switch (div) {
    case 2u:   return SPI_BAUDRATEPRESCALER_2;
    case 4u:   return SPI_BAUDRATEPRESCALER_4;
    case 8u:   return SPI_BAUDRATEPRESCALER_8;
    case 16u:  return SPI_BAUDRATEPRESCALER_16;
    case 32u:  return SPI_BAUDRATEPRESCALER_32;
    case 64u:  return SPI_BAUDRATEPRESCALER_64;
    case 256u: return SPI_BAUDRATEPRESCALER_256;
    case 128u:
    default:   return SPI_BAUDRATEPRESCALER_128;
    }
}

static uint32_t lcd_spi_effective_prescaler(void)
{
    return (uint32_t)APP_LCD_SPI_PRESCALER;
}

static void lcd_spi_periph_init(void)
{
    uint32_t div = lcd_spi_effective_prescaler();

    g_lcd_spi.Instance = LCD_SPI;
    g_lcd_spi.Init.Mode = SPI_MODE_MASTER;
    g_lcd_spi.Init.Direction = SPI_DIRECTION_2LINES;
    g_lcd_spi.Init.DataSize = SPI_DATASIZE_8BIT;
    g_lcd_spi.Init.CLKPolarity = SPI_POLARITY_LOW;
    g_lcd_spi.Init.CLKPhase = SPI_PHASE_1EDGE;
    g_lcd_spi.Init.NSS = SPI_NSS_SOFT;
    g_lcd_spi.Init.BaudRatePrescaler = lcd_spi_prescaler_to_hal(div);
    g_lcd_spi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    g_lcd_spi.Init.TIMode = SPI_TIMODE_DISABLE;
    g_lcd_spi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    g_lcd_spi.Init.CRCPolynomial = 7;
    HAL_SPI_Init(&g_lcd_spi);

#if (APP_BOOT_STAGE_LOG != 0)
    {
        char buf[80];
        uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
        uint32_t sck_hz = (div != 0u) ? (pclk2 / div) : 0u;
        snprintf(buf, sizeof(buf),
                 "[LCD] SPI1 prescaler=/%lu PCLK2=%luHz SCK~%luHz\r\n",
                 (unsigned long)div, (unsigned long)pclk2, (unsigned long)sck_hz);
        usart_debug_tx_str(buf);
    }
#endif
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
    lcd_spi_wait_idle();
    lcd_cs_high();

    lcddev.id = ((uint16_t)rb[2] << 8) | rb[3];
#if (APP_BOOT_STAGE_LOG != 0)
    {
        char buf[96];
        snprintf(buf, sizeof(buf),
                 "[LCD] read_id D3 raw %02X %02X %02X %02X -> 0x%04X (force ILI9341)\r\n",
                 rb[0], rb[1], rb[2], rb[3], (unsigned)lcddev.id);
        usart_debug_tx_str(buf);
    }
#endif
    lcddev.id = 0x9341u;
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
    lcd_spi_wait_idle();
    lcd_cs_release();
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
    lcd_spi_wait_idle();
    lcd_cs_release();
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
    lcd_spi_wait_idle();
    lcd_cs_release();
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
    lcd_cs_release();
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
    bsp_w25q16_end_session();

    lcd_gpio_rst_dc_cs_init();
    lcd_spi_pins_init();
    LCD_SPI_CLK_ENABLE();
    lcd_spi_periph_init();
    lcd_boot_diag_log("after_spi_init");
    lcd_hard_reset();
    lcd_read_id_spi();
    lcd_boot_diag_log("after_read_id");

#if (APP_LCD_FORCE_ST7789 != 0)
    lcd_ex_st7789_reginit();
#if (APP_BOOT_STAGE_LOG != 0)
    usart_debug_tx_str("[LCD] panel init: ST7789 (forced)\r\n");
#endif
#elif (APP_LCD_ID_UNKNOWN_USE_ST7789 != 0)
    lcd_ex_st7789_reginit();
#if (APP_BOOT_STAGE_LOG != 0)
    usart_debug_tx_str("[LCD] panel init: ST7789\r\n");
#endif
#else
    lcd_ex_ili9341_reginit();
#if (APP_BOOT_STAGE_LOG != 0)
    usart_debug_tx_str("[LCD] panel init: ILI9341\r\n");
#endif
#endif

    lcd_display_dir(0);

    LCD_BL(1);
#if (APP_LCD_HW_TEST == 0) && (APP_LCD_SKIP_BOOT_CLEAR == 0)
    lcd_clear(WHITE);
#endif
    lcd_boot_diag_log("after_clear");
}

#if (APP_LCD_HW_TEST != 0)
void lcd_panel_hw_selftest(void)
{
    uint16_t w = lcddev.width;
    uint16_t h = lcddev.height;

    if (w == 0u || h == 0u)
    {
        w = 240u;
        h = 320u;
    }

    usart_debug_tx_str("[LCD] HW test BLACK full screen 3s\r\n");
    lcd_fill(0u, 0u, (uint16_t)(w - 1u), (uint16_t)(h - 1u), BLACK);
    delay_ms(3000);

    usart_debug_tx_str("[LCD] HW test RED full screen 3s\r\n");
    lcd_fill(0u, 0u, (uint16_t)(w - 1u), (uint16_t)(h - 1u), RED);
    delay_ms(3000);

    usart_debug_tx_str("[LCD] HW test GREEN full screen 3s\r\n");
    lcd_fill(0u, 0u, (uint16_t)(w - 1u), (uint16_t)(h - 1u), GREEN);
    delay_ms(3000);

    usart_debug_tx_str("[LCD] HW test BLUE full screen 3s\r\n");
    lcd_fill(0u, 0u, (uint16_t)(w - 1u), (uint16_t)(h - 1u), BLUE);
    delay_ms(3000);

    usart_debug_tx_str("[LCD] HW test done -> LVGL\r\n");
}
#else
void lcd_panel_hw_selftest(void)
{
}
#endif

/**
 * @brief       ????????
 * @param       color: ??????????
 * @retval      ??
 */
void lcd_clear(uint16_t color)
{
    lcd_fill(0u, 0u, (uint16_t)(lcddev.width - 1u), (uint16_t)(lcddev.height - 1u), color);
}

void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    uint16_t i;
    uint16_t xlen = (uint16_t)(ex - sx + 1u);
    uint16_t c = (uint16_t)color;
    static uint8_t s_row[240u * 2u];
    uint32_t j;
    uint8_t hi = (uint8_t)(c >> 8);
    uint8_t lo = (uint8_t)(c & 0xFFu);

    if(xlen > 240u) {
        return;
    }
    for(j = 0u; j < xlen; j++) {
        s_row[j * 2u] = hi;
        s_row[j * 2u + 1u] = lo;
    }

    for(i = sy; i <= ey; i++) {
        lcd_spi_cs_hold_begin();
        lcd_set_cursor(sx, i);
        lcd_write_ram_prepare();
        HAL_SPI_Transmit(&g_lcd_spi, s_row, (uint16_t)(xlen * 2u), HAL_MAX_DELAY);
        lcd_spi_wait_idle();
        lcd_write_ram_end();
        lcd_spi_cs_hold_end();
    }
}

void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint16_t height;
    uint16_t width;
    uint16_t i;
    uint16_t j;
    static uint8_t s_row[240u * 2u];

    if(color == NULL) {
        return;
    }

    width = (uint16_t)(ex - sx + 1u);
    height = (uint16_t)(ey - sy + 1u);
    if(width > 240u) {
        return;
    }

    for(i = 0u; i < height; i++) {
        for(j = 0u; j < width; j++) {
            uint16_t pix = color[(uint32_t)i * width + j];
            s_row[j * 2u] = (uint8_t)(pix >> 8);
            s_row[j * 2u + 1u] = (uint8_t)(pix & 0xFFu);
        }
        lcd_spi_cs_hold_begin();
        lcd_set_cursor(sx, (uint16_t)(sy + i));
        lcd_write_ram_prepare();
        HAL_SPI_Transmit(&g_lcd_spi, s_row, (uint16_t)(width * 2u), HAL_MAX_DELAY);
        lcd_spi_wait_idle();
        lcd_write_ram_end();
        lcd_spi_cs_hold_end();
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
    if(hspi != NULL && hspi->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
    }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
    (void)hspi;
}
