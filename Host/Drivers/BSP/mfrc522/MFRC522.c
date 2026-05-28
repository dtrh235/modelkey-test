#include "MFRC522.h"

#define MAXRLEN 18
#define RC522_DELAY()  delay_us(2)

void MFRC522_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    g.Pin = MFRC522_GPIO_SDA_PIN;
    HAL_GPIO_Init(MFRC522_GPIO_SDA_PORT, &g);

    g.Pin = MFRC522_GPIO_SCK_PIN;
    HAL_GPIO_Init(MFRC522_GPIO_SCK_PORT, &g);

    g.Pin = MFRC522_GPIO_MOSI_PIN;
    HAL_GPIO_Init(MFRC522_GPIO_MOSI_PORT, &g);

    g.Pin = MFRC522_GPIO_MISO_PIN;
    g.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(MFRC522_GPIO_MISO_PORT, &g);

    g.Pin = MFRC522_GPIO_RST_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(MFRC522_GPIO_RST_PORT, &g);
}

// ????(??)
unsigned char Read_MFRC522(unsigned char Address)
{
    unsigned char i, ucAddr;
    unsigned char ucResult=0;

    MFRC522_SCK_L;
    RC522_DELAY();
    MFRC522_SDA_L;
    RC522_DELAY();
    ucAddr = ((Address<<1)&0x7E)|0x80;

    for(i=8;i>0;i--)
    {
        if(ucAddr&0x80) MFRC522_MOSI_H;
        else MFRC522_MOSI_L;
        RC522_DELAY();
        MFRC522_SCK_H;
        RC522_DELAY();
        ucAddr <<= 1;
        MFRC522_SCK_L;
        RC522_DELAY();
    }

    for(i=8;i>0;i--)
    {
        MFRC522_SCK_H;
        RC522_DELAY();
        ucResult <<= 1;
        ucResult |= MFRC522_MISO_READ;
        MFRC522_SCK_L;
        RC522_DELAY();
    }

    MFRC522_SCK_H;
    RC522_DELAY();
    MFRC522_SDA_H;
    RC522_DELAY();
    return ucResult;
}

// ????(??)
void Write_MFRC522(unsigned char Address, unsigned char value)
{
    unsigned char i, ucAddr;

    MFRC522_SCK_L;
    MFRC522_SDA_L;
    ucAddr = ((Address<<1)&0x7E);

    for(i=8;i>0;i--)
    {
        if(ucAddr&0x80) MFRC522_MOSI_H;
        else MFRC522_MOSI_L;
        RC522_DELAY();
        MFRC522_SCK_H;
        RC522_DELAY();
        ucAddr <<= 1;
        MFRC522_SCK_L;
        RC522_DELAY();
    }

    for(i=8;i>0;i--)
    {
        if(value&0x80) MFRC522_MOSI_H;
        else MFRC522_MOSI_L;
        RC522_DELAY();
        MFRC522_SCK_H;
        RC522_DELAY();
        value <<= 1;
        MFRC522_SCK_L;
        RC522_DELAY();
    }

    MFRC522_SCK_H;
    RC522_DELAY();
    MFRC522_SDA_H;
    RC522_DELAY();
}

// ??(??)
char MFRC522_Reset(void)
{
    uint32_t retry = 1000u;
    MFRC522_RST_H;
    delay_us(1);
    MFRC522_RST_L;
    delay_us(1);
    MFRC522_RST_H;
    delay_us(1);

    Write_MFRC522(CommandReg,0x0F);
    while((Read_MFRC522(CommandReg) & 0x10u) && (retry-- > 0u)) {
        delay_us(5);
    }
    if(retry == 0u) {
        return MI_ERR;
    }

    delay_us(1);

    Write_MFRC522(ModeReg,0x3D);
    Write_MFRC522(TReloadRegL,30);
    Write_MFRC522(TReloadRegH,0);
    Write_MFRC522(TModeReg,0x8D);
    Write_MFRC522(TPrescalerReg,0x3E);
    Write_MFRC522(TxAutoReg,0x40);
    return MI_OK;
}

// ????(??)
void SetBitMask(unsigned char reg,unsigned char mask)
{
    char tmp = 0x0;
    tmp = Read_MFRC522(reg);
    Write_MFRC522(reg,tmp | mask);
}

// ????(??)
void ClearBitMask(unsigned char reg,unsigned char mask)
{
    char tmp = 0x0;
    tmp = Read_MFRC522(reg);
    Write_MFRC522(reg, tmp & ~mask);
}

