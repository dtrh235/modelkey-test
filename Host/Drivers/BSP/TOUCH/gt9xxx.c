#include "app_config.h"
#if defined(APP_TP_FT6336_CAP_ONLY) && (APP_TP_FT6336_CAP_ONLY != 0)

#include "./BSP/TOUCH/gt9xxx.h"

uint8_t gt9xxx_init(void) { return 1; }

uint8_t gt9xxx_scan(uint8_t mode) { (void)mode; return 0; }

#else
#include "string.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/TOUCH/ctiic.h"
#include "./BSP/TOUCH/gt9xxx.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"


/* ע��: ����GT9271֧��10�㴥��֮��, ��������оƬֻ֧�� 5�㴥�� */
uint8_t g_gt_tnum = 5;      /* Ĭ��֧�ֵĴ���������(5�㴥��) */

/**
 * @brief       ��gt9xxxд��һ������
 * @param       reg : ��ʼ�Ĵ�����ַ
 * @param       buf : ���ݻ�������
 * @param       len : д���ݳ���
 * @retval      0, �ɹ�; 1, ʧ��;
 */
uint8_t gt9xxx_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    uint8_t ret = 0;

    ct_iic_start();
    ct_iic_send_byte(GT9XXX_CMD_WR);    /* ����д���� */
    ct_iic_wait_ack();
    ct_iic_send_byte(reg >> 8);         /* ���͸�8λ��ַ */
    ct_iic_wait_ack();
    ct_iic_send_byte(reg & 0XFF);       /* ���͵�8λ��ַ */
    ct_iic_wait_ack();

    for (i = 0; i < len; i++)
    {
        ct_iic_send_byte(buf[i]);       /* ������ */
        ret = ct_iic_wait_ack();

        if (ret) break;
    }

    ct_iic_stop();  /* ����һ��ֹͣ���� */
    return ret;
}

/**
 * @brief       ��gt9xxx����һ������
 * @param       reg : ��ʼ�Ĵ�����ַ
 * @param       buf : ���ݻ�������
 * @param       len : �����ݳ���
 * @retval      ��
 */
void gt9xxx_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    ct_iic_start();
    ct_iic_send_byte(GT9XXX_CMD_WR);    /* ����д���� */
    ct_iic_wait_ack();
    ct_iic_send_byte(reg >> 8);         /* ���͸�8λ��ַ */
    ct_iic_wait_ack();
    ct_iic_send_byte(reg & 0XFF);       /* ���͵�8λ��ַ */
    ct_iic_wait_ack();
    ct_iic_start();
    ct_iic_send_byte(GT9XXX_CMD_RD);    /* ���Ͷ����� */
    ct_iic_wait_ack();

    for (i = 0; i < len; i++)
    {
        buf[i] = ct_iic_read_byte(i == (len - 1) ? 0 : 1);  /* ��ȡ���� */
    }

    ct_iic_stop();  /* ����һ��ֹͣ���� */
}

/**
 * @brief       ��ʼ��gt9xxx������
 * @param       ��
 * @retval      0, ��ʼ���ɹ�; 1, ��ʼ��ʧ��;
 */
