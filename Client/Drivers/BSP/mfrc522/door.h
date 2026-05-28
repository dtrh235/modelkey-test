#ifndef __DOOR_H
#define __DOOR_H

#include <stdint.h>

void Flash_ReadID(void);
uint8_t Flash_AddID(uint8_t *id);
/* 与矩阵键盘 KEY_Init 重名冲突，单独命名；注意 PA1 可能与 key.c 中 PA1 用途冲突 */
void Door_Enroll_Key_Init(void);
void flash_card(void);

// NFC录入相关函数
uint8_t NFC_Enroll_Detect(uint8_t *card_id, uint32_t timeout_ms);
uint8_t NFC_Enroll_Card(void);
/* 单次检测并与已录入卡库比对：1=已录入，0=无卡或未录入 */
uint8_t NFC_Check_Registered(uint8_t *uid_out);
/* 单次读卡UID：1=检测到，0=未检测到 */
uint8_t NFC_Read_UID(uint8_t *uid_out);

extern uint8_t card_count;
extern uint8_t rfid_state;
#endif

