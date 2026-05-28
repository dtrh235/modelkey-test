#ifndef __AS608_H
#define __AS608_H
#include "stm32f4xx_hal.h"


/* 手指检测脚：禁止用 PA6（本板 LCD 的 SPI1 MISO）。模块触摸感应输出接 PE10。改脚只改下面两宏。 */
#define AS608_STA_GPIO_PORT   GPIOE
#define AS608_STA_GPIO_PIN    GPIO_PIN_10
#define GZ_Sta   HAL_GPIO_ReadPin(AS608_STA_GPIO_PORT, AS608_STA_GPIO_PIN)
/* AS608 调试日志开关：1 开启，0 关闭 */
#define AS608_LOG_ENABLE      0
/* 1: 打开串口中断内的 AS608 字节镜像，供协议解析接收应答 */
#define AS608_UART_SNIFF_IN_ISR 1
#define CharBuffer1 0x01
#define CharBuffer2 0x02

extern uint32_t AS608Addr;//模块地址

typedef struct  
{
	uint16_t pageID;//指纹ID
	uint16_t mathscore;//匹配得分
}SearchResult;

typedef struct
{
	uint16_t GZ_max;//指纹最大容量
	uint8_t  GZ_level;//安全等级
	uint32_t GZ_addr;
	uint8_t  GZ_size;//通讯数据包大小
	uint8_t  GZ_N;//波特率基数N
}SysPara;

void GZ_StaGPIO_Init(void); /* 初始化 AS608_STA_* 为输入（读状态引脚） */
void as608_uart_rx_byte(uint8_t b); /* 由串口接收回调转发原始字节，供协议解析 */
void as608_rx_buffer_clear(void);  /* 清空指纹模块 UART 累积缓冲 */
uint16_t as608_rx_widx_get(void);  /* 当前累积 RX 字节数（诊断用） */
	
uint8_t GZ_GetImage(void); //录入图像 
 
uint8_t GZ_GenChar(uint8_t BufferID);//生成特征 

uint8_t GZ_Match(void);//精确比对两枚指纹特征 

uint8_t GZ_Search(uint8_t BufferID,uint16_t StartPage,uint16_t PageNum,SearchResult *p);//搜索指纹 

uint8_t GZ_SearchLibrary(uint8_t BufferID, SearchResult *p);
 
uint8_t GZ_RegModel(void);//合并特征（生成模板） 
 
uint8_t GZ_StoreChar(uint8_t BufferID,uint16_t PageID);//储存模板 

uint8_t GZ_DeletChar(uint16_t PageID,uint16_t N);//删除模板 

uint8_t GZ_Empty(void);//清空指纹库 

uint8_t GZ_WriteReg(uint8_t RegNum,uint8_t DATA);//写系统寄存器 
 
uint8_t GZ_ReadSysPara(SysPara *p); //读系统基本参数 

uint8_t GZ_SetAddr(uint32_t addr);  //设置模块地址 

uint8_t GZ_WriteNotepad(uint8_t NotePageNum,uint8_t *content);//写记事本 

uint8_t GZ_ReadNotepad(uint8_t NotePageNum,uint8_t *note);//读记事 

uint8_t GZ_HighSpeedSearch(uint8_t BufferID,uint16_t StartPage,uint16_t PageNum,SearchResult *p);//高速搜索 
  
uint8_t GZ_ValidTempleteNum(uint16_t *ValidN);//读有效模板个数 

uint8_t GZ_HandShake(uint32_t *GZ_Addr); //与AS608模块握手

/* 指纹库模板读写（用于主机→从机 RS485 同步） */
#define AS608_TEMPLATE_SIZE 512u
uint8_t GZ_LoadChar(uint8_t BufferID, uint16_t PageID);
uint8_t GZ_UpCharBuffer(uint8_t BufferID, uint8_t *tpl_buf, uint16_t tpl_buf_sz);
uint8_t GZ_DownCharBuffer(uint8_t BufferID, const uint8_t *tpl_buf, uint16_t tpl_len);
uint8_t GZ_ReadTemplatePage(uint16_t page_id, uint8_t *tpl_buf, uint16_t tpl_buf_sz);
uint8_t GZ_WriteTemplatePage(uint16_t page_id, const uint8_t *tpl_buf, uint16_t tpl_len);

const char *EnsureMessage(uint8_t ensure);//确认码错误信息解析

#endif
