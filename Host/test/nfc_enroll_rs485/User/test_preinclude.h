/**
 * Keil ���� Host/User ��Դ�ļ�ʱ�������ҵ� Host/app_config.h��RS485=0����
 * ���ļ�ͨ��Ԥ����ǿ��ʹ�ò��Թ������á�
 */
#ifndef APP_CONFIG_H
#include "app_config.h"
#endif

#undef APP_RS485_ENABLE
#define APP_RS485_ENABLE 1

#include "door.h"
