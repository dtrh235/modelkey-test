/**
 ****************************************************************************************************
 * @file        main.c
 * @author      Mr.Mr.
 * @version     V1.0
 * @date        2023-04-23
 * @brief       触摸屏实验
 * @license     Copyright (c) 2020-2032,  
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:ST-1 STM32F407核心板
 *  
 *  
 * 公司网址:www.genbotter.com
 * 购买地址:makerbase.taobao.com
 * 
 ****************************************************************************************************
 */

#include "main.h"
#include "app_fp_hw_diag.h"
#include "app_fp_mirror_diag.h"
#include "app_host_diag.h"
#include "../Drivers/BSP/W25Q16/bsp_w25q16.h"
#include "app_unlock_uart4.h"
#include "app_unlock_event.h"
#include "app_home_unlock.h"
#include "app_home_full_hint.h"
#include "app_wifi_scan.h"
#include "app_wifi_diag.h"
#include "app_screen_wifi_flow.h"
#include "cloud_legacy_adapter.h"
#include "cloud_aliyun_at.h"
#include "app_link_guard.h"
#include "cloud_ota_service.h"
#include "app_state.h"
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
#include "app_fp_mirror_tx.h"
#endif
#if (APP_RS485_ENABLE == 1)
#include "app_rs485_link.h"
#include "app_rs485_proto.h"
#endif

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

#define APP_TASK_PRIO_STORAGE  1u
#define APP_TASK_PRIO_CLOUD    2u
#define APP_TASK_PRIO_CLOUD_WIFI 4u
#define APP_TASK_PRIO_FP       3u
#define APP_TASK_PRIO_NFC      3u
#define APP_TASK_PRIO_GUI      5u

#define APP_TASK_STACK_STORAGE 1536u
/* CloudTask：WiFi 独占时栈需容纳 CWLAP 调用链 */
#define APP_TASK_STACK_CLOUD   4096u
#define APP_TASK_STACK_FP      768u
#define APP_TASK_STACK_NFC     768u
#define APP_TASK_STACK_GUI     1536u
#define APP_TASK_STACK_HOME_AUTH 768u
#define APP_TASK_PRIO_HOME_AUTH  3u

static void app_process_cloud_uart_lines(void)
{
    if(g_usart_rx_sta & 0x8000u) {
        g_usart_rx_sta = 0u;
    }
}

static uint8_t app_enroll_flow_active(void)
{
    return (g_app_scr == APP_SCR_6 ||
            g_app_scr == APP_SCR_8 ||
            g_app_scr == APP_SCR_9 ||
            g_app_scr == APP_SCR_10) ? 1u : 0u;
}

static void app_gui_task(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();

    (void)argument;
    for(;;) {
        app_home_unlock_housekeeping();

        if(g_screen3_need_init && lv_scr_act() == guider_ui.screen_3) {
            screen3_apply_uniform_menu_style();
            screen3_set_menu_selected(g_screen3_pending_index);
            g_screen3_need_init = 0u;
        }
        if(g_screen4_need_init && g_app_scr == APP_SCR_4) {
            lv_obj_t *table = guider_ui.screen_4_table_1;
            if(lv_obj_is_valid(table)) {
                lv_obj_add_state(table, LV_STATE_FOCUSED);
                lv_obj_add_state(table, LV_STATE_FOCUS_KEY);

                {
                    uint16_t r_sel = 0u;
                    uint16_t c_sel = 0u;
                    lv_table_get_selected_cell(table, &r_sel, &c_sel);
                    if(r_sel == LV_TABLE_CELL_NONE || c_sel == LV_TABLE_CELL_NONE) {
                        int32_t k = LV_KEY_DOWN;
                        lv_event_send(table, LV_EVENT_KEY, &k);
                    }
                }
            }
            g_screen4_need_init = 0u;
        }
        if(g_screen5_need_init && g_app_scr == APP_SCR_5) {
            screen5_hide_error_label();
            if(lv_obj_is_valid(guider_ui.screen_5_ta_1)) {
                if(g_screen5_found_acc[0] != '\0') {
                    lv_textarea_set_text(guider_ui.screen_5_ta_1, g_screen5_found_acc);
                } else {
                    lv_textarea_set_text(guider_ui.screen_5_ta_1, "");
                }
                lv_textarea_set_max_length(guider_ui.screen_5_ta_1, 12);
            }
            screen5_set_field_selected();
            g_screen5_need_init = 0u;
        }
        if(g_screen7_need_init && lv_scr_act() == guider_ui.screen_7) {
            g_screen7_choice_yes = 1u;
            screen7_hide_all_msgbox();
            if(lv_obj_is_valid(guider_ui.screen_7_ta_1)) {
                lv_textarea_set_text(guider_ui.screen_7_ta_1, "");
                lv_textarea_set_max_length(guider_ui.screen_7_ta_1, 12);
            }
            screen7_set_field_selected();
            g_screen7_need_init = 0u;
        }

        app_key_ui_handle();
        app_unlock_event_gui_pump();
        app_home_full_hint_update();
        screen1_cursor_blink_handle();
        screen2_cursor_blink_handle();
        screen5_cursor_blink_handle();
        screen6_dlg_cursor_blink_handle();
        screen7_cursor_blink_handle();
        app_touch_ui_handle();

        if(g_app_scr == APP_SCR_11) {
            if(screen_wifi_popup_is_active()) {
                screen_wifi_cursor_blink_handle();
            }
            if(screen_wifi_gui_work_pending() != 0u) {
                screen_wifi_poll_tick();
            }
            /* WiFi 页触屏/选中/弹窗依赖 LVGL 刷新；空闲时也不能停 timer */
            lv_timer_handler();
        } else {
            screen_wifi_cursor_blink_handle();
            lv_timer_handler();
            screen8_cursor_blink_handle();
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(g_app_scr == APP_SCR_11 ? 10u : 5u));
    }
}

