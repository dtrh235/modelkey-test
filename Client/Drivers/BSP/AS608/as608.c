#include <string.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "as608.h"
#include "SYSTEM/usart/usart.h"

#define AS608_RX_BUF_SZ 512u

uint32_t AS608Addr = 0XFFFFFFFF; /* 默认模块地址 */
uint8_t aRxBuffer[AS608_RX_BUF_SZ];
volatile uint8_t RX_len; /* 接收字节计数 */

static volatile uint16_t s_as608_rx_widx;
static uint8_t s_tpl_wr_last_stage;
static uint8_t s_tpl_wr_last_code;
static uint16_t s_tpl_wr_last_rx;

#define AS608_TPL_STAGE_NONE          0u
#define AS608_TPL_STAGE_START_CMD     1u
#define AS608_TPL_STAGE_DATA_ACK      2u
#define AS608_TPL_STAGE_STORE_CHAR    3u

#if AS608_LOG_ENABLE
#define AS608_LOG(fmt, ...)  ((void)0)
static void as608_log_result(const char *cmd, uint8_t ensure)
{
    AS608_LOG("%s ret=0x%02X (%s)", cmd, ensure, EnsureMessage(ensure));
}
#else
#define AS608_LOG(...)       ((void)0)
static void as608_log_result(const char *cmd, uint8_t ensure)
{
    (void)cmd;
    (void)ensure;
}
#endif

void as608_rx_buffer_clear(void)
{
    s_as608_rx_widx = 0u;
    aRxBuffer[0] = 0u;
    RX_len = 0u;
}

static void as608_uart_poll_rx(void)
{
    if(g_uart1_handle.Instance == NULL) {
        return;
    }
    while(__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_RXNE) != RESET) {
        uint8_t b = (uint8_t)(READ_REG(g_uart1_handle.Instance->DR) & 0xFFu);
        as608_uart_rx_byte(b);
    }
}

uint16_t as608_rx_widx_get(void)
{
    return s_as608_rx_widx;
}

uint8_t as608_write_tpl_last_stage(void)
{
    return s_tpl_wr_last_stage;
}

uint8_t as608_write_tpl_last_code(void)
{
    return s_tpl_wr_last_code;
}

uint16_t as608_write_tpl_last_rx(void)
{
    return s_tpl_wr_last_rx;
}

void as608_uart_rx_byte(uint8_t b)
{
    if(s_as608_rx_widx < (AS608_RX_BUF_SZ - 1u))
    {
        aRxBuffer[s_as608_rx_widx++] = b;
        aRxBuffer[s_as608_rx_widx] = 0u;
    }
    else
    {
        as608_rx_buffer_clear();
        aRxBuffer[s_as608_rx_widx++] = b;
        aRxBuffer[s_as608_rx_widx] = 0u;
    }
    RX_len = 1u;
}

void GZ_StaGPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    /* GPIOx 为指针宏，不能用于 #if；用运行时比较选 RCC */
    if(AS608_STA_GPIO_PORT == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    } else if(AS608_STA_GPIO_PORT == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    } else if(AS608_STA_GPIO_PORT == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    } else if(AS608_STA_GPIO_PORT == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    } else if(AS608_STA_GPIO_PORT == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    } else if(AS608_STA_GPIO_PORT == GPIOF) {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    } else if(AS608_STA_GPIO_PORT == GPIOG) {
        __HAL_RCC_GPIOG_CLK_ENABLE();
    }

    gpio.Pin = AS608_STA_GPIO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(AS608_STA_GPIO_PORT, &gpio);
}