uint8_t gt9xxx_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    uint8_t temp[5];

    GT9XXX_RST_GPIO_CLK_ENABLE();   /* RST����ʱ��ʹ�� */
    GT9XXX_INT_GPIO_CLK_ENABLE();   /* INT����ʱ��ʹ�� */

    gpio_init_struct.Pin = GT9XXX_RST_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;            /* ������� */
    gpio_init_struct.Pull = GPIO_PULLUP;                    /* ���� */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;     /* ���� */
    HAL_GPIO_Init(GT9XXX_RST_GPIO_PORT, &gpio_init_struct); /* ��ʼ��RST���� */

    gpio_init_struct.Pin = GT9XXX_INT_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                /* ���� */
    gpio_init_struct.Pull = GPIO_PULLUP;                    /* ���� */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;     /* ���� */
    HAL_GPIO_Init(GT9XXX_INT_GPIO_PORT, &gpio_init_struct); /* ��ʼ��INT���� */

    ct_iic_init();      /* ��ʼ����������I2C���� */
    GT9XXX_RST(0);      /* ��λ */
    delay_ms(10);
    GT9XXX_RST(1);      /* �ͷŸ�λ */
    delay_ms(10);

    /* INT����ģʽ����, ����ģʽ, �������� */
    gpio_init_struct.Pin = GT9XXX_INT_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                /* ���� */
    gpio_init_struct.Pull = GPIO_NOPULL;                    /* ����������������ģʽ */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;     /* ���� */
    HAL_GPIO_Init(GT9XXX_INT_GPIO_PORT, &gpio_init_struct); /* ��ʼ��INT���� */

    delay_ms(100);
    gt9xxx_rd_reg(GT9XXX_PID_REG, temp, 4); /* ��ȡ����IC��ID */
    temp[4] = 0;
    
    /* �ж�һ���Ƿ����ض��Ĵ����� */
    if (strcmp((char *)temp, "911") && strcmp((char *)temp, "9147") && strcmp((char *)temp, "1158") && strcmp((char *)temp, "9271"))
    {
        return 1;   /* �����Ǵ������õ���GT911/9147/1158/9271�����ʼ��ʧ�ܣ���Ӳ���鿴����IC�ͺ��Լ��鿴ʱ�����Ƿ���ȷ */
    }
    
    if (strcmp((char *)temp, "9271") == 0)  /* ID==9271, ֧��10�㴥�� */
    {
         g_gt_tnum = 10;    /* ֧��10�㴥���� */
    }
    
    temp[0] = 0X02;
    gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1);    /* ����λGT9XXX */
    
    delay_ms(10);
    
    temp[0] = 0X00;
    gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1);    /* ������λ, ���������״̬ */

    return 0;
}

/* GT9XXX 10��������(���) ��Ӧ�ļĴ����� */
const uint16_t GT9XXX_TPX_TBL[10] =
{
    GT9XXX_TP1_REG, GT9XXX_TP2_REG, GT9XXX_TP3_REG, GT9XXX_TP4_REG, GT9XXX_TP5_REG,
    GT9XXX_TP6_REG, GT9XXX_TP7_REG, GT9XXX_TP8_REG, GT9XXX_TP9_REG, GT9XXX_TP10_REG,
};

/**
 * @brief       ɨ�败����(���ò�ѯ��ʽ)
 * @param       mode : ������δ�õ��β���, Ϊ�˼��ݵ�����
 * @retval      ��ǰ����״̬
 *   @arg       0, �����޴���; 
 *   @arg       1, �����д���;
 */