static void app_cloud_task(void *argument)
{
    uint32_t cloud_poll_ms = 0u;
    uint32_t hb_ms = 0u;
    static uint8_t s_cloud_boot_done = 0u;

    (void)argument;
#if (APP_RTOS_HEARTBEAT_DEBUG != 0)
    usart_debug_tx_str("[RTOS] CloudTask start\r\n");
#endif

    for(;;) {
#if (APP_CLOUD_ENABLE == 1)
        /* ESP 复位+UART2 约 5s，须在调度器后执行，避免 main 里阻塞导致白屏 */
        if(s_cloud_boot_done == 0u) {
            /* 勿在 WiFi 连接过程中复位 ESP，等本轮 connect 结束再 init */
            if(app_wifi_connect_busy() != 0u || app_link_guard_wifi() != 0u) {
                vTaskDelay(pdMS_TO_TICKS(20u));
                continue;
            }
            tcp_mqtt_init();
            s_cloud_boot_done = 1u;
            vTaskDelay(pdMS_TO_TICKS(10u));
            continue;
        }
#endif
#if (APP_RTOS_HEARTBEAT_DEBUG != 0)
        if(hb_ms == 0u || (HAL_GetTick() - hb_ms) >= 3000u) {
            hb_ms = HAL_GetTick();
            usart_debug_tx_str("[RTOS] CloudTask alive\r\n");
        }
#else
        (void)hb_ms;
#endif

        app_wifi_scan_stuck_recover();

#if (APP_WIFI_UI_SCAN_ENABLE == 1)
        if(g_app_scr == APP_SCR_11) {
            uint8_t uart_work = app_wifi_scan_uart_busy();
            uint8_t conn_busy = app_wifi_connect_busy();
            uint8_t mqtt_work = (uint8_t)(
                cloud_aliyun_at_wifi_link_ready() != 0u ||
                app_link_guard_active() != 0u ||
                cloud_aliyun_at_wifi_bringup_active() != 0u);
            uint8_t need_cloud = (uint8_t)(uart_work != 0u || conn_busy != 0u || mqtt_work != 0u);

            if(need_cloud != 0u) {
                if(conn_busy != 0u || app_link_guard_mqtt() != 0u) {
                    cloud_uart2_pump_rx(8u);
                }
                if(cloud_aliyun_at_poll_allowed(1u) != 0u) {
                    cloud_aliyun_at_poll_5ms();
                }
                if(conn_busy != 0u) {
                    (void)app_wifi_connect_poll();
                }
                if(uart_work != 0u) {
                    app_wifi_scan_cloud_tick();
                }
                cloud_ota_service_poll_5ms();
            }
            vTaskDelay(pdMS_TO_TICKS(need_cloud ? 5u : 200u));
            continue;
        }
#endif

        app_process_cloud_uart_lines();

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
        if(app_rs485_master_cmd_in_progress() == 0u) {
            app_rs485_master_poll_incoming(APP_RS485_MASTER_POLL_MS);
        }
        app_rs485_master_mirror_if_requested();
        users_mirror_run_pending();
        app_fp_mirror_tx_tick();
        {
            static uint32_t s_rs485_slave_upkeep_ms;
            static uint8_t s_rs485_slave_ping_ok;
            uint32_t tnow = HAL_GetTick();

            if((tnow - s_rs485_slave_upkeep_ms) >= 10000u) {
                s_rs485_slave_upkeep_ms = tnow;
                if(app_rs485_link_ready() != 0u) {
                    uint8_t ping_ok;

                    /* 模板分片发送期间禁止 upkeep PING，避免打断当前页 chunk 流导致 idx 卡死重传。 */
                    if(app_fp_mirror_tx_busy() != 0u) {
                        ping_ok = s_rs485_slave_ping_ok;
                    } else {
                        ping_ok = app_rs485_proto_ping_peer(200u) ? 1u : 0u;
                    }
                    if(ping_ok == 0u) {
#if defined(APP_HOST_RS485_DIAG) && (APP_HOST_RS485_DIAG != 0)
                        HOST_RS485_LOG("[RS485] link check PING slave: FAIL\r\n");
#endif
#if (APP_FP_MIRROR_DIAG != 0)
                        FP_MIRROR_LOG("[HOST][RS485] upkeep PING FAIL (A/B/GND slave SLAVE fw?)");
#endif
                    } else {
#if defined(APP_HOST_RS485_DIAG) && (APP_HOST_RS485_DIAG != 0) && \
    defined(APP_HOST_RS485_LOG_VERBOSE) && (APP_HOST_RS485_LOG_VERBOSE != 0)
                        HOST_RS485_LOG("[RS485] link check PING slave: OK\r\n");
#endif
#if (APP_FP_MIRROR_DIAG != 0)
                        if(s_rs485_slave_ping_ok == 0u) {
                            FP_MIRROR_LOG("[HOST][RS485] upkeep PING OK slave online");
                        }
#endif
                    }
                    if(ping_ok != 0u) {
                        /* 仅在从机离线->在线边沿做一次全量镜像；其余时机由“用户变更/指纹变更/从机mirror_req”触发。 */
                        if(s_rs485_slave_ping_ok == 0u &&
                           app_fp_mirror_tx_busy() == 0u) {
                            users_mirror_to_slave_on_link();
                        }
                        s_rs485_slave_ping_ok = 1u;
                    } else {
                        s_rs485_slave_ping_ok = 0u;
                    }
                } else {
#if defined(APP_HOST_RS485_DIAG) && (APP_HOST_RS485_DIAG != 0)
                    HOST_RS485_LOG("[RS485] link check: RS485 USART6 not ready\r\n");
#endif
                    s_rs485_slave_ping_ok = 0u;
                }
            }
        }
#endif

        if((HAL_GetTick() - cloud_poll_ms) >= 30u) {
            cloud_poll_ms = HAL_GetTick();
            tcp_mqtt_while();
        }

        vTaskDelay(pdMS_TO_TICKS(5u));
    }
}

