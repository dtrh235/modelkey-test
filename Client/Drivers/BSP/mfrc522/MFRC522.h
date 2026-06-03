#ifndef __MFRC522_H
#define	__MFRC522_H

#include "stm32f4xx_hal.h"
#include "SYSTEM/delay/delay.h"

/*****************软SPI RC522 — 与 HAL 工程对接******************/

#define MFRC522_GPIO_SDA_PORT     GPIOB
#define MFRC522_GPIO_SDA_PIN      GPIO_PIN_12

#define MFRC522_GPIO_SCK_PORT     GPIOB
#define MFRC522_GPIO_SCK_PIN      GPIO_PIN_13

#define MFRC522_GPIO_MOSI_PORT    GPIOB
#define MFRC522_GPIO_MOSI_PIN     GPIO_PIN_15

#define MFRC522_GPIO_MISO_PORT    GPIOB
#define MFRC522_GPIO_MISO_PIN     GPIO_PIN_14

#define MFRC522_GPIO_RST_PORT     GPIOD
#define MFRC522_GPIO_RST_PIN      GPIO_PIN_1

#define MFRC522_SDA_L     HAL_GPIO_WritePin(MFRC522_GPIO_SDA_PORT, MFRC522_GPIO_SDA_PIN, GPIO_PIN_RESET)
#define MFRC522_SDA_H     HAL_GPIO_WritePin(MFRC522_GPIO_SDA_PORT, MFRC522_GPIO_SDA_PIN, GPIO_PIN_SET)

#define MFRC522_RST_L     HAL_GPIO_WritePin(MFRC522_GPIO_RST_PORT, MFRC522_GPIO_RST_PIN, GPIO_PIN_RESET)
#define MFRC522_RST_H     HAL_GPIO_WritePin(MFRC522_GPIO_RST_PORT, MFRC522_GPIO_RST_PIN, GPIO_PIN_SET)

#define MFRC522_SCK_L     HAL_GPIO_WritePin(MFRC522_GPIO_SCK_PORT, MFRC522_GPIO_SCK_PIN, GPIO_PIN_RESET)
#define MFRC522_SCK_H     HAL_GPIO_WritePin(MFRC522_GPIO_SCK_PORT, MFRC522_GPIO_SCK_PIN, GPIO_PIN_SET)

#define MFRC522_MOSI_L    HAL_GPIO_WritePin(MFRC522_GPIO_MOSI_PORT, MFRC522_GPIO_MOSI_PIN, GPIO_PIN_RESET)
#define MFRC522_MOSI_H    HAL_GPIO_WritePin(MFRC522_GPIO_MOSI_PORT, MFRC522_GPIO_MOSI_PIN, GPIO_PIN_SET)

#define MFRC522_MISO_READ ((HAL_GPIO_ReadPin(MFRC522_GPIO_MISO_PORT, MFRC522_GPIO_MISO_PIN) == GPIO_PIN_SET) ? 1u : 0u)

// ????????(????)
#define PCD_IDLE              0x00
#define PCD_AUTHENT           0x0E
#define PCD_RECEIVE           0x08
#define PCD_TRANSMIT          0x04
#define PCD_TRANSCEIVE        0x0C
#define PCD_RESETPHASE        0x0F
#define PCD_CALCCRC           0x03

#define PICC_REQIDL           0x26
#define PICC_REQALL           0x52
#define PICC_ANTICOLL1        0x93
#define PICC_ANTICOLL2        0x95
#define PICC_AUTHENT1A        0x60
#define PICC_AUTHENT1B        0x61
#define PICC_READ             0x30
#define PICC_WRITE            0xA0
#define PICC_HALT             0x50

#define DEF_FIFO_LENGTH       64
#define MAXRLEN  18

// ?????(????)
#define CommandReg            0x01
#define ComIEnReg             0x02
#define DivlEnReg             0x03
#define ComIrqReg             0x04
#define DivIrqReg             0x05
#define ErrorReg              0x06
#define Status1Reg            0x07
#define Status2Reg            0x08
#define FIFODataReg           0x09
#define FIFOLevelReg          0x0A
#define WaterLevelReg         0x0B
#define ControlReg            0x0C
#define BitFramingReg         0x0D
#define CollReg               0x0E
#define ModeReg               0x11
#define TxControlReg          0x14
#define TxAutoReg             0x15
#define CRCResultRegM         0x21
#define CRCResultRegL         0x22
#define TModeReg              0x2A
#define TPrescalerReg         0x2B
#define TReloadRegH           0x2C
#define TReloadRegL           0x2D

// ???
#define MI_OK                 0x26
#define MI_NOTAGERR           0xcc
#define MI_ERR                0xbb

// ????
void MFRC522_Init(void);
char MFRC522_Reset(void);
void Write_MFRC522(unsigned char Address, unsigned char value);
unsigned char Read_MFRC522(unsigned char Address);
void MFRC522_AntennaOn(void);
void MFRC522_AntennaOff(void);
char MFRC522_Request(unsigned char req_code,unsigned char *pTagType);
char MFRC522_Anticoll(unsigned char *pSnr);
char MFRC522_SelectTag(unsigned char *pSnr);
char MFRC522_AuthState(unsigned char auth_mode,unsigned char addr,unsigned char *pKey,unsigned char *pSnr);
char MFRC522_Read(unsigned char addr,unsigned char *pData);
char MFRC522_Halt(void);
char MFRC522_Write(unsigned char addr,unsigned char *pData);

#endif