//串口发送一个字节
static void Com_SendData(uint8_t data)
{
	/* Slave FP write runs concurrently with RS485 task; if preempted, 5ms per-byte timeout
	 * is too tight and causes template packet loss (0x01 / 0xFF). Keep margin same as host. */
	HAL_UART_Transmit(&g_uart1_handle, &data, 1u, 50u);
}
//发送包头
static void SendHead(void)
{
    /* 每次发送新命令前清空历史串口数据，避免旧包干扰解析 */
    as608_rx_buffer_clear();
	Com_SendData(0xEF);
	Com_SendData(0x01);
}
//发送地址
static void SendAddr(void)
{
	Com_SendData(AS608Addr>>24);
	Com_SendData(AS608Addr>>16);
	Com_SendData(AS608Addr>>8);
	Com_SendData(AS608Addr);
}
//发送包标识,
static void SendFlag(uint8_t flag)
{
	Com_SendData(flag);
}
//发送包长度
static void SendLength(int length)
{
	Com_SendData(length>>8);
	Com_SendData(length);
}
//发送指令码
static void Sendcmd(uint8_t cmd)
{
	Com_SendData(cmd);
}
//发送校验和
static void SendCheck(uint16_t check)
{
	Com_SendData(check>>8);
	Com_SendData(check);
}
//判断中断接收的数组有没有应答包
//waittime为等待中断接收数据的时间（单位1ms）
//返回值：数据包首地址
static uint8_t *JudgeStr(uint16_t waittime)
{
    uint16_t i;
    uint16_t widx;

	while(--waittime)
	{
		as608_uart_poll_rx();
		HAL_Delay(1);
		if(RX_len) /* 接收到一次数据 */
		{
			RX_len = 0;
            widx = s_as608_rx_widx;
            if(widx < 10u) {
                continue;
            }
            for(i = 0u; i + 9u < widx; i++)
            {
                /* 二进制协议包头: EF 01 + Addr(4B) + 07(应答包) + Len(2B) + Confirm */
                if((aRxBuffer[i] == 0xEFu) &&
                   (aRxBuffer[i + 1u] == 0x01u) &&
                   (aRxBuffer[i + 6u] == 0x07u))
                {
                    return &aRxBuffer[i];
                }
            }
		}
	}
	return 0;
}
//录入图像 GZ_GetImage
//功能:探测手指，探测到后录入指纹图像存于ImageBuffer。 
//模块返回确认字
uint8_t GZ_GetImage(void)
{
  uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x03);
	Sendcmd(0x01);
  temp =  0x01+0x03+0x01;
	SendCheck(temp);
	data=JudgeStr(2000);
	if(data)
		ensure=data[9];
	else
		ensure=0xff;
    as608_log_result("GetImage", ensure);
	return ensure;
}
//生成特征 GZ_GenChar
//功能:将ImageBuffer中的原始图像生成指纹特征文件存于CharBuffer1或CharBuffer2			 
//参数:BufferID --> charBuffer1:0x01	charBuffer1:0x02										
//模块返回确认字
uint8_t GZ_GenChar(uint8_t BufferID)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x04);
	Sendcmd(0x02);
	Com_SendData(BufferID);
	temp = 0x01+0x04+0x02+BufferID;
	SendCheck(temp);
	data=JudgeStr(2000);
	if(data)
		ensure=data[9];
	else
		ensure=0xff;
    AS608_LOG("GenChar buffer=%u", (unsigned)BufferID);
    as608_log_result("GenChar", ensure);
	return ensure;
}
//精确比对两枚指纹特征 GZ_Match
//功能:精确比对CharBuffer1 与CharBuffer2 中的特征文件 
//模块返回确认字
uint8_t GZ_Match(void)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x03);
	Sendcmd(0x03);
	temp = 0x01+0x03+0x03;
	SendCheck(temp);
	data=JudgeStr(2000);
	if(data)
		ensure=data[9];
	else
		ensure=0xff;
	return ensure;
}
//搜索指纹 GZ_Search
//功能:以CharBuffer1或CharBuffer2中的特征文件搜索整个或部分指纹库.若搜索到，则返回页码。			
//参数:  BufferID @ref CharBuffer1	CharBuffer2
//说明:  模块返回确认字，页码（相配指纹模板）
uint8_t GZ_Search(uint8_t BufferID,uint16_t StartPage,uint16_t PageNum,SearchResult *p)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x08);
	Sendcmd(0x04);
	Com_SendData(BufferID);
	Com_SendData(StartPage>>8);
	Com_SendData(StartPage);
	Com_SendData(PageNum>>8);
	Com_SendData(PageNum);
	temp = 0x01+0x08+0x04+BufferID
	+(StartPage>>8)+(uint8_t)StartPage
	+(PageNum>>8)+(uint8_t)PageNum;
	SendCheck(temp);
	data=JudgeStr(2000);
	if(data)
	{
		ensure = data[9];
		p->pageID   =(data[10]<<8)+data[11];
		p->mathscore=(data[12]<<8)+data[13];	
	}
	else
		ensure = 0xff;
    if(ensure == 0x00u)
    {
        AS608_LOG("Search OK: id=%u score=%u", (unsigned)p->pageID, (unsigned)p->mathscore);
    }
    else
    {
        AS608_LOG("Search fail: buffer=%u start=%u num=%u", (unsigned)BufferID, (unsigned)StartPage, (unsigned)PageNum);
    }
    as608_log_result("Search", ensure);
	return ensure;	
}