#if (APP_TEMP_DISABLE_BIOMETRIC == 0)
static void app_fp_task(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();

    (void)argument;
    for(;;) {
        if(app_enroll_flow_active() != 0u && g_screen8_fp_enroll_state == 1u && g_nfc_enroll_state != 1u) {
            screen8_handle_fp_enroll();
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10u));
    }
}

static void app_nfc_task(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();

    (void)argument;
    for(;;) {
        if(app_enroll_flow_active() != 0u && g_nfc_enroll_state == 1u) {
            screen8_handle_nfc_enroll();
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10u));
    }
}

static void app_home_auth_task(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();

    (void)argument;
    for(;;) {
        app_home_auth_poll_tick();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10u));
    }
}
#endif

static void app_storage_task(void *argument)
{
    (void)argument;
    users_storage_task_register_current();

    for(;;) {
        users_storage_task_wait_and_flush(250u);
    }
}

static TaskHandle_t s_cloud_task_hdl;

static void app_create_tasks(void)
{
    app_wifi_diag_init();
    configASSERT(xTaskCreate(app_gui_task, "GuiTask", APP_TASK_STACK_GUI, NULL, APP_TASK_PRIO_GUI, NULL) == pdPASS);
    configASSERT(xTaskCreate(app_cloud_task, "CloudTask", APP_TASK_STACK_CLOUD, NULL, APP_TASK_PRIO_CLOUD,
                             &s_cloud_task_hdl) == pdPASS);
    app_wifi_scan_bind_cloud_task(s_cloud_task_hdl);
#if (APP_TEMP_DISABLE_BIOMETRIC == 0)
    configASSERT(xTaskCreate(app_fp_task, "FpTask", APP_TASK_STACK_FP, NULL, APP_TASK_PRIO_FP, NULL) == pdPASS);
    configASSERT(xTaskCreate(app_nfc_task, "NfcTask", APP_TASK_STACK_NFC, NULL, APP_TASK_PRIO_NFC, NULL) == pdPASS);
    configASSERT(xTaskCreate(app_home_auth_task, "HomeAuth", APP_TASK_STACK_HOME_AUTH, NULL,
                             APP_TASK_PRIO_HOME_AUTH, NULL) == pdPASS);
#endif
    configASSERT(xTaskCreate(app_storage_task, "StorageTask", APP_TASK_STACK_STORAGE, NULL, APP_TASK_PRIO_STORAGE, NULL) == pdPASS);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    usart_debug_tx_str("\r\n[RTOS] stack overflow task=");
    if(pcTaskName != NULL) {
        usart_debug_tx_str(pcTaskName);
    }
    usart_debug_tx_str("\r\n");
    __disable_irq();
    while(1) {
    }
}

