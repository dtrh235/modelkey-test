#ifndef __CT_IIC_H
#define __CT_IIC_H

#include "./SYSTEM/sys/sys.h"

/* ??? ILI9341+FT6336 ??�SCL=PB6 SDA=PB7??init ?????????? PB0/PF11 */
#define CT_IIC_SCL_GPIO_PORT            GPIOB
#define CT_IIC_SCL_GPIO_PIN             GPIO_PIN_6
#define CT_IIC_SDA_GPIO_PORT            GPIOB
#define CT_IIC_SDA_GPIO_PIN             GPIO_PIN_7

void ct_iic_bind(GPIO_TypeDef *scl_port, uint16_t scl_pin, GPIO_TypeDef *sda_port, uint16_t sda_pin, uint8_t bus_id);
uint8_t ct_iic_bus_id(void);
const char *ct_iic_bus_name(void);
uint8_t ct_iic_probe_wr(uint8_t addr_wr);
uint8_t ct_iic_rd_reg_trace(uint8_t dev_wr, uint8_t dev_rd, uint8_t reg, uint8_t *buf, uint8_t len, uint8_t *ack0, uint8_t *ack1, uint8_t *ack2);
uint8_t ct_iic_pin_read_sda(void);
uint8_t ct_iic_pin_read_scl(void);

typedef struct {
    uint8_t scl_low;   /* 1: open-drain low, pin reads 0 */
    uint8_t scl_high;  /* 1: release, pin reads 1 */
    uint8_t sda_low;
    uint8_t sda_high;
} ct_iic_gpio_test_t;

void ct_iic_gpio_line_test(ct_iic_gpio_test_t *out);
/* Scan 7-bit addr 0x01..0x7F; returns count; addr_wr[] holds 8-bit write addr */
uint8_t ct_iic_bus_scan(uint8_t *addr_wr, uint8_t max, uint8_t *found);

void ct_iic_init(void);
void ct_iic_stop(void);
void ct_iic_start(void);
void ct_iic_ack(void);
void ct_iic_nack(void);
uint8_t ct_iic_wait_ack(void);
void ct_iic_send_byte(uint8_t txd);
uint8_t ct_iic_read_byte(unsigned char ack);

#endif
