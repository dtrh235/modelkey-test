#ifndef APP_RS485_LINK_H
#define APP_RS485_LINK_H

#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#include "app_config.h"

#if (APP_RS485_ENABLE == 1)

/*
 * 硬件（STM32F407 USART6 复用）：
 *   PC6 = USART6_TX → 接 MAX485 类模块 DI（MCU 发 → 总线驱）
 *   PC7 = USART6_RX ← 接模块 RO
 *   PC8 = DE/RE 短接后接到同一 GPIO（高=发送使能，低=接收）
 * 若模块丝印将 DI/RO 与 PC6/PC7 反接，请改线使 TX 仍接 DI、RX 仍接 RO。
 */

void app_rs485_link_init(void);
uint8_t app_rs485_link_ready(void);

void app_rs485_de_tx(void);
void app_rs485_de_rx(void);

HAL_StatusTypeDef app_rs485_raw_send(const uint8_t *data, uint16_t len, uint32_t tout_ms);
HAL_StatusTypeDef app_rs485_raw_recv(uint8_t *data, uint16_t len, uint32_t tout_ms);
HAL_StatusTypeDef app_rs485_recv_byte(uint8_t *b, uint32_t tout_ms);
void app_rs485_flush_rx(void);

/* 收完整一帧（含 CRC）；超时或缓冲区不足返回 0。 */
uint16_t app_rs485_recv_frame(uint8_t *buf, uint16_t cap, uint32_t overall_timeout_ms);

#endif /* APP_RS485_ENABLE */

#endif /* APP_RS485_LINK_H */