// ????(??)
char MFRC522_ToCard(unsigned char Command,
                 unsigned char *pInData,
                 unsigned char InLenByte,
                 unsigned char *pOutData,
                 unsigned int  *pOutLenBit)
{
    char status = MI_ERR;
    unsigned char irqEn   = 0x00;
    unsigned char waitFor = 0x00;
    unsigned char lastBits;
    unsigned char n;
    unsigned int i;
    switch (Command)
    {
       case PCD_AUTHENT:
          irqEn   = 0x12;
          waitFor = 0x10;
          break;
       case PCD_TRANSCEIVE:
          irqEn   = 0x77;
          waitFor = 0x30;
          break;
       default:
         break;
    }

    Write_MFRC522(ComIEnReg,irqEn|0x80);
    ClearBitMask(ComIrqReg,0x80);
    Write_MFRC522(CommandReg,PCD_IDLE);
    SetBitMask(FIFOLevelReg,0x80);

    for (i=0; i<InLenByte; i++)
    {
        Write_MFRC522(FIFODataReg, pInData[i]);
    }
    Write_MFRC522(CommandReg, Command);

    if (Command == PCD_TRANSCEIVE)
    {
        SetBitMask(BitFramingReg,0x80);
    }

    i = 1500;
    do
    {
         n = Read_MFRC522(ComIrqReg);
         i--;
    }
    while ((i!=0) && !(n&0x01) && !(n&waitFor));
    ClearBitMask(BitFramingReg,0x80);

    if (i!=0)
    {
         if(!(Read_MFRC522(ErrorReg)&0x1B))
         {
             status = MI_OK;
             if (n & irqEn & 0x01)
             {   status = MI_NOTAGERR;   }
             if (Command == PCD_TRANSCEIVE)
             {
                n = Read_MFRC522(FIFOLevelReg);
                lastBits = Read_MFRC522(ControlReg) & 0x07;
                if (lastBits)
                {   *pOutLenBit = (n-1)*8 + lastBits;   }
                else
                {   *pOutLenBit = n*8;   }
                if (n == 0)
                {   n = 1;    }
                if (n > MAXRLEN)
                {   n = MAXRLEN;   }
                for (i=0; i<n; i++)
                {   pOutData[i] = Read_MFRC522(FIFODataReg);    }
            }
         }
         else
         {
            status = MI_ERR;
         }
    }

    SetBitMask(ControlReg,0x80);
    Write_MFRC522(CommandReg,PCD_IDLE);
    return status;
}

// ????(??)
void MFRC522_AntennaOn(void)
{
    unsigned char i;
    i = Read_MFRC522(TxControlReg);
    if (!(i & 0x03))
    {
        SetBitMask(TxControlReg, 0x03);
    }
}

// ????(??)
void MFRC522_AntennaOff(void)
{
    ClearBitMask(TxControlReg, 0x03);
}

// CRC??(??)
void CalulateCRC(unsigned char *pIndata,unsigned char len,unsigned char *pOutData)
{
    unsigned char i,n;
    ClearBitMask(DivIrqReg,0x04);
    Write_MFRC522(CommandReg,PCD_IDLE);
    SetBitMask(FIFOLevelReg,0x80);
    for (i=0; i<len; i++)
    {   Write_MFRC522(FIFODataReg, *(pIndata+i));   }
    Write_MFRC522(CommandReg, PCD_CALCCRC);
    i = 0xFF;
    do
    {
        n = Read_MFRC522(DivIrqReg);
        i--;
    }
    while ((i!=0) && !(n&0x04));
    pOutData[0] = Read_MFRC522(CRCResultRegL);
    pOutData[1] = Read_MFRC522(CRCResultRegM);
}

// ??(??)
char MFRC522_Halt(void)
{
    unsigned int  unLen;
    unsigned char ucComMF522Buf[MAXRLEN];
    char status;
    ucComMF522Buf[0] = PICC_HALT;
    ucComMF522Buf[1] = 0;
    CalulateCRC(ucComMF522Buf,2,&ucComMF522Buf[2]);

    status = MFRC522_ToCard(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &unLen);
    return status;
}


// ??(??)
char MFRC522_Request(unsigned char req_code,unsigned char *pTagType)
{
   char status;
   unsigned int  unLen;
   unsigned char ucComMF522Buf[MAXRLEN];

   ClearBitMask(Status2Reg,0x08);
   Write_MFRC522(BitFramingReg,0x07);
   SetBitMask(TxControlReg,0x03);

   ucComMF522Buf[0] = req_code;

   status = MFRC522_ToCard(PCD_TRANSCEIVE,ucComMF522Buf,1,ucComMF522Buf,&unLen);

   if ((status == MI_OK) && (unLen == 0x10))
   {
       *pTagType     = ucComMF522Buf[0];
       *(pTagType+1) = ucComMF522Buf[1];
   }
   else
   {   status = MI_ERR;  }

   return status;
}