uint8_t GZ_SearchLibrary(uint8_t BufferID, SearchResult *p)
{
    const uint16_t page_num = 300u;
    uint8_t en;

    if(p == NULL) {
        return 0xFFu;
    }
    en = GZ_Search(BufferID, 0u, page_num, p);
    if(en == 0x00u) {
        return en;
    }
    if(en == 0x08u || en == 0x09u || en == 0x17u) {
        uint8_t en2 = GZ_HighSpeedSearch(BufferID, 0u, page_num, p);
        if(en2 == 0x00u) {
            return en2;
        }
    }
    return en;
}

//合并特征（生成模板）GZ_RegModel
//功能:将CharBuffer1与CharBuffer2中的特征文件合并生成 模板,结果存于CharBuffer1与CharBuffer2	
//说明:  模块返回确认字
uint8_t GZ_RegModel(void)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x03);
	Sendcmd(0x05);
	temp = 0x01+0x03+0x05;
	SendCheck(temp);
	data=JudgeStr(2000);
	if(data)
		ensure=data[9];
	else
		ensure=0xff;
    as608_log_result("RegModel", ensure);
	return ensure;		
}
//储存模板 GZ_StoreChar
//功能:将 CharBuffer1 或 CharBuffer2 中的模板文件存到 PageID 号flash数据库位置。			
//参数:  BufferID @ref charBuffer1:0x01	charBuffer1:0x02
//       PageID（指纹库位置号）
//说明:  模块返回确认字
uint8_t GZ_StoreChar(uint8_t BufferID,uint16_t PageID)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x06);
	Sendcmd(0x06);
	Com_SendData(BufferID);
	Com_SendData(PageID>>8);
	Com_SendData(PageID);
	temp = 0x01+0x06+0x06+BufferID
	+(PageID>>8)+(uint8_t)PageID;
	SendCheck(temp);
	data=JudgeStr(2000);
	if(data)
		ensure=data[9];
	else
		ensure=0xff;
    AS608_LOG("StoreChar buffer=%u page=%u", (unsigned)BufferID, (unsigned)PageID);
    as608_log_result("StoreChar", ensure);
	return ensure;	
}
//删除模板 GZ_DeletChar
//功能:  删除flash数据库中指定ID号开始的N个指纹模板
//参数:  PageID(指纹库模板号)，N删除的模板个数。
//说明:  模块返回确认字
uint8_t GZ_DeletChar(uint16_t PageID,uint16_t N)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x07);
	Sendcmd(0x0C);
	Com_SendData(PageID>>8);
	Com_SendData(PageID);
	Com_SendData(N>>8);
	Com_SendData(N);
	temp = 0x01+0x07+0x0C+(PageID>>8)+(uint8_t)PageID+(N>>8)+(uint8_t)N;
	SendCheck(temp);
	data=JudgeStr(2000);
	if(data)
		ensure=data[9];
	else
		ensure=0xff;
	return ensure;
}
//清空指纹库 GZ_Empty
//功能:  删除flash数据库中所有指纹模板
//参数:  无
//说明:  模块返回确认字
uint8_t GZ_Empty(void)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x03);
	Sendcmd(0x0D);
	temp = 0x01+0x03+0x0D;
	SendCheck(temp);
	data=JudgeStr(2000);
	if(data)
		ensure=data[9];
	else
		ensure=0xff;
	return ensure;
}
//写系统寄存器 GZ_WriteReg
//功能:  写模块寄存器
//参数:  寄存器序号RegNum:4\5\6
//说明:  模块返回确认字
uint8_t GZ_WriteReg(uint8_t RegNum,uint8_t DATA)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x05);
	Sendcmd(0x0E);
	Com_SendData(RegNum);
	Com_SendData(DATA);
	temp = RegNum+DATA+0x01+0x05+0x0E;
	SendCheck(temp);
	data=JudgeStr(2000);
	if(data)
		ensure=data[9];
	else
		ensure=0xff;
	return ensure;
}
//读系统基本参数 GZ_ReadSysPara
//功能: 读取模块的基本参数（波特率，包大小等)
//参数:  无
//说明: 模块返回确认字 + 基本参数（16bytes）
uint8_t GZ_ReadSysPara(SysPara *p)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x03);
	Sendcmd(0x0F);
	temp = 0x01+0x03+0x0F;
	SendCheck(temp);
	data=JudgeStr(1000);
	if(data)
	{
		ensure = data[9];
		p->GZ_max = (data[14]<<8)+data[15];
		p->GZ_level = data[17];
		p->GZ_addr=(data[18]<<24)+(data[19]<<16)+(data[20]<<8)+data[21];
		p->GZ_size = data[23];
		p->GZ_N = data[25];
	}		
	else
		ensure=0xff;
	return ensure;
}
//设置模块地址 GZ_SetAddr
//功能:  设置模块地址
//参数:  GZ_addr
//说明:  模块返回确认字
uint8_t GZ_SetAddr(uint32_t GZ_addr)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x07);
	Sendcmd(0x15);
	Com_SendData(GZ_addr>>24);
	Com_SendData(GZ_addr>>16);
	Com_SendData(GZ_addr>>8);
	Com_SendData(GZ_addr);
	temp = 0x01+0x07+0x15
	+(uint8_t)(GZ_addr>>24)+(uint8_t)(GZ_addr>>16)
	+(uint8_t)(GZ_addr>>8) +(uint8_t)GZ_addr;				
	SendCheck(temp);
	AS608Addr=GZ_addr;//发送完指令，更换地址
  data=JudgeStr(2000);
	if(data)
		ensure=data[9];
	else
		ensure=0xff;	
		AS608Addr = GZ_addr;
	if(ensure==0x00)//设置地址成功
	{
		
	}
	return ensure;
}
//功能： 模块内部为用户开辟了256bytes的FLASH空间用于存用户记事本,
//	该记事本逻辑上被分成 16 个页。
//参数:  NotePageNum(0~15),Byte32(要写入内容，32个字节)
//说明:  模块返回确认字
uint8_t GZ_WriteNotepad(uint8_t NotePageNum,uint8_t *Byte32)
{
	uint16_t temp;
  uint8_t  ensure,i;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(36);
	Sendcmd(0x18);
	Com_SendData(NotePageNum);
	for(i=0;i<32;i++)
	 {
		 Com_SendData(Byte32[i]);
		 temp += Byte32[i];
	 }
  temp =0x01+36+0x18+NotePageNum+temp;
	SendCheck(temp);
  data=JudgeStr(2000);
	if(data)
		ensure=data[9];
	else
		ensure=0xff;
	return ensure;
}
//读记事GZ_ReadNotepad
//功能： 读取FLASH用户区的128bytes数据
//参数:  NotePageNum(0~15)
//说明: 模块返回确认字+用户信息
uint8_t GZ_ReadNotepad(uint8_t NotePageNum,uint8_t *Byte32)
{
	uint16_t temp;
  uint8_t  ensure,i;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x04);
	Sendcmd(0x19);
	Com_SendData(NotePageNum);
	temp = 0x01+0x04+0x19+NotePageNum;
	SendCheck(temp);
  data=JudgeStr(2000);
	if(data)
	{
		ensure=data[9];
		for(i=0;i<32;i++)
		{
			Byte32[i]=data[10+i];
		}
	}
	else
		ensure=0xff;
	return ensure;
}
//高速搜索GZ_HighSpeedSearch
//功能：以 CharBuffer1或CharBuffer2中的特征文件高速搜索整个或部分指纹库。
//		  若搜索到，则返回页码,该指令对于的确存在于指纹库中 ，且登录时质量
//		  很好的指纹，会很快给出搜索结果。
//参数:  BufferID， StartPage(起始页)，PageNum（页数）
//说明:  模块返回确认字+页码（相配指纹模板）
uint8_t GZ_HighSpeedSearch(uint8_t BufferID,uint16_t StartPage,uint16_t PageNum,SearchResult *p)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x08);
	Sendcmd(0x1b);
	Com_SendData(BufferID);
	Com_SendData(StartPage>>8);
	Com_SendData(StartPage);
	Com_SendData(PageNum>>8);
	Com_SendData(PageNum);
	temp = 0x01+0x08+0x1b+BufferID
	+(StartPage>>8)+(uint8_t)StartPage
	+(PageNum>>8)+(uint8_t)PageNum;
	SendCheck(temp);
	data=JudgeStr(2000);
 	if(data)
	{
		ensure=data[9];
		p->pageID 	=(data[10]<<8) +data[11];
		p->mathscore=(data[12]<<8) +data[13];
	}
	else
		ensure=0xff;
    if(ensure == 0x00u)
    {
        AS608_LOG("HighSpeedSearch OK: id=%u score=%u", (unsigned)p->pageID, (unsigned)p->mathscore);
    }
    else
    {
        AS608_LOG("HighSpeedSearch fail: buffer=%u start=%u num=%u", (unsigned)BufferID, (unsigned)StartPage, (unsigned)PageNum);
    }
    as608_log_result("HighSpeedSearch", ensure);
	return ensure;
}
//读有效模板个数 GZ_ValidTempleteNum
//功能：读有效模板个数
//参数: 无
//说明: 模块返回确认字+有效模板个数ValidN
uint8_t GZ_ValidTempleteNum(uint16_t *ValidN)
{
	uint16_t temp;
  uint8_t  ensure;
	uint8_t  *data;
	SendHead();
	SendAddr();
	SendFlag(0x01);//命令包标识
	SendLength(0x03);
	Sendcmd(0x1d);
	temp = 0x01+0x03+0x1d;
	SendCheck(temp);
  data=JudgeStr(2000);
	if(data)
	{
		ensure=data[9];
		if(ValidN != NULL)
		{
			if(ensure == 0x00u)
			{
				*ValidN = (uint16_t)(((uint16_t)data[10] << 8) | data[11]);
			}
			else
			{
				*ValidN = 0u;
			}
		}
	}
	else
		ensure=0xff;
	
	return ensure;
}
//与AS608握手 GZ_HandShake
//参数: GZ_Addr地址指针
//说明: 模块返新地址（正确地址）	
uint8_t GZ_HandShake(uint32_t *GZ_Addr)
{
	uint16_t wait;
	SendHead();
	SendAddr();
	Com_SendData(0X01);
	Com_SendData(0X00);
	Com_SendData(0X00);
	/* AS608 握手应答包共 12 字节(EF 01 + 4B addr + 07 + 2B len + confirm + 2B chk)，
	 * 57600 baud 下整包 ~2ms。
	 * 之前是固定 HAL_Delay(200)，模块没插也要傻等满 200ms。
	 * 现在等到整包到齐(s_as608_rx_widx>=12)即可退出，模块在线 ~5ms 返回；
	 * 模块不在线则等满 200ms 再判定失败——保证 aRxBuffer[6] 一定读到的是真正的字节6而不是缓冲区残留。 */
	for(wait = 0u; wait < 200u; wait++) {
		as608_uart_poll_rx();
		HAL_Delay(1);
		if(s_as608_rx_widx >= 12u) break;
	}
	if(s_as608_rx_widx >= 12u || RX_len)
	{
		RX_len=0;
		if(//判断是不是模块返回的应答包
				aRxBuffer[0]==0XEF
			&&aRxBuffer[1]==0X01
			&&aRxBuffer[6]==0X07
			)
			{
				*GZ_Addr=(aRxBuffer[2]<<24) + (aRxBuffer[3]<<16)
								+(aRxBuffer[4]<<8) + (aRxBuffer[5]);
				return 0;
			}

	}
	return 1;		
}

