#include "door.h"
#include "MFRC522.h"
#include <string.h>
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "SYSTEM/usart/usart.h"
#include "SYSTEM/delay/delay.h"

#define printf(...) ((void)0)

#define MAX_CARD_NUM      10
#define FLASH_STORE_ADDR  ((uint32_t)0x08080000)
extern uint32_t Image$$ER_IROM1$$RO$$Limit;

unsigned char card_buf[10];
unsigned char save_ID[MAX_CARD_NUM][4] = {0};
uint8_t card_count = 0;
uint8_t rfid_state = 0; 
static uint8_t g_flash_store_conflict_warned = 0u;

static uint8_t flash_store_is_safe(void)
{
    uint32_t fw_end = (uint32_t)&Image$$ER_IROM1$$RO$$Limit;
    /* 当前工程把程序放到了 0x080Fxxxx，不能再擦写 sector8(0x08080000) */
    return (fw_end <= FLASH_STORE_ADDR) ? 1u : 0u;
}

/* ????????????? PA1?????? key.c ??? PA1 ????????????????????????? */
#define DOOR_KEY_READ()  (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET)

void Flash_ReadID(void)
{
    uint8_t i, j;
    if(!flash_store_is_safe()) {
        card_count = 0u;
        if(!g_flash_store_conflict_warned) {
            printf("[NFC][WARN] flash sector conflict, use RAM only\r\n");
            g_flash_store_conflict_warned = 1u;
        }
        return;
    }
    card_count = *(uint8_t*)(FLASH_STORE_ADDR + MAX_CARD_NUM*4);

    if(card_count > MAX_CARD_NUM || card_count == 0xFF)
    {
        card_count = 0;
        return;
    }

    for(i=0; i<card_count; i++)
    {
        for(j=0; j<4; j++)
        {
            save_ID[i][j] = *(uint8_t*)(FLASH_STORE_ADDR + i*4 + j);
        }
    }
}

uint8_t Flash_AddID(uint8_t *id)
{
    uint8_t i, j;

    if(card_count >= MAX_CARD_NUM) return 1;

    for(i=0; i<card_count; i++)
    {
        if(memcmp(save_ID[i], id, 4)==0) return 2;
    }

    memcpy(save_ID[card_count], id, 4);
    card_count++;

    if(!flash_store_is_safe()) {
        if(!g_flash_store_conflict_warned) {
            printf("[NFC][WARN] flash write skipped, use RAM only\r\n");
            g_flash_store_conflict_warned = 1u;
        }
        return 0;
    }

    {
        FLASH_EraseInitTypeDef erase = {0};
        uint32_t sect_err = 0;

        HAL_FLASH_Unlock();
        erase.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        erase.Sector = FLASH_SECTOR_8;
        erase.NbSectors = 1;
        if(HAL_FLASHEx_Erase(&erase, &sect_err) != HAL_OK) {
            HAL_FLASH_Lock();
            return 1;
        }

        for(i = 0; i < card_count; i++) {
            for(j = 0; j < 4; j++) {
                if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, FLASH_STORE_ADDR + i * 4u + j,
                                     (uint64_t)save_ID[i][j]) != HAL_OK) {
                    HAL_FLASH_Lock();
                    return 1;
                }
            }
        }
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, FLASH_STORE_ADDR + MAX_CARD_NUM * 4u,
                             (uint64_t)card_count) != HAL_OK) {
            HAL_FLASH_Lock();
            return 1;
        }
        HAL_FLASH_Lock();
    }
    return 0;
}

void Door_Enroll_Key_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = GPIO_PIN_1;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLDOWN;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
}

// ????
// NFC录入状态检测
uint8_t NFC_Enroll_Detect(uint8_t *card_id, uint32_t timeout_ms)
{
    unsigned char status;
    unsigned char anticoll_status;
    uint32_t start_time = HAL_GetTick();
    uint32_t last_log_time = start_time;

    if(card_id == NULL || timeout_ms == 0u) {
        return 0u;
    }

    while((HAL_GetTick() - start_time) < timeout_ms) {
        status = MFRC522_Request(PICC_REQALL, card_buf);
        anticoll_status = 0xFFu;
        if(status == MI_OK) {
            anticoll_status = (unsigned char)MFRC522_Anticoll(card_buf);
            if(anticoll_status == MI_OK) {
                // 检测到NFC卡
                memcpy(card_id, card_buf, 4);
                MFRC522_Halt();
                return 1; // 录入成功
            }
        }
        if((HAL_GetTick() - last_log_time) >= 500u) {
            last_log_time = HAL_GetTick();
            printf("[NFC][detect] req=%02X anti=%02X\r\n", status, anticoll_status);
        }
        delay_ms(5); // 提高轮询频率，避免UI超时感知迟钝
    }

    return 0; // 超时未检测到
}

