#include "app_nfc_hw.h"

#include <stdio.h>

#include "./SYSTEM/delay/delay.h"
#include "../Drivers/BSP/mfrc522/MFRC522.h"

#define NFCHW_LOG(fmt, ...) ((void)0)

/* MFRC522 VersionReg 地址（datasheet 9.3.4.8），用来确认芯片活没活：
 * 0x91=v1.0, 0x92=v2.0；读到 0x00/0xFF 说明 SPI 通信坏掉了。 */
#define MFRC522_VERSION_REG 0x37

static uint8_t s_nfc_hw_ready = 0u;
static uint8_t s_nfc_last_reset_ret = 0u;
static uint8_t s_nfc_last_version = 0u;

void app_nfc_hw_wakeup_no_reset(void)
{
    MFRC522_Init();
    delay_ms(2);
    MFRC522_AntennaOn();
    Flash_ReadID();
    s_nfc_hw_ready = 1u;
}

/* 强力硬件复位：自己直接控制 RST 引脚（不走 MFRC522_Reset 里那个 1us 的不可靠时序）。
 * datasheet 要求 RST 拉低维持时间最低 100ns，但拉高后晶振至少 37.74us 才稳定。
 * 时间是按"覆盖任何模组的电源/晶振边界情况"+"运行时调用不至于把 LVGL 卡到看不出"折中。 */
static void mfrc522_strong_hw_reset(uint8_t cold)
{
    MFRC522_RST_H;
    delay_ms(1);
    MFRC522_RST_L;
    delay_ms(cold ? 10u : 5u);   /* 冷启动给 10ms，运行时只给 5ms 减少阻塞 */
    MFRC522_RST_H;
    delay_ms(cold ? 50u : 10u);  /* 冷启动给 50ms 等晶振；运行时 10ms 已足够 */
}

/* 内部函数：执行一次完整的"硬复位 + 软复位 + 关键寄存器配齐"流程。
 * cold=1 表示是冷启动，多给一些稳定时间；cold=0 是运行中救活，尽量轻。 */
static void mfrc522_full_reinit(uint8_t cold)
{
    uint8_t i;
    char ret = (char)(-1);
    uint8_t ver = 0u;

    MFRC522_Init();
    /* 冷启动给 RC522 内部 27.12MHz 晶振充分的上电稳定时间；运行时已稳定，跳过。 */
    if(cold) {
        delay_ms(50);
    }

    mfrc522_strong_hw_reset(cold);

    /* 立即读 VersionReg 校验芯片是否真的活着。任何 != 0x91/0x92 都说明 SPI/电源/接线异常。 */
    ver = Read_MFRC522(MFRC522_VERSION_REG);
    s_nfc_last_version = ver;
    NFCHW_LOG("strong_reset done version=0x%02X (expect 0x91/0x92)", ver);

    {
        uint8_t max_attempts = cold ? 5u : 2u;
        uint8_t retry_delay = cold ? 20u : 5u;
        for(i = 0u; i < max_attempts; i++) {
            ret = MFRC522_Reset();
            if(ret == MI_OK) {
                MFRC522_AntennaOn();
                if(cold) delay_ms(5);
                break;
            }
            delay_ms(retry_delay);
        }
        NFCHW_LOG("soft_reset attempts=%u ret=%d", (unsigned)(i + 1u), (int)ret);
    }

    s_nfc_last_reset_ret = (uint8_t)ret;
    if(ret == MI_OK) {
        MFRC522_AntennaOn();
        /* 关键补强：把 RxGain 拉到最大（0x70 = 48dB）。
         * MFRC522 默认 RxGain 偏小，冷启动后经常出现"芯片活着但收不到卡的回波"，
         * 表现就是 Request 返回 MI_ERR（ErrorReg 报 ProtocolErr/ParityErr）。
         * 很多 RC522 模组都需要把 RFCfgReg 拉到最大档才能稳定读卡。 */
        Write_MFRC522(0x26u, 0x70u);   /* RFCfgReg: RxGain[6:4]=111 = 48dB */
        /* 再保险一次 Force100ASK（TxASKReg=0x15, bit6=1） */
        Write_MFRC522(0x15u, 0x40u);
        Flash_ReadID();
        /* 复位后再读一次 VersionReg + TxControlReg + RFCfgReg，确认配置真的落地。 */
        {
            uint8_t ver2 = Read_MFRC522(MFRC522_VERSION_REG);
            uint8_t tx   = Read_MFRC522(0x14u); /* TxControlReg */
            uint8_t rf   = Read_MFRC522(0x26u); /* RFCfgReg */
            uint8_t ask  = Read_MFRC522(0x15u); /* TxASKReg */
            NFCHW_LOG("post_init version=0x%02X tx=0x%02X rf=0x%02X ask=0x%02X", ver2, tx, rf, ask);
        }
        s_nfc_hw_ready = 1u;
    } else {
        s_nfc_hw_ready = 0u;
    }
}

void app_nfc_hw_init_once(void)
{
    mfrc522_full_reinit(1u);   /* cold = 1 */
}

void app_nfc_hw_deep_recover(void)
{
    NFCHW_LOG("deep_recover begin (light)");
    mfrc522_full_reinit(0u);   /* cold = 0：运行时调用，给 LVGL 留时间 */
    NFCHW_LOG("deep_recover done ready=%u", s_nfc_hw_ready);
}

uint8_t app_nfc_hw_ready(void)
{
    return s_nfc_hw_ready;
}

uint8_t app_nfc_last_reset_ret(void)
{
    return s_nfc_last_reset_ret;
}