static uint16_t as608_pkt_bytes_from_code(uint8_t code)
{
    if(code == 0u) {
        return 32u;
    }
    if(code == 1u) {
        return 64u;
    }
    if(code == 2u) {
        return 128u;
    }
    if(code == 3u) {
        return 256u;
    }
    return 32u;
}

static uint16_t as608_pkg_checksum(uint8_t pid, uint16_t len_field, const uint8_t *data, uint16_t data_len)
{
    uint16_t sum = (uint16_t)pid + (uint16_t)(len_field >> 8) + (uint16_t)(len_field & 0xFFu);
    uint16_t i;

    for(i = 0u; i < data_len; i++) {
        sum += (uint16_t)data[i];
    }
    return sum;
}

static uint8_t as608_send_packet_bulk(uint8_t pid, const uint8_t *data, uint16_t data_len)
{
    uint8_t pkt[9u + 256u + 2u];
    uint16_t len_field = (uint16_t)(data_len + 2u);
    uint16_t sum;
    uint16_t i;
    uint16_t n;

    if(data_len > 256u) {
        return 0u;
    }

    pkt[0] = 0xEFu;
    pkt[1] = 0x01u;
    pkt[2] = (uint8_t)(AS608Addr >> 24);
    pkt[3] = (uint8_t)(AS608Addr >> 16);
    pkt[4] = (uint8_t)(AS608Addr >> 8);
    pkt[5] = (uint8_t)AS608Addr;
    pkt[6] = pid;
    pkt[7] = (uint8_t)(len_field >> 8);
    pkt[8] = (uint8_t)len_field;

    for(i = 0u; i < data_len; i++) {
        pkt[9u + i] = data[i];
    }

    sum = as608_pkg_checksum(pid, len_field, data, data_len);
    pkt[9u + data_len] = (uint8_t)(sum >> 8);
    pkt[10u + data_len] = (uint8_t)sum;
    n = (uint16_t)(11u + data_len);

    return (HAL_UART_Transmit(&g_uart1_handle, pkt, n, 300u) == HAL_OK) ? 1u : 0u;
}

