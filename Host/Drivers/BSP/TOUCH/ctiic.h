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

void ct_iic_init(void);
void ct_iic_stop(void);
void ct_iic_start(void);
void ct_iic_ack(void);
void ct_iic_nack(void);
uint8_t ct_iic_wait_ack(void);
void ct_iic_send_byte(uint8_t txd);
uint8_t ct_iic_read_byte(unsigned char ack);

#endif
