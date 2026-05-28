#include "string.h"
#include "app_config.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/TOUCH/ctiic.h"
#include "./BSP/TOUCH/ft5206.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"


/**
 * @brief       ??FT5206��?????????
 * @param       reg : ???????????
 * @param       buf : ???????????
 * @param       len : ��???????
 * @retval      0, ???; 1, ???;
 */
uint8_t ft5206_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    uint8_t ret = 0;
    
    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_WR);    /* ????��???? */
    ct_iic_wait_ack();
    ct_iic_send_byte(reg & 0XFF);       /* ?????8��??? */
    ct_iic_wait_ack();

    for (i = 0; i < len; i++)
    {
        ct_iic_send_byte(buf[i]);       /* ?????? */
        ret = ct_iic_wait_ack();

        if (ret)break;
    }

    ct_iic_stop();  /* ????????????? */
    return ret;
}

/**
 * @brief       ??FT5206???????????
 * @param       reg : ???????????
 * @param       buf : ???????????
 * @param       len : ?????????
 * @retval      ??
 */
void ft5206_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    
    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_WR);    /* ????��???? */
    ct_iic_wait_ack();
    ct_iic_send_byte(reg & 0XFF);       /* ?????8��??? */
    ct_iic_wait_ack();
    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_RD);    /* ????????? */
    ct_iic_wait_ack();

    for (i = 0; i < len; i++)
    {
        buf[i] = ct_iic_read_byte(i == (len - 1) ? 0 : 1);  /* ??????? */
    }

    ct_iic_stop();  /* ????????????? */
}

static void ft5206_reset_config(void)
{
    uint8_t temp[2];

    FT5206_RST(0);
    delay_ms(20);
    FT5206_RST(1);
    delay_ms(50);
    temp[0] = 0;
    ft5206_wr_reg(FT5206_DEVIDE_MODE, temp, 1);
    ft5206_wr_reg(FT5206_ID_G_MODE, temp, 1);
    temp[0] = 22;
    ft5206_wr_reg(FT5206_ID_G_THGROUP, temp, 1);
    temp[0] = 12;
    ft5206_wr_reg(FT5206_ID_G_PERIODACTIVE, temp, 1);
}

static uint8_t ft5206_chip_ok(void)
{
    uint8_t id = 0u;

    if(ct_iic_probe_wr(FT5206_CMD_WR) != 0u) {
        return 0u;
    }
    if(ct_iic_pin_read_sda() == 0u) {
        return 0u;
    }
    ft5206_rd_reg(FT5206_CHIP_ID_REG, &id, 1);
    if(id == 0x51u || id == 0x33u || id == 0x64u) {
        return 1u;
    }
    /* 部分模组上电读 A3=0x00，但 I2C 已 ACK，扫描可出坐标 */
    return 1u;
}

static uint8_t ft5206_try_bus(GPIO_TypeDef *scl_port, uint16_t scl_pin,
                              GPIO_TypeDef *sda_port, uint16_t sda_pin, uint8_t bus_id)
{
    ct_iic_bind(scl_port, scl_pin, sda_port, sda_pin, bus_id);
    ct_iic_init();
    ft5206_reset_config();
    return ft5206_chip_ok();
}

/**
 * @brief       ?????FT5206??????
 * @param       ??
 * @retval      0, ????????; 1, ????????;
 */
uint8_t ft5206_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    FT5206_RST_GPIO_CLK_ENABLE();
    FT5206_INT_GPIO_CLK_ENABLE();

    gpio_init_struct.Pin = FT5206_RST_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(FT5206_RST_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = FT5206_INT_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(FT5206_INT_GPIO_PORT, &gpio_init_struct);

    if(ft5206_try_bus(GPIOB, GPIO_PIN_6, GPIOB, GPIO_PIN_7, 0u) != 0u) {
        return 0;
    }
    if(ft5206_try_bus(GPIOB, GPIO_PIN_7, GPIOB, GPIO_PIN_6, 0u) != 0u) {
        return 0;
    }

#if !defined(APP_TP_FT6336_CAP_ONLY) || (APP_TP_FT6336_CAP_ONLY == 0)
    if(ft5206_try_bus(GPIOB, GPIO_PIN_0, GPIOF, GPIO_PIN_11, 1u) != 0u) {
        return 0;
    }
#endif

    ct_iic_bind(GPIOB, GPIO_PIN_6, GPIOB, GPIO_PIN_7, 0u);
    ct_iic_init();
    ft5206_reset_config();
    return 1;
}

