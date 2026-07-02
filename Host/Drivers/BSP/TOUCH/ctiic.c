#include "./BSP/TOUCH/ctiic.h"
#include "./SYSTEM/delay/delay.h"

static GPIO_TypeDef *s_scl_port = GPIOB;
static uint16_t s_scl_pin = GPIO_PIN_6;
static GPIO_TypeDef *s_sda_port = GPIOB;
static uint16_t s_sda_pin = GPIO_PIN_7;
static uint8_t s_bus_id = 0u;

static void ct_iic_gpio_clk(GPIO_TypeDef *port)
{
    if(port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    } else if(port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    } else if(port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    } else if(port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    } else if(port == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    } else if(port == GPIOF) {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    }
}

static void ct_scl_wr(uint8_t x)
{
    HAL_GPIO_WritePin(s_scl_port, s_scl_pin, x ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void ct_sda_wr(uint8_t x)
{
    HAL_GPIO_WritePin(s_sda_port, s_sda_pin, x ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static uint8_t ct_sda_rd(void)
{
    return (HAL_GPIO_ReadPin(s_sda_port, s_sda_pin) == GPIO_PIN_SET) ? 1u : 0u;
}

#ifndef CT_IIC_DELAY_US
#define CT_IIC_DELAY_US  5u
#endif

static void ct_iic_delay(void)
{
    delay_us(CT_IIC_DELAY_US);
}

void ct_iic_bind(GPIO_TypeDef *scl_port, uint16_t scl_pin, GPIO_TypeDef *sda_port, uint16_t sda_pin, uint8_t bus_id)
{
    s_scl_port = scl_port;
    s_scl_pin = scl_pin;
    s_sda_port = sda_port;
    s_sda_pin = sda_pin;
    s_bus_id = bus_id;
}

uint8_t ct_iic_bus_id(void)
{
    return s_bus_id;
}

const char *ct_iic_bus_name(void)
{
    if(s_bus_id == 0u) {
        return "PB6/PB7";
    }
    if(s_bus_id == 1u) {
        return "PB0/PF11";
    }
    return "?";
}

uint8_t ct_iic_probe_wr(uint8_t addr_wr)
{
    uint8_t ack;

    ct_iic_start();
    ct_iic_send_byte(addr_wr);
    ack = ct_iic_wait_ack();
    ct_iic_stop();
    return ack;
}

uint8_t ct_iic_pin_read_sda(void)
{
    return ct_sda_rd();
}

uint8_t ct_iic_pin_read_scl(void)
{
    return (HAL_GPIO_ReadPin(s_scl_port, s_scl_pin) == GPIO_PIN_SET) ? 1u : 0u;
}

void ct_iic_gpio_line_test(ct_iic_gpio_test_t *out)
{
    if(out == NULL) {
        return;
    }

    out->scl_low = 0u;
    out->scl_high = 0u;
    out->sda_low = 0u;
    out->sda_high = 0u;

    ct_scl_wr(0);
    ct_iic_delay();
    if(ct_iic_pin_read_scl() == 0u) {
        out->scl_low = 1u;
    }
    ct_scl_wr(1);
    ct_iic_delay();
    if(ct_iic_pin_read_scl() != 0u) {
        out->scl_high = 1u;
    }

    ct_sda_wr(0);
    ct_iic_delay();
    if(ct_sda_rd() == 0u) {
        out->sda_low = 1u;
    }
    ct_sda_wr(1);
    ct_iic_delay();
    if(ct_sda_rd() != 0u) {
        out->sda_high = 1u;
    }
}

uint8_t ct_iic_bus_scan(uint8_t *addr_wr, uint8_t max, uint8_t *found)
{
    uint8_t n = 0u;
    uint8_t a7;

    if(found != NULL) {
        *found = 0u;
    }
    if(addr_wr == NULL || max == 0u) {
        return 0u;
    }

    for(a7 = 1u; a7 < 0x7Fu; a7++) {
        uint8_t wr = (uint8_t)(a7 << 1);
        if(ct_iic_probe_wr(wr) == 0u) {
            addr_wr[n++] = wr;
            if(found != NULL) {
                *found = n;
            }
            if(n >= max) {
                break;
            }
        }
    }

    if(found != NULL) {
        *found = n;
    }
    return n;
}

uint8_t ct_iic_rd_reg_trace(uint8_t dev_wr, uint8_t dev_rd, uint8_t reg, uint8_t *buf, uint8_t len, uint8_t *ack0, uint8_t *ack1, uint8_t *ack2)
{
    uint8_t i;
    uint8_t a0 = 1u;
    uint8_t a1 = 1u;
    uint8_t a2 = 1u;

    if(buf == NULL || len == 0u) {
        return 1u;
    }

    ct_iic_start();
    ct_iic_send_byte(dev_wr);
    a0 = ct_iic_wait_ack();
    ct_iic_send_byte(reg);
    a1 = ct_iic_wait_ack();
    ct_iic_start();
    ct_iic_send_byte(dev_rd);
    a2 = ct_iic_wait_ack();

    for(i = 0u; i < len; i++) {
        buf[i] = ct_iic_read_byte((i + 1u) >= len ? 0u : 1u);
    }

    ct_iic_stop();

    if(ack0 != NULL) {
        *ack0 = a0;
    }
    if(ack1 != NULL) {
        *ack1 = a1;
    }
    if(ack2 != NULL) {
        *ack2 = a2;
    }

    return (uint8_t)(a0 | a1 | a2);
}

void ct_iic_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    ct_iic_gpio_clk(s_scl_port);
    ct_iic_gpio_clk(s_sda_port);

    gpio_init_struct.Pin = s_scl_pin;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(s_scl_port, &gpio_init_struct);

    gpio_init_struct.Pin = s_sda_pin;
    HAL_GPIO_Init(s_sda_port, &gpio_init_struct);

    ct_iic_stop();
}

void ct_iic_start(void)
{
    ct_sda_wr(1);
    ct_scl_wr(1);
    ct_iic_delay();
    ct_sda_wr(0);
    ct_iic_delay();
    ct_scl_wr(0);
    ct_iic_delay();
}

void ct_iic_stop(void)
{
    ct_sda_wr(0);
    ct_iic_delay();
    ct_scl_wr(1);
    ct_iic_delay();
    ct_sda_wr(1);
    ct_iic_delay();
}

uint8_t ct_iic_wait_ack(void)
{
    uint8_t waittime = 0;
    uint8_t rack = 0;

    ct_sda_wr(1);
    ct_iic_delay();
    ct_scl_wr(1);
    ct_iic_delay();

    while(ct_sda_rd()) {
        waittime++;
        if(waittime > 250) {
            ct_iic_stop();
            rack = 1;
            break;
        }
        ct_iic_delay();
    }

    ct_scl_wr(0);
    ct_iic_delay();
    return rack;
}

void ct_iic_ack(void)
{
    ct_sda_wr(0);
    ct_iic_delay();
    ct_scl_wr(1);
    ct_iic_delay();
    ct_scl_wr(0);
    ct_iic_delay();
    ct_sda_wr(1);
    ct_iic_delay();
}

void ct_iic_nack(void)
{
    ct_sda_wr(1);
    ct_iic_delay();
    ct_scl_wr(1);
    ct_iic_delay();
    ct_scl_wr(0);
    ct_iic_delay();
}

void ct_iic_send_byte(uint8_t data)
{
    uint8_t t;

    for(t = 0; t < 8; t++) {
        ct_sda_wr((data & 0x80) >> 7);
        ct_iic_delay();
        ct_scl_wr(1);
        ct_iic_delay();
        ct_scl_wr(0);
        data <<= 1;
    }

    ct_sda_wr(1);
}

uint8_t ct_iic_read_byte(unsigned char ack)
{
    uint8_t i;
    uint8_t receive = 0;

    for(i = 0; i < 8; i++) {
        receive <<= 1;
        ct_scl_wr(1);
        ct_iic_delay();
        if(ct_sda_rd()) {
            receive++;
        }
        ct_scl_wr(0);
        ct_iic_delay();
    }

    if(!ack) {
        ct_iic_nack();
    } else {
        ct_iic_ack();
    }

    return receive;
}