// NFC录入函数
uint8_t NFC_Enroll_Card(void)
{
    uint8_t card_id[4] = {0};
    uint8_t ret;
    
    // 检测NFC卡，超时时间设为10秒
    if(NFC_Enroll_Detect(card_id, 10000)) {
        // 录入到Flash
        ret = Flash_AddID(card_id);
        if(ret == 0) {
            printf("NFC enroll OK\r\n");
            return 1; // 录入成功
        } else if(ret == 1) {
            printf("NFC storage full\r\n");
            return 2; // 存储空间已满
        } else if(ret == 2) {
            printf("NFC card exists\r\n");
            return 3; // 卡已存在
        }
    }
    
    return 0; // 录入失败
}

uint8_t NFC_Check_Registered(uint8_t *uid_out)
{
    unsigned char status;
    uint8_t i;

    status = MFRC522_Request(PICC_REQALL, card_buf);
    if(status != MI_OK) {
        return 0u;
    }

    status = MFRC522_Anticoll(card_buf);
    if(status != MI_OK) {
        return 0u;
    }

    MFRC522_Halt();
    for(i = 0u; i < card_count; i++) {
        if(memcmp(card_buf, save_ID[i], 4) == 0) {
            if(uid_out != NULL) {
                memcpy(uid_out, card_buf, 4);
            }
            return 1u;
        }
    }
    return 0u;
}

uint8_t NFC_Read_UID(uint8_t *uid_out)
{
    unsigned char status;
    if(uid_out == NULL) return 0u;

    status = MFRC522_Request(PICC_REQALL, card_buf);
    if(status != MI_OK) return 0u;

    status = MFRC522_Anticoll(card_buf);
    if(status != MI_OK) return 0u;

    memcpy(uid_out, card_buf, 4);
    MFRC522_Halt();
    return 1u;
}

void flash_card(void)
{
	
    unsigned char status;
    uint8_t i, find;
rfid_state = 0;
    status = MFRC522_Request(PICC_REQALL, card_buf);
    if(status != MI_OK) {
			//rfid_state = 0;
			return;
		}
    status = MFRC522_Anticoll(card_buf);
    if(status != MI_OK){
//rfid_state = 0;
		return;
		}
    printf("????:");
    for(i=0; i<4; i++)
    {
        printf("%02X ", card_buf[i]);
    }
    printf("\r\n");

    if(DOOR_KEY_READ())
    {
        delay_ms(20);
        if(DOOR_KEY_READ())
        {
            uint8_t ret = Flash_AddID(card_buf);
            if(ret == 0)
            {
                printf("?????!\r\n");
                printf("??????????:%d\r\n\r\n", card_count);
            }
            else if(ret == 1)
            {
                printf("?????????!\r\n\r\n");
            }
            else if(ret == 2)
            {
                printf("????????!\r\n\r\n");
            }

           // while(KEY_READ == 0);
            MFRC522_Halt();
            delay_ms(300);
            return;
        }
    }

    if(card_count == 0)
    {
        printf("????????????????????\r\n\r\n");
			rfid_state = 0;
        delay_ms(1000);
    }
    else
    {
        find = 0;
        for(i = 0; i < card_count; i++)
        {
            if(memcmp(card_buf, save_ID[i], 4) == 0)
            {
                find = 1;
                break;
            }
        }
       //rfid_state = 0;
        if(find){
            printf("??????,??????!\r\n\r\n");
				    rfid_state = 1;
					delay_ms(10000);
				}		
        else{
            printf("?????!\r\n\r\n");
				    rfid_state = 0;
				}
        delay_ms(500);
    }
 
    MFRC522_Halt();
    delay_ms(300);
}