/* FT5206 5???????? ??????????? */
const uint16_t FT5206_TPX_TBL[5] = {FT5206_TP1_REG, FT5206_TP2_REG, FT5206_TP3_REG, FT5206_TP4_REG, FT5206_TP5_REG};

/**
 * @brief       ??��????(???��?????)
 * @param       mode : ??????��????��???, ???????????
 * @retval      ?????????
 *   @arg       0, ?????????; 
 *   @arg       1, ?????��???;
 */
uint8_t ft5206_scan(uint8_t mode)
{
    uint8_t sta = 0;
    uint8_t buf[4];
    uint8_t i = 0;
    uint8_t res = 0;
    uint16_t temp;
    static uint8_t t = 0;   /* ?????????,???????CPU????? */
    
    t++;
    
    if((t % 10) == 0 || t < 10 || FT5206_INT == 0)
    {
        ft5206_rd_reg(FT5206_REG_NUM_FINGER, &sta, 1);  /* ???????????? */

        if ((sta & 0XF) && ((sta & 0XF) < 6))
        {
            temp = 0XFFFF << (sta & 0XF);           /* ????????????1??��??,???tp_dev.sta???? */
            tp_dev.sta = (~temp) | TP_PRES_DOWN | TP_CATH_PRES;
            delay_ms(4);    /* ??????????????????????��??????? */
            
            for (i = 0; i < 5; i++)
            {
                if (tp_dev.sta & (1 << i))          /* ??????��? */
                {
                    ft5206_rd_reg(FT5206_TPX_TBL[i], buf, 4);   /* ???XY????? */

                    if (tp_dev.touchtype & 0X01)    /* ???? */
                    {
                        tp_dev.y[i] = ((uint16_t)(buf[0] & 0X0F) << 8) + buf[1];
                        tp_dev.x[i] = ((uint16_t)(buf[2] & 0X0F) << 8) + buf[3];
                    }
                    else
                    {
                        uint16_t rx = (uint16_t)(((buf[0] & 0X0F) << 8) + buf[1]);
                        uint16_t ry = (uint16_t)(((buf[2] & 0X0F) << 8) + buf[3]);

#if defined(APP_TP_MIRROR_X) && (APP_TP_MIRROR_X != 0)
                        tp_dev.x[i] = lcddev.width - rx;
#else
                        tp_dev.x[i] = rx;
#endif
                        tp_dev.y[i] = ry;
                    }

                    if ((buf[0] & 0XF0) != 0X80)tp_dev.x[i] = tp_dev.y[i] = 0;      /* ??????contact????????????�� */

                    //printf("x[%d]:%d,y[%d]:%d\r\n", i, tp_dev.x[i], i, tp_dev.y[i]);
                }
            }

            res = 1;

            if (tp_dev.x[0] == 0 && tp_dev.y[0] == 0)sta = 0;   /* ?????????????0,??????????? */

            t = 0;  /* ???????,??????????????10??,???????????? */
        }
    }

    if ((sta & 0X1F) == 0)  /* ????????? */
    {
        if (tp_dev.sta & TP_PRES_DOWN)      /* ?????????? */
        {
            tp_dev.sta &= ~TP_PRES_DOWN;    /* ????????? */
        }
        else    /* ??????��????? */
        {
            tp_dev.x[0] = 0xffff;
            tp_dev.y[0] = 0xffff;
            tp_dev.sta &= 0XE000;           /* ???????��??? */
        }
    }

    if (t > 240)t = 10; /* ?????10??????? */

    return res;
}




