uint8_t gt9xxx_scan(uint8_t mode)
{
    uint8_t buf[4];
    uint8_t i = 0;
    uint8_t res = 0;
    uint16_t temp;
    uint16_t tempsta;
    static uint8_t t = 0;   /* ���Ʋ�ѯ���,�Ӷ�����CPUռ���� */
    t++;

    if ((t % 10) == 0 || t < 10)    /* ����ʱ,ÿ����10��CTP_Scan�����ż��1��,�Ӷ���ʡCPUʹ���� */
    {
        gt9xxx_rd_reg(GT9XXX_GSTID_REG, &mode, 1);  /* ��ȡ�������״̬ */

        if ((mode & 0X80) && ((mode & 0XF) <= g_gt_tnum))
        {
            i = 0;
            gt9xxx_wr_reg(GT9XXX_GSTID_REG, &i, 1); /* ���־ */
        }

        if ((mode & 0XF) && ((mode & 0XF) <= g_gt_tnum))
        {
            temp = 0XFFFF << (mode & 0XF);  /* ����ĸ���ת��Ϊ1��λ��,ƥ��tp_dev.sta���� */
            tempsta = tp_dev.sta;           /* ���浱ǰ��tp_dev.staֵ */
            tp_dev.sta = (~temp) | TP_PRES_DOWN | TP_CATH_PRES;
            tp_dev.x[g_gt_tnum - 1] = tp_dev.x[0];  /* ���津��0������,���������һ���� */
            tp_dev.y[g_gt_tnum - 1] = tp_dev.y[0];

            for (i = 0; i < g_gt_tnum; i++)
            {
                if (tp_dev.sta & (1 << i))  /* ������Ч? */
                {
                    gt9xxx_rd_reg(GT9XXX_TPX_TBL[i], buf, 4);   /* ��ȡXY����ֵ */

                    if (lcddev.id == 0X5510 || lcddev.id == 0X9806 || lcddev.id == 0X7796)     /* 4.3��800*480 �� 3.5��480*320 MCU�� */
                    {
                        if (tp_dev.touchtype & 0X01)    /* ���� */
                        {
                            tp_dev.x[i] = lcddev.width - (((uint16_t)buf[3] << 8) + buf[2]);
                            tp_dev.y[i] = ((uint16_t)buf[1] << 8) + buf[0];
                        }
                        else
                        {
                            tp_dev.x[i] = ((uint16_t)buf[1] << 8) + buf[0];
                            tp_dev.y[i] = ((uint16_t)buf[3] << 8) + buf[2];
                        }
                    }
                    else    /* �����ͺ� */
                    {
                        if (tp_dev.touchtype & 0X01)    /* ���� */
                        {
                            tp_dev.x[i] = ((uint16_t)buf[1] << 8) + buf[0];
                            tp_dev.y[i] = ((uint16_t)buf[3] << 8) + buf[2];
                        }
                        else
                        {
                            tp_dev.x[i] = lcddev.width - (((uint16_t)buf[3] << 8) + buf[2]);
                            tp_dev.y[i] = ((uint16_t)buf[1] << 8) + buf[0];
                        }
                    }

                    //printf("x[%d]:%d,y[%d]:%d\r\n", i, tp_dev.x[i], i, tp_dev.y[i]);
                }
            }

            res = 1;

            if (tp_dev.x[0] > lcddev.width || tp_dev.y[0] > lcddev.height)  /* �Ƿ�����(���곬����) */
            {
                if ((mode & 0XF) > 1)   /* ��������������,�򸴵ڶ�����������ݵ���һ������. */
                {
                    tp_dev.x[0] = tp_dev.x[1];
                    tp_dev.y[0] = tp_dev.y[1];
                    t = 0;  /* ����һ��,��������������10��,�Ӷ���������� */
                }
                else        /* �Ƿ�����,����Դ˴�����(��ԭԭ����) */
                {
                    tp_dev.x[0] = tp_dev.x[g_gt_tnum - 1];
                    tp_dev.y[0] = tp_dev.y[g_gt_tnum - 1];
                    mode = 0X80;
                    tp_dev.sta = tempsta;   /* �ָ�tp_dev.sta */
                }
            }
            else 
            {
                t = 0;      /* ����һ��,��������������10��,�Ӷ���������� */
            }
        }
    }

    if ((mode & 0X8F) == 0X80)  /* �޴����㰴�� */
    {
        if (tp_dev.sta & TP_PRES_DOWN)      /* ֮ǰ�Ǳ����µ� */
        {
            tp_dev.sta &= ~TP_PRES_DOWN;    /* ��ǰ����ɿ� */
        }
        else    /* ֮ǰ��û�б����� */
        {
            tp_dev.x[0] = 0xffff;
            tp_dev.y[0] = 0xffff;
            tp_dev.sta &= 0XE000;           /* �������Ч��� */
        }
    }

    if (t > 240)t = 10; /* ���´�10��ʼ���� */

    return res;
}





























#endif
