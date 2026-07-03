#include "bsp_w25q16.h"

#include <string.h>
#include <stdio.h>

#include "app_config.h"
#include "SYSTEM/delay/delay.h"
#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "semphr.h"
#endif
#if (APP_W25Q_DEBUG != 0)
#include "SYSTEM/usart/usart.h"
#define W25Q_LOG(fmt, ...) do { \
        char _wq[140]; \
        int _wn = snprintf(_wq, sizeof(_wq) - 3, "[W25Q] " fmt, ##__VA_ARGS__); \
        if(_wn < 0) _wn = 0; \
        if((size_t)_wn > sizeof(_wq) - 3) _wn = (int)(sizeof(_wq) - 3); \
        _wq[_wn++] = '\r'; \
        _wq[_wn++] = '\n'; \
        _wq[_wn] = '\0'; \
        usart_debug_tx_str(_wq); \
    } while(0)
#else
#define W25Q_LOG(fmt, ...) ((void)0)
#endif

/*
 * Winbond W25Q16 — 软件模拟 SPI（Mode0），PE0=CS PB3=SCK PB5=MOSI PB4=MISO
 * 网表 PCB4_5: F_CS -> U27.97(PE0)；PB3/4/5 与 JTAG 复用，Debug 选 SW。
 */

#define W25Q_CS_PORT  GPIOE
#define W25Q_CS_PIN   GPIO_PIN_0
#define W25Q_SCK_PORT GPIOB
#define W25Q_SCK_PIN  GPIO_PIN_3
#define W25Q_MISO_PORT GPIOB
#define W25Q_MISO_PIN GPIO_PIN_4
#define W25Q_MOSI_PORT GPIOB
#define W25Q_MOSI_PIN GPIO_PIN_5

#define CMD_WRITE_ENABLE    0x06u
#define CMD_READ_DATA       0x03u
#define CMD_PAGE_PROGRAM    0x02u
#define CMD_READ_STATUS1    0x05u
#define CMD_SECTOR_ERASE_4K 0x20u
#define CMD_READ_JEDEC_ID   0x9Fu

#define W25Q_BUSY_WAIT_MS 5000u

#ifndef W25Q_SOFT_SPI_DELAY_US
#define W25Q_SOFT_SPI_DELAY_US  2u
#endif

static uint8_t s_w25q_ready;
#if (APP_USE_FREERTOS == 1)
static SemaphoreHandle_t s_w25q_mtx;
#endif

void bsp_w25q16_lock(void)
{
#if (APP_USE_FREERTOS == 1)
    if(s_w25q_mtx == NULL) {
        s_w25q_mtx = xSemaphoreCreateRecursiveMutex();
    }
    if(s_w25q_mtx != NULL) {
        (void)xSemaphoreTakeRecursive(s_w25q_mtx, portMAX_DELAY);
    }
#else
    /* bare-metal: single-threaded */
#endif
}

void bsp_w25q16_unlock(void)
{
#if (APP_USE_FREERTOS == 1)
    if(s_w25q_mtx != NULL) {
        (void)xSemaphoreGiveRecursive(s_w25q_mtx);
    }
#endif
}

static void w25q_delay_pins(void)
{
    if(W25Q_SOFT_SPI_DELAY_US != 0u) {
        delay_us(W25Q_SOFT_SPI_DELAY_US);
    }
}