static uint8_t as608_wait_rx_grow(uint16_t min_bytes, uint16_t ms)
{
    uint16_t last = 0u;
    uint16_t stable = 0u;

    while(ms-- != 0u) {
        as608_uart_poll_rx();
        HAL_Delay(1);
        if(s_as608_rx_widx >= min_bytes) {
            if(s_as608_rx_widx == last) {
                stable++;
                if(stable >= 5u) {
                    return 1u;
                }
            } else {
                last = s_as608_rx_widx;
                stable = 0u;
            }
        }
    }
    return (s_as608_rx_widx >= min_bytes) ? 1u : 0u;
}

static uint8_t as608_find_ack_confirm(uint8_t expect_cmd, uint8_t *confirm_out)
{
    uint16_t i;

    for(i = 0u; i + 12u <= s_as608_rx_widx; i++) {
        if((aRxBuffer[i] == 0xEFu) && (aRxBuffer[i + 1u] == 0x01u) && (aRxBuffer[i + 6u] == 0x07u)) {
            if(expect_cmd != 0u && aRxBuffer[i + 9u] != expect_cmd && aRxBuffer[i + 9u] != 0x00u) {
                continue;
            }
            if(confirm_out != NULL) {
                *confirm_out = aRxBuffer[i + 9u];
            }
            return 0u;
        }
    }
    return 0xFFu;
}