void vApplicationMallocFailedHook(void)
{
    usart_debug_tx_str("\r\n[RTOS] malloc failed\r\n");
    __disable_irq();
    while(1) {
    }
}

int main(void)
{
    HAL_Init();                         /* 初始化HAL库 */
    sys_stm32_clock_init(336, 8, 2, 7); /* 配置时钟，168MHz */
    board_default_gpio_init();          /* 默认GPIO电平 */
    delay_init(168);                    /* 初始化延时（W25Q 软 SPI 依赖 delay_us） */
    usart_init(115200);
    app_wifi_cfg_init_defaults();
#if (APP_CLOUD_UART_DEBUG != 0)
    usart_debug_tx_str("[FW] cloud debug ON -> [ALIYUN] on USART1 PA9 115200\r\n");
#endif
#if (APP_TOUCH_UART_DEBUG != 0)
    usart_debug_tx_str("\r\n=== Touch debug USART1 PA9=TX PA10=RX 115200 [TP] only ===\r\n");
#elif (APP_WIFI_UART_DEBUG != 0)
#if (APP_DEBUG_ON_USART6 != 0)
    usart_debug_tx_str("\r\n=== WiFi debug USART6 PC6=TX PC7=RX 115200 ===\r\n");
#else
    usart_debug_tx_str("\r\n=== WiFi debug USART1 PA9=TX PA10=RX 115200 ===\r\n");
#endif
    usart_debug_tx_str("[FW] wifi_diag_trace_build " __DATE__ " " __TIME__ "\r\n");
#endif
#if (APP_RS485_ENABLE == 1)
    app_rs485_link_init();
#endif
    /* 外部 Flash 尽早访问：PA15/PB3/PB4 与 JTAG 复用，须在其它模块占 GPIO 前完成 */
    if(!users_storage_load()) {
        g_default_admin_deleted = 1u;
        g_default_admin_is_admin_role = 0u;
        g_default_admin_account[0] = '\0';
        g_default_admin_password[0] = '\0';
        (void)users_try_register("1", "1111", true);
        (void)users_try_register("2", "2222", false);
        (void)users_storage_save();
    } else if(users_migrate_default_admin_to_users() != 0u) {
        (void)users_storage_save();
    }

    app_fp_hw_boot_diag(&g_fp_hw_inited);
//    led_init();                         /* 初始化LED */
    KEY_Init();                         /* 初始化按键 */
//    sram_init();
//    lcd_init();                         /* 初始化LCD */
//    tp_dev.init();                      /* 初始化触摸屏 */
//    nfc_hw_init_once();                 /* NFC上电初始化（仅一次） */
#if (APP_BOOT_VTOR_RELOCATE == 1)
    SCB->VTOR = BOOT_APP_VTOR_ADDR;     /* BootLoader + APP 双工程: APP中断向量重定位 */
#endif

    app_unlock_uart4_init();            /* UART4 解锁成功发送 '1' */

    /* MFRC522 必须做一次软复位才能进入正常读卡状态；否则首页"刷卡"读不到 UID。
     * 之前只在录入页才走 init_once（含 Reset），所以重启回到首页直接刷卡无法解锁。 */
#if (APP_TEMP_DISABLE_BIOMETRIC == 0)
    app_nfc_hw_init_once();
#endif

    btim_timx_int_init(1000 - 1, 84);   // 1ms中断初始化，重装载值、预分频值
    
    lv_init();                          // lvgl初始化
    lv_port_disp_init();                // 显示设备初始化
    lv_port_indev_init();               // 输入设备初始化
    
    setup_ui(&guider_ui);
    events_init(&guider_ui);
    g_app_scr = APP_SCR_HOME;
    lock_btn_set_selected(0);
    menu_btn_set_selected(0);
    app_home_unlock_set_poll_base(HAL_GetTick());
    /* 首帧刷新：此前 tcp_mqtt_init 在 main 里阻塞数秒，lv_timer_handler 未跑会白屏 */
    lv_timer_handler();
    lv_refr_now(lv_disp_get_default());

#if (APP_USE_FREERTOS == 1)
    app_create_tasks();
    vTaskStartScheduler();
#elif (APP_CLOUD_ENABLE == 1)
    tcp_mqtt_init();
#endif

    while (1) {
    }
}