static uint8_t w25q_soft_xfer_byte(uint8_t tx)
{
    uint8_t rx = 0u;
    int i;

    for(i = 7; i >= 0; i--) {
        HAL_GPIO_WritePin(W25Q_MOSI_PORT, W25Q_MOSI_PIN,
                          (tx & (1u << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        w25q_delay_pins();

        HAL_GPIO_WritePin(W25Q_SCK_PORT, W25Q_SCK_PIN, GPIO_PIN_SET);
        w25q_delay_pins();

        if(HAL_GPIO_ReadPin(W25Q_MISO_PORT, W25Q_MISO_PIN) == GPIO_PIN_SET) {
            rx = (uint8_t)(rx | (1u << i));
        }

        HAL_GPIO_WritePin(W25Q_SCK_PORT, W25Q_SCK_PIN, GPIO_PIN_RESET);
        w25q_delay_pins();
    }
    return rx;
}

static void w25q_soft_send(const uint8_t *buf, size_t len)
{
    size_t i;
    for(i = 0u; i < len; i++) {
        (void)w25q_soft_xfer_byte(buf[i]);
    }
}

static void w25q_cs_low(void)
{
    HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
    w25q_delay_pins();
}

static void w25q_cs_high(void)
{
    w25q_delay_pins();
    HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);
}

static uint8_t w25q_read_status1(void)
{
    uint8_t cmd = CMD_READ_STATUS1;

    w25q_cs_low();
    (void)w25q_soft_xfer_byte(cmd);
    uint8_t st = w25q_soft_xfer_byte(0xFFu);
    w25q_cs_high();
    return st;
}

static bool w25q_wait_not_busy(void)
{
    uint32_t t0 = HAL_GetTick();
    while((w25q_read_status1() & 0x01u) != 0u) {
        if((HAL_GetTick() - t0) > W25Q_BUSY_WAIT_MS) {
            W25Q_LOG("wait busy timeout, sr1=0x%02X", w25q_read_status1());
            return false;
        }
    }
    return true;
}

static bool w25q_write_enable(void)
{
    uint8_t cmd = CMD_WRITE_ENABLE;
    w25q_cs_low();
    (void)w25q_soft_xfer_byte(cmd);
    w25q_cs_high();
    return true;
}

static void w25q_gpio_deinit_lines(void)
{
    GPIO_InitTypeDef io = {0};

    HAL_GPIO_WritePin(W25Q_SCK_PORT, W25Q_SCK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(W25Q_MOSI_PORT, W25Q_MOSI_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

    io.Mode = GPIO_MODE_INPUT;
    io.Pull = GPIO_NOPULL;
    io.Speed = GPIO_SPEED_FREQ_LOW;
    io.Pin = W25Q_SCK_PIN | W25Q_MISO_PIN | W25Q_MOSI_PIN;
    HAL_GPIO_Init(GPIOB, &io);
}

static uint8_t w25q_jedec_plausible(const uint8_t id[3])
{
    if(id[0] == 0xFFu && id[1] == 0xFFu && id[2] == 0xFFu) return 0u;
    if(id[0] == 0u && id[1] == 0u && id[2] == 0u) return 0u;
    switch(id[0]) {
    case 0xEFu:
    case 0xC8u:
    case 0xC2u:
    case 0x9Du:
    case 0x0Bu:
    case 0x68u:
    case 0x1Fu:
    case 0x85u:
        return 1u;
    default:
        return 0u;
    }
}

static void w25q_read_jedec_raw(uint8_t id[3])
{
    uint8_t cmd = CMD_READ_JEDEC_ID;

    HAL_GPIO_WritePin(W25Q_SCK_PORT, W25Q_SCK_PIN, GPIO_PIN_RESET);
    w25q_cs_low();
    (void)w25q_soft_xfer_byte(cmd);
    id[0] = w25q_soft_xfer_byte(0xFFu);
    id[1] = w25q_soft_xfer_byte(0xFFu);
    id[2] = w25q_soft_xfer_byte(0xFFu);
    w25q_cs_high();
}

static void w25q_release_jtag_pins(void)
{
    /*
     * PB3/PB4 与 JTAG 复用。Keil Debug 须选 SW（勿选 JTAG），
     * 否则 PB3/PB4 可能无法作为 GPIO 驱动 W25Q。
     */
    __HAL_RCC_SYSCFG_CLK_ENABLE();
}

static void w25q_gpio_soft_spi_setup(void)
{
    GPIO_InitTypeDef gpio = {0};

    w25q_release_jtag_pins();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(W25Q_SCK_PORT, W25Q_SCK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(W25Q_MOSI_PORT, W25Q_MOSI_PIN, GPIO_PIN_RESET);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Pin = W25Q_CS_PIN;
    HAL_GPIO_Init(W25Q_CS_PORT, &gpio);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Pin = W25Q_SCK_PIN | W25Q_MOSI_PIN;
    HAL_GPIO_Init(W25Q_SCK_PORT, &gpio);

    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin = W25Q_MISO_PIN;
    HAL_GPIO_Init(W25Q_MISO_PORT, &gpio);

    W25Q_LOG("soft-SPI PE0=CS PB3/4/5");
}

bool bsp_w25q16_read(uint32_t addr, uint8_t *buf, size_t len)
{
    uint8_t hdr[4];
    bool ok;

    bsp_w25q16_lock();
    if(s_w25q_ready == 0u || buf == NULL || len == 0u) {
        bsp_w25q16_unlock();
        return false;
    }
    if(addr + (uint32_t)len > 0x00200000u) {
        bsp_w25q16_unlock();
        return false;
    }

    hdr[0] = CMD_READ_DATA;
    hdr[1] = (uint8_t)((addr >> 16) & 0xFFu);
    hdr[2] = (uint8_t)((addr >> 8) & 0xFFu);
    hdr[3] = (uint8_t)(addr & 0xFFu);

    w25q_cs_low();
    w25q_soft_send(hdr, sizeof(hdr));
    {
        size_t i;
        for(i = 0u; i < len; i++) {
            buf[i] = w25q_soft_xfer_byte(0xFFu);
        }
    }
    w25q_cs_high();
    ok = true;
    bsp_w25q16_unlock();
    return ok;
}

bool bsp_w25q16_write_page(uint32_t addr, const uint8_t *buf, size_t len)
{
    size_t i;
    bool ok;

    bsp_w25q16_lock();
    if(s_w25q_ready == 0u || buf == NULL || len == 0u) {
        bsp_w25q16_unlock();
        return false;
    }
    if(((addr & 0xFFu) + len) > 256u) {
        bsp_w25q16_unlock();
        return false;
    }

    if(!w25q_write_enable()) {
        bsp_w25q16_unlock();
        return false;
    }
    if(!w25q_wait_not_busy()) {
        bsp_w25q16_unlock();
        return false;
    }

    {
        uint8_t hdr[4];
        hdr[0] = CMD_PAGE_PROGRAM;
        hdr[1] = (uint8_t)((addr >> 16) & 0xFFu);
        hdr[2] = (uint8_t)((addr >> 8) & 0xFFu);
        hdr[3] = (uint8_t)(addr & 0xFFu);
        w25q_cs_low();
        w25q_soft_send(hdr, sizeof(hdr));
        for(i = 0u; i < len; i++) {
            (void)w25q_soft_xfer_byte(buf[i]);
        }
        w25q_cs_high();
    }

    ok = w25q_wait_not_busy();
    bsp_w25q16_unlock();
    return ok;
}

bool bsp_w25q16_erase_sector_4k(uint32_t addr)
{
    uint8_t cmd[4];
    bool ok;

    bsp_w25q16_lock();
    if(s_w25q_ready == 0u) {
        bsp_w25q16_unlock();
        return false;
    }
    addr &= 0xFFFFF000u;

    if(!w25q_write_enable()) {
        bsp_w25q16_unlock();
        return false;
    }
    if(!w25q_wait_not_busy()) {
        bsp_w25q16_unlock();
        return false;
    }

    cmd[0] = CMD_SECTOR_ERASE_4K;
    cmd[1] = (uint8_t)((addr >> 16) & 0xFFu);
    cmd[2] = (uint8_t)((addr >> 8) & 0xFFu);
    cmd[3] = (uint8_t)(addr & 0xFFu);

    w25q_cs_low();
    w25q_soft_send(cmd, sizeof(cmd));
    w25q_cs_high();

    ok = w25q_wait_not_busy();
    bsp_w25q16_unlock();
    return ok;
}

uint32_t bsp_w25q16_read_jedec_id(void)
{
    uint8_t cmd = CMD_READ_JEDEC_ID;
    uint8_t id[3];

    if(s_w25q_ready == 0u) {
        return 0u;
    }

    w25q_cs_low();
    (void)w25q_soft_xfer_byte(cmd);
    id[0] = w25q_soft_xfer_byte(0xFFu);
    id[1] = w25q_soft_xfer_byte(0xFFu);
    id[2] = w25q_soft_xfer_byte(0xFFu);
    w25q_cs_high();

    return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | (uint32_t)id[2];
}

uint32_t bsp_w25q16_probe_jedec_id(void)
{
    uint32_t id;

    if(!bsp_w25q16_init()) {
        return 0u;
    }
    id = bsp_w25q16_read_jedec_id();
    bsp_w25q16_end_session();
    return id;
}

void bsp_w25q16_hw_diag(bsp_w25q16_hw_diag_t *out)
{
    if(out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    bsp_w25q16_end_session();
    w25q_gpio_soft_spi_setup();

    out->miso_idle = (HAL_GPIO_ReadPin(W25Q_MISO_PORT, W25Q_MISO_PIN) == GPIO_PIN_SET) ? 1u : 0u;
    out->cs_level = (HAL_GPIO_ReadPin(W25Q_CS_PORT, W25Q_CS_PIN) == GPIO_PIN_SET) ? 1u : 0u;
    out->sck_level = (HAL_GPIO_ReadPin(W25Q_SCK_PORT, W25Q_SCK_PIN) == GPIO_PIN_SET) ? 1u : 0u;

    HAL_GPIO_WritePin(W25Q_SCK_PORT, W25Q_SCK_PIN, GPIO_PIN_RESET);
    delay_us(5u);
    out->sck_lo = (HAL_GPIO_ReadPin(W25Q_SCK_PORT, W25Q_SCK_PIN) == GPIO_PIN_RESET) ? 1u : 0u;
    HAL_GPIO_WritePin(W25Q_SCK_PORT, W25Q_SCK_PIN, GPIO_PIN_SET);
    delay_us(5u);
    out->sck_hi = (HAL_GPIO_ReadPin(W25Q_SCK_PORT, W25Q_SCK_PIN) == GPIO_PIN_SET) ? 1u : 0u;
    HAL_GPIO_WritePin(W25Q_SCK_PORT, W25Q_SCK_PIN, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(W25Q_MOSI_PORT, W25Q_MOSI_PIN, GPIO_PIN_RESET);
    delay_us(5u);
    out->mosi_lo = (HAL_GPIO_ReadPin(W25Q_MOSI_PORT, W25Q_MOSI_PIN) == GPIO_PIN_RESET) ? 1u : 0u;
    HAL_GPIO_WritePin(W25Q_MOSI_PORT, W25Q_MOSI_PIN, GPIO_PIN_SET);
    delay_us(5u);
    out->mosi_hi = (HAL_GPIO_ReadPin(W25Q_MOSI_PORT, W25Q_MOSI_PIN) == GPIO_PIN_SET) ? 1u : 0u;
    HAL_GPIO_WritePin(W25Q_MOSI_PORT, W25Q_MOSI_PIN, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
    delay_us(5u);
    out->cs_lo = (HAL_GPIO_ReadPin(W25Q_CS_PORT, W25Q_CS_PIN) == GPIO_PIN_RESET) ? 1u : 0u;
    HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);
    delay_us(5u);
    out->cs_hi = (HAL_GPIO_ReadPin(W25Q_CS_PORT, W25Q_CS_PIN) == GPIO_PIN_SET) ? 1u : 0u;

    w25q_read_jedec_raw(out->jedec1);
    delay_us(50u);
    w25q_read_jedec_raw(out->jedec2);

    w25q_gpio_deinit_lines();
}

bool bsp_w25q16_init(void)
{
    bsp_w25q16_lock();
    if(s_w25q_ready != 0u) {
        bsp_w25q16_unlock();
        return true;
    }

    w25q_gpio_soft_spi_setup();

    {
        uint8_t id1[3];
        uint8_t id2[3];

        w25q_read_jedec_raw(id1);
        delay_us(10u);
        w25q_read_jedec_raw(id2);
        if(memcmp(id1, id2, sizeof(id1)) != 0 || !w25q_jedec_plausible(id1)) {
            W25Q_LOG("JEDEC fail %02X %02X %02X", id1[0], id1[1], id1[2]);
            w25q_gpio_deinit_lines();
            bsp_w25q16_unlock();
            return false;
        }
        W25Q_LOG("JEDEC OK %02X %02X %02X", id1[0], id1[1], id1[2]);
    }

    s_w25q_ready = 1u;
    bsp_w25q16_unlock();
    return true;
}

void bsp_w25q16_end_session(void)
{
    bsp_w25q16_lock();
    if(s_w25q_ready != 0u) {
        w25q_cs_high();
        w25q_gpio_deinit_lines();
        s_w25q_ready = 0u;
    }
    bsp_w25q16_unlock();
}
