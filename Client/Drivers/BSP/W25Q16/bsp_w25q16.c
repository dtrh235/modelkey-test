#include "bsp_w25q16.h"

#include <string.h>
#include <stdio.h>

#include "SYSTEM/delay/delay.h"

/*
 * Winbond W25Q16 鈥?杞欢妯℃嫙 SPI锛圡ode0: CPOL=0 CPHA=0锛夛紝浠呭崰 GPIO锛屼笉鍗?SPI 澶栬銆?
 * PA15=CS, PB3=SCK, PB5=MOSI, PB4=MISO锛堟澘杞藉浐瀹氳蛋绾匡級銆?
 */

#define W25Q_CS_PORT  GPIOA
#define W25Q_CS_PIN   GPIO_PIN_15
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

#define W25Q_LOG(fmt, ...) ((void)0)

static uint8_t s_w25q_ready;

static void w25q_delay_pins(void)
{
    delay_us(2u);
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
    /* 鎬荤嚎鎮┖甯歌 0xFF锛涘叏 0 澶氫负鏃犲搷搴?*/
    if(id[0] == 0xFFu && id[1] == 0xFFu && id[2] == 0xFFu) return 0u;
    if(id[0] == 0u && id[1] == 0u && id[2] == 0u) return 0u;
    switch(id[0]) {
    case 0xEFu: /* Winbond */
    case 0xC8u: /* GigaDevice / 鍏嗘槗绛?*/
    case 0xC2u: /* Macronix */
    case 0x9Du: /* ISSI */
    case 0x0Bu: /* XTX */
    case 0x68u: /* 閮ㄥ垎鍏煎鍓墝 */
    case 0x1Fu: /* Adesto */
    case 0x85u: /* Puya 绛?*/
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

static void w25q_gpio_soft_spi_setup(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
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
}

bool bsp_w25q16_read(uint32_t addr, uint8_t *buf, size_t len)
{
    uint8_t hdr[4];

    if(s_w25q_ready == 0u || buf == NULL || len == 0u) {
        return false;
    }
    if(addr + (uint32_t)len > 0x00200000u) {
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
    return true;
}

bool bsp_w25q16_write_page(uint32_t addr, const uint8_t *buf, size_t len)
{
    size_t i;

    if(s_w25q_ready == 0u || buf == NULL || len == 0u) {
        return false;
    }
    if(((addr & 0xFFu) + len) > 256u) {
        return false;
    }

    if(!w25q_write_enable()) {
        return false;
    }
    if(!w25q_wait_not_busy()) {
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

    return w25q_wait_not_busy();
}

bool bsp_w25q16_erase_sector_4k(uint32_t addr)
{
    uint8_t cmd[4];

    if(s_w25q_ready == 0u) {
        return false;
    }
    addr &= 0xFFFFF000u;

    if(!w25q_write_enable()) {
        return false;
    }
    if(!w25q_wait_not_busy()) {
        return false;
    }

    cmd[0] = CMD_SECTOR_ERASE_4K;
    cmd[1] = (uint8_t)((addr >> 16) & 0xFFu);
    cmd[2] = (uint8_t)((addr >> 8) & 0xFFu);
    cmd[3] = (uint8_t)(addr & 0xFFu);

    w25q_cs_low();
    w25q_soft_send(cmd, sizeof(cmd));
    w25q_cs_high();

    return w25q_wait_not_busy();
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

bool bsp_w25q16_init(void)
{
    if(s_w25q_ready != 0u) {
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
            W25Q_LOG("JEDEC invalid id1=%02X %02X %02X id2=%02X %02X %02X", id1[0], id1[1], id1[2], id2[0], id2[1], id2[2]);
            w25q_gpio_deinit_lines();
            return false;
        }
        W25Q_LOG("JEDEC OK id=%02X %02X %02X", id1[0], id1[1], id1[2]);
    }

    s_w25q_ready = 1u;
    return true;
}

void bsp_w25q16_end_session(void)
{
    if(s_w25q_ready != 0u) {
        w25q_gpio_deinit_lines();
        s_w25q_ready = 0u;
    }
}