uint8_t GZ_LoadChar(uint8_t BufferID, uint16_t PageID)
{
    uint16_t temp;
    uint8_t *data;

    SendHead();
    SendAddr();
    SendFlag(0x01u);
    SendLength(0x06);
    Sendcmd(0x07u);
    Com_SendData(BufferID);
    Com_SendData((uint8_t)(PageID >> 8));
    Com_SendData((uint8_t)PageID);
    temp = (uint16_t)(0x01u + 0x06u + 0x07u + BufferID + (PageID >> 8) + (PageID & 0xFFu));
    SendCheck(temp);
    data = JudgeStr(2000);
    if(data != NULL) {
        return data[9];
    }
    return 0xFFu;
}

uint8_t GZ_UpCharBuffer(uint8_t BufferID, uint8_t *tpl_buf, uint16_t tpl_buf_sz)
{
    uint16_t pos = 0u;
    uint16_t temp;
    uint8_t *data;
    uint16_t wait_ms = 4000u;
    uint16_t i;
    uint16_t plen;
    uint16_t dlen;
    uint16_t pkt_len;

    if(tpl_buf == NULL || tpl_buf_sz < AS608_TEMPLATE_SIZE) {
        return 0xFFu;
    }

    as608_rx_buffer_clear();
    SendHead();
    SendAddr();
    SendFlag(0x01u);
    SendLength(0x04);
    Sendcmd(0x08u);
    Com_SendData(BufferID);
    temp = (uint16_t)(0x01u + 0x04u + 0x08u + BufferID);
    SendCheck(temp);

    data = JudgeStr(2000);
    if(data == NULL || data[9] != 0x00u) {
        return (data != NULL) ? data[9] : 0xFFu;
    }

    while(pos < AS608_TEMPLATE_SIZE && wait_ms > 0u) {
        as608_uart_poll_rx();
        for(i = 0u; i + 12u <= s_as608_rx_widx; i++) {
            if((aRxBuffer[i] != 0xEFu) || (aRxBuffer[i + 1u] != 0x01u)) {
                continue;
            }
            if(aRxBuffer[i + 6u] == 0x02u) {
                plen = ((uint16_t)aRxBuffer[i + 7u] << 8) | (uint16_t)aRxBuffer[i + 8u];
                dlen = (plen >= 2u) ? (uint16_t)(plen - 2u) : 0u;
                pkt_len = (uint16_t)(9u + dlen + 2u);
                if((i + pkt_len) > s_as608_rx_widx) {
                    break;
                }
                if((pos + dlen) > AS608_TEMPLATE_SIZE) {
                    return 0xFFu;
                }
                memcpy(tpl_buf + pos, &aRxBuffer[i + 9u], dlen);
                pos += dlen;
                {
                    uint16_t tail = (uint16_t)(s_as608_rx_widx - (i + pkt_len));
                    if(tail > 0u) {
                        memmove(aRxBuffer, aRxBuffer + i + pkt_len, tail);
                    }
                    s_as608_rx_widx = tail;
                    aRxBuffer[s_as608_rx_widx] = 0u;
                }
                i = 0u;
                continue;
            }
            if(aRxBuffer[i + 6u] == 0x07u) {
                uint8_t confirm = aRxBuffer[i + 9u];
                if((confirm == 0x08u || confirm == 0x00u) && pos >= AS608_TEMPLATE_SIZE) {
                    return 0x00u;
                }
                if(confirm != 0x00u && confirm != 0x08u) {
                    return confirm;
                }
            }
        }
        HAL_Delay(1);
        wait_ms--;
    }
    return (pos >= AS608_TEMPLATE_SIZE) ? 0x00u : 0xFFu;
}