// ???(??)
char MFRC522_Anticoll(unsigned char *pSnr)
{
    char status;
    unsigned char i,snr_check=0;
    unsigned int  unLen;
    unsigned char ucComMF522Buf[MAXRLEN];

    ClearBitMask(Status2Reg,0x08);
    Write_MFRC522(BitFramingReg,0x00);
    ClearBitMask(CollReg,0x80);

    ucComMF522Buf[0] = PICC_ANTICOLL1;
    ucComMF522Buf[1] = 0x20;

    status = MFRC522_ToCard(PCD_TRANSCEIVE,ucComMF522Buf,2,ucComMF522Buf,&unLen);

    if (status == MI_OK)
    {
    	 for (i=0; i<4; i++)
         {
             *(pSnr+i)  = ucComMF522Buf[i];
             snr_check ^= ucComMF522Buf[i];
         }
         if (snr_check != ucComMF522Buf[i])
         {   status = MI_ERR;    }
    }

    SetBitMask(CollReg,0x80);
    return status;
}

// ??(??)
char MFRC522_SelectTag(unsigned char *pSnr)
{
    char status;
    unsigned char i;
    unsigned int  unLen;
    unsigned char ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = PICC_ANTICOLL1;
    ucComMF522Buf[1] = 0x70;
    ucComMF522Buf[6] = 0;
    for (i=0; i<4; i++)
    {
    	ucComMF522Buf[i+2] = *(pSnr+i);
    	ucComMF522Buf[6]  ^= *(pSnr+i);
    }
    CalulateCRC(ucComMF522Buf,7,&ucComMF522Buf[7]);

    ClearBitMask(Status2Reg,0x08);

    status = MFRC522_ToCard(PCD_TRANSCEIVE,ucComMF522Buf,9,ucComMF522Buf,&unLen);

    if ((status == MI_OK) && (unLen == 0x18))
    {   status = MI_OK;  }
    else
    {   status = MI_ERR;    }

    return status;
}

// ????(??)
char MFRC522_AuthState(unsigned char auth_mode,unsigned char addr,unsigned char *pKey,unsigned char *pSnr)
{
    char status;
    unsigned int  unLen;
    unsigned char i,ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = auth_mode;
    ucComMF522Buf[1] = addr;
    for (i=0; i<6; i++)
    {    ucComMF522Buf[i+2] = *(pKey+i);   }
    for (i=0; i<6; i++)
    {    ucComMF522Buf[i+8] = *(pSnr+i);   }

    status = MFRC522_ToCard(PCD_AUTHENT,ucComMF522Buf,12,ucComMF522Buf,&unLen);
    if ((status != MI_OK) || (!(Read_MFRC522(Status2Reg) & 0x08)))
    {   status = MI_ERR;   }

    return status;
}

// ??(??)
char MFRC522_Read(unsigned char addr,unsigned char *pData)
{
    char status;
    unsigned int  unLen;
    unsigned char i,ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = PICC_READ;
    ucComMF522Buf[1] = addr;
    CalulateCRC(ucComMF522Buf,2,&ucComMF522Buf[2]);

    status = MFRC522_ToCard(PCD_TRANSCEIVE,ucComMF522Buf,4,ucComMF522Buf,&unLen);
    if ((status == MI_OK) && (unLen == 0x90))
    {
        for (i=0; i<16; i++)
        {    *(pData+i) = ucComMF522Buf[i];   }
    }
    else
    {   status = MI_ERR;   }

    return status;
}

// ??(??)
char MFRC522_Write(unsigned char addr,unsigned char *pData)
{
    char status;
    unsigned int  unLen;
    unsigned char i,ucComMF522Buf[MAXRLEN];

    ucComMF522Buf[0] = PICC_WRITE;
    ucComMF522Buf[1] = addr;
    CalulateCRC(ucComMF522Buf,2,&ucComMF522Buf[2]);

    status = MFRC522_ToCard(PCD_TRANSCEIVE,ucComMF522Buf,4,ucComMF522Buf,&unLen);

    if ((status != MI_OK) || (unLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A))
    {   status = MI_ERR;   }

    if (status == MI_OK)
    {
        for (i=0; i<16; i++)
        {    ucComMF522Buf[i] = *(pData+i);   }
        CalulateCRC(ucComMF522Buf,16,&ucComMF522Buf[16]);

        status = MFRC522_ToCard(PCD_TRANSCEIVE,ucComMF522Buf,18,ucComMF522Buf,&unLen);
        if ((status != MI_OK) || (unLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A))
        {   status = MI_ERR;   }
    }

    return status;
}