uint8_t GZ_ReadTemplatePage(uint16_t page_id, uint8_t *tpl_buf, uint16_t tpl_buf_sz)
{
    uint8_t en;

    if(tpl_buf == NULL || tpl_buf_sz < AS608_TEMPLATE_SIZE) {
        return 0xFFu;
    }

    as608_rx_buffer_clear();
    en = GZ_LoadChar(CharBuffer1, page_id);
    if(en != 0x00u) {
        return en;
    }
    HAL_Delay(20);
    as608_rx_buffer_clear();
    return GZ_UpCharBuffer(CharBuffer1, tpl_buf, tpl_buf_sz);
}

uint8_t GZ_WriteTemplatePage(uint16_t page_id, const uint8_t *tpl_buf, uint16_t tpl_len)
{
    uint8_t en;
    uint16_t off;
    uint16_t chunk;
    uint8_t *data;
    SysPara sp;
    uint16_t pkt_bytes = 32u;

    if(tpl_buf == NULL || tpl_len != AS608_TEMPLATE_SIZE) {
        s_tpl_wr_last_stage = AS608_TPL_STAGE_NONE;
        s_tpl_wr_last_code = 0xFFu;
        s_tpl_wr_last_rx = s_as608_rx_widx;
        return 0xFFu;
    }
    s_tpl_wr_last_stage = AS608_TPL_STAGE_NONE;
    s_tpl_wr_last_code = 0x00u;
    s_tpl_wr_last_rx = 0u;

    as608_rx_buffer_clear();
    if(GZ_ReadSysPara(&sp) == 0x00u) {
        pkt_bytes = as608_pkt_bytes_from_code(sp.GZ_size);
    }
    if(pkt_bytes == 0u || (AS608_TEMPLATE_SIZE % pkt_bytes) != 0u) {
        pkt_bytes = 32u;
    }

    as608_rx_buffer_clear();
    SendHead();
    SendAddr();
    SendFlag(0x01u);
    SendLength(0x04);
    Sendcmd(0x09u);
    Com_SendData(CharBuffer1);
    {
        uint16_t temp = (uint16_t)(0x01u + 0x04u + 0x09u + CharBuffer1);
        SendCheck(temp);
    }
    data = JudgeStr(2000);
    if(data == NULL || data[9] != 0x00u) {
        s_tpl_wr_last_stage = AS608_TPL_STAGE_START_CMD;
        s_tpl_wr_last_code = (data != NULL) ? data[9] : 0xFFu;
        s_tpl_wr_last_rx = s_as608_rx_widx;
        return (data != NULL) ? data[9] : 0xFFu;
    }

    for(off = 0u; off < AS608_TEMPLATE_SIZE; off += pkt_bytes) {
        uint8_t pid = 0x02u;
        chunk = pkt_bytes;
        if((off + chunk) >= AS608_TEMPLATE_SIZE) {
            pid = 0x08u; /* last data packet */
        }
        if(as608_send_packet_bulk(pid, tpl_buf + off, chunk) == 0u) {
            s_tpl_wr_last_stage = AS608_TPL_STAGE_DATA_ACK;
            s_tpl_wr_last_code = 0xFFu;
            s_tpl_wr_last_rx = s_as608_rx_widx;
            return 0xFFu;
        }
        HAL_Delay(8);
    }
    if(as608_wait_rx_grow(12u, 1200u) == 0u) {
        s_tpl_wr_last_stage = AS608_TPL_STAGE_DATA_ACK;
        s_tpl_wr_last_code = 0xFFu;
        s_tpl_wr_last_rx = s_as608_rx_widx;
        return 0xFFu;
    }
    if(as608_find_ack_confirm(0x00u, &en) != 0u) {
        if(as608_find_ack_confirm(0x08u, &en) != 0u) {
            s_tpl_wr_last_stage = AS608_TPL_STAGE_DATA_ACK;
            s_tpl_wr_last_code = 0xFFu;
            s_tpl_wr_last_rx = s_as608_rx_widx;
            return 0xFFu;
        }
    }
    if(en != 0x00u && en != 0x08u) {
        s_tpl_wr_last_stage = AS608_TPL_STAGE_DATA_ACK;
        s_tpl_wr_last_code = en;
        s_tpl_wr_last_rx = s_as608_rx_widx;
        return en;
    }

    as608_rx_buffer_clear();
    HAL_Delay(60);
    en = GZ_StoreChar(CharBuffer1, page_id);
    s_tpl_wr_last_stage = AS608_TPL_STAGE_STORE_CHAR;
    s_tpl_wr_last_code = en;
    s_tpl_wr_last_rx = s_as608_rx_widx;
    as608_rx_buffer_clear();
    return en;
}

//模块应答包确认码信息解析
//功能：解析确认码错误信息返回信息
//参数: ensure
const char *EnsureMessage(uint8_t ensure) 
{
	const char *p;
	switch(ensure)
	{
		case  0x00:
			p="OK";break;		
		case  0x01:
			p="数据包接收错误";break;
		case  0x02:
			p="传感器上没有手指";break;
		case  0x03:
			p="录入指纹图像失败";break;
		case  0x04:
			p="指纹图像太干、太淡而生不成特征";break;
		case  0x05:
			p="指纹图像太湿、太糊而生不成特征";break;
		case  0x06:
			p="指纹图像太乱而生不成特征";break;
		case  0x07:
			p="指纹图像正常，但特征点太少（或面积太小）而生不成特征";break;
		case  0x08:
			p="指纹不匹配";break;
		case  0x09:
			p="没搜索到指纹";break;
		case  0x0a:
			p="特征合并失败";break;
		case  0x0b:
			p="访问指纹库时地址序号超出指纹库范围";break;
		case  0x17:
			p="特征缓冲区无效或模板损坏";break;
		case  0x10:
			p="删除模板失败";break;
		case  0x11:
			p="清空指纹库失败";break;	
		case  0x15:
			p="缓冲区内没有有效原始图而生不成图像";break;
		case  0x18:
			p="读写 FLASH 出错";break;
		case  0x19:
			p="未定义错误";break;
		case  0x1a:
			p="无效寄存器号";break;
		case  0x1b:
			p="寄存器设定内容错误";break;
		case  0x1c:
			p="记事本页码指定错误";break;
		case  0x1f:
			p="指纹库满";break;
		case  0x20:
			p="地址错误";break;
		default :
			p="模块返回确认码有误";break;
	}
 return p;	
}



