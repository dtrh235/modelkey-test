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
#include "app_config.h"
#include "app_iwdg.h"
#include "app_slave_diag.h"
#include "app_unlock_uart4.h"
#include "app_unlock_event.h"
#include "app_nfc_hw.h"
#include "app_user_ops.h"
#if (APP_RS485_ENABLE == 1)
#include "app_rs485_link.h"
#include "app_rs485_proto.h"
#if APP_RS485_IS_SLAVE
#include "app_slave_fp_sync.h"
#endif
#endif
#include "app_nav_entries.h"
#include "app_slave_ui_fixup.h"
#include "app_home_fp_poll.h"
#include "app_fp_hw_diag.h"

#if (APP_USE_FREERTOS == 1)
#include "FreeRTOS.h"
#include "task.h"
#endif

#define APP_TASK_PRIO_STORAGE  1u
#define APP_TASK_PRIO_CLOUD    2u
#define APP_TASK_PRIO_FP       4u
#define APP_TASK_PRIO_NFC      3u
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
/* Keep RS485 responsive for ACKs; host slows template pages so FpTask can write between bursts. */
#define APP_TASK_PRIO_RS485    3u
#endif
#define APP_TASK_PRIO_GUI      5u

#define APP_TASK_STACK_STORAGE 1536u
#define APP_TASK_STACK_CLOUD   1280u
#define APP_TASK_STACK_RS485   512u
#define APP_TASK_STACK_FP      1280u
#define APP_TASK_STACK_NFC     768u
#define APP_TASK_STACK_GUI     1024u

#if (APP_USE_FREERTOS == 1)
void vApplicationTickHook(void)
{
    app_iwdg_feed();
}

void vApplicationIdleHook(void)
{
    app_iwdg_feed();
}
#endif

static void app_process_cloud_uart_lines(void)
{
    if(g_usart_rx_sta & 0x8000u) {
        uint16_t len = (uint16_t)(g_usart_rx_sta & 0x3FFFu);
        if(len > 0u && len < USART_REC_LEN) {
            g_usart_rx_buf[len] = '\0';
            cloud_ota_service_ingest_line((const char *)g_usart_rx_buf);
        }
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
#if APP_RS485_IS_SLAVE
        app_unlock_event_gui_poll();
#endif

#if !APP_RS485_IS_SLAVE
        if(g_screen3_need_init && lv_scr_act() == guider_ui.screen_3) {
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
#endif /* !APP_RS485_IS_SLAVE */

        app_key_ui_handle();
        screen1_cursor_blink_handle();
#if !APP_RS485_IS_SLAVE
        screen2_cursor_blink_handle();
        screen5_cursor_blink_handle();
        screen6_dlg_cursor_blink_handle();
        screen7_cursor_blink_handle();
        screen8_cursor_blink_handle();
        screen8_success_popup_poll();
#endif
        lv_timer_handler();

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(5u));
    }
}

static void app_cloud_task(void *argument)
{
    uint32_t cloud_poll_ms = 0u;
    uint32_t ota_poll_ms = 0u;

    (void)argument;
    tcp_mqtt_init();

    for(;;) {
        app_process_cloud_uart_lines();

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_MASTER
        app_rs485_master_poll_incoming(3u);
#endif

        if((HAL_GetTick() - cloud_poll_ms) >= 30u) {
            cloud_poll_ms = HAL_GetTick();
            tcp_mqtt_while();
        }
        if((HAL_GetTick() - ota_poll_ms) >= 50u) {
            ota_poll_ms = HAL_GetTick();
            ota_firmware_get();
        }

        vTaskDelay(pdMS_TO_TICKS(5u));
    }
}

static void app_fp_task(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();

    (void)argument;
    for(;;) {
        if(app_enroll_flow_active() != 0u && g_screen8_fp_enroll_state == 1u && g_nfc_enroll_state != 1u) {
            screen8_handle_fp_enroll();
        }

#if APP_RS485_IS_SLAVE
        /* 先写 RS485 队列里的模板，写完再轮询开锁，避免与 AS608 写入抢 USART3 */
        app_slave_fp_template_apply_pending(&g_fp_hw_inited);
        if(g_app_scr == APP_SCR_1 &&
           app_slave_fp_queue_pending() == 0u &&
           app_slave_fp_quiet_active() == 0u) {
#else
        if(g_app_scr == APP_SCR_HOME) {
#endif
            app_home_fp_poll_handle(&g_home_fp_last_poll_ms, &g_fp_hw_inited);
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

#if APP_RS485_IS_SLAVE
        if(g_app_scr == APP_SCR_1) {
#else
        if(g_app_scr == APP_SCR_HOME) {
#endif
            app_home_nfc_poll_handle();
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10u));
    }
}

#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
static void app_rs485_slave_task(void *argument)
{
    (void)argument;
    for(;;) {
        app_rs485_slave_server_poll(APP_RS485_SLAVE_LISTEN_MS);
        app_rs485_slave_upkeep_mirror_request();
        app_rs485_slave_flush_pending_notify();
        vTaskDelay(pdMS_TO_TICKS(1u));
    }
}
#endif

static void app_storage_task(void *argument)
{
    (void)argument;
    users_storage_task_register_current();

    for(;;) {
        users_storage_boot_commit_pending();
        users_slave_mirror_save_tick();
        users_storage_task_wait_and_flush(250u);
    }
}

static void app_create_tasks(void)
{
    configASSERT(xTaskCreate(app_gui_task, "GuiTask", APP_TASK_STACK_GUI, NULL, APP_TASK_PRIO_GUI, NULL) == pdPASS);
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
    configASSERT(xTaskCreate(app_rs485_slave_task, "Rs485Slave", APP_TASK_STACK_RS485, NULL, APP_TASK_PRIO_RS485, NULL) == pdPASS);
#endif
    configASSERT(xTaskCreate(app_cloud_task, "CloudTask", APP_TASK_STACK_CLOUD, NULL, APP_TASK_PRIO_CLOUD, NULL) == pdPASS);
    configASSERT(xTaskCreate(app_fp_task, "FpTask", APP_TASK_STACK_FP, NULL, APP_TASK_PRIO_FP, NULL) == pdPASS);
    configASSERT(xTaskCreate(app_nfc_task, "NfcTask", APP_TASK_STACK_NFC, NULL, APP_TASK_PRIO_NFC, NULL) == pdPASS);
    configASSERT(xTaskCreate(app_storage_task, "StorageTask", APP_TASK_STACK_STORAGE, NULL, APP_TASK_PRIO_STORAGE, NULL) == pdPASS);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
#if (APP_SLAVE_USART1_DEBUG != 0)
    SLAVE_DBG_LOG("[SLV] FATAL stack overflow task=%s", (pcTaskName != NULL) ? pcTaskName : "?");
#endif
    __disable_irq();
    for(;;) {
        app_iwdg_feed();
    }
}

void vApplicationMallocFailedHook(void)
{
#if (APP_SLAVE_USART1_DEBUG != 0)
    SLAVE_DBG_LOG("[SLV] FATAL malloc failed (heap/LVGL OOM?)");
#endif
    __disable_irq();
    for(;;) {
        app_iwdg_feed();
    }
}

#if (APP_SLAVE_USART1_DEBUG != 0)
/* 上电后 USB 转串口常有一小段乱码；延时后再发。多条相同 boot 行 = MCU 在反复复位，读 RCC_CSR 判断原因。 */
static void slave_boot_uart_sync_and_log_reset(void)
{
    uint32_t csr;

    delay_ms(30);
    usart_debug_tx_str("\r\n");
    csr = RCC->CSR;
#if (APP_SLAVE_LOG_VERBOSE != 0)
    SLAVE_DBG_LOG("[SLV] rst CSR=0x%08lX LPWR=%u WWDG=%u IWDG=%u SFTR=%u POR=%u PIN=%u BOR=%u",
                  (unsigned long)csr,
                  (unsigned)((csr & RCC_CSR_LPWRRSTF) != 0u),
                  (unsigned)((csr & RCC_CSR_WWDGRSTF) != 0u),
                  (unsigned)((csr & RCC_CSR_IWDGRSTF) != 0u),
                  (unsigned)((csr & RCC_CSR_SFTRSTF) != 0u),
                  (unsigned)((csr & RCC_CSR_PORRSTF) != 0u),
                  (unsigned)((csr & RCC_CSR_PINRSTF) != 0u),
                  (unsigned)((csr & RCC_CSR_BORRSTF) != 0u));
#else
    SLAVE_DBG_LOG("[SLV] boot rst IWDG=%u WWDG=%u PIN=%u",
                  (unsigned)((csr & RCC_CSR_IWDGRSTF) != 0u),
                  (unsigned)((csr & RCC_CSR_WWDGRSTF) != 0u),
                  (unsigned)((csr & RCC_CSR_PINRSTF) != 0u));
#endif
    RCC->CSR |= RCC_CSR_RMVF;
}
#endif

int main(void)
{
    HAL_Init();                         /* 初始化HAL库 */
    app_iwdg_feed();
    sys_stm32_clock_init(336, 8, 2, 7); /* 配置时钟，168MHz */
    app_iwdg_feed();
    board_default_gpio_init();          /* 默认GPIO电平 */
    delay_init(168);                    /* 初始化延时 */
    usart_init(115200);                 /* USART1 调试；USART3 PB10/PB11 AS608 57600 */
#if (APP_RS485_ENABLE == 1)
    app_rs485_link_init();              /* 须在 boot diag 前 */
#endif
    app_fp_hw_boot_diag(&g_fp_hw_inited);
#if (APP_SLAVE_USART1_DEBUG != 0)
    slave_boot_uart_sync_and_log_reset();
#if (APP_SLAVE_LOG_VERBOSE != 0)
    SLAVE_DBG_LOG("[SLV] boot tick=%lu RS485=%u addr=%02X peer=%02X",
                  (unsigned long)HAL_GetTick(), (unsigned)(APP_RS485_ENABLE),
                  (unsigned)APP_RS485_LOCAL_ADDR, (unsigned)APP_RS485_PEER_ADDR);
#endif
#endif
    app_iwdg_feed();
//    led_init();                         /* 初始化LED */
    KEY_Init();                         /* 初始化按键 */
//    sram_init();
//    lcd_init();                         /* 初始化LCD */
//    tp_dev.init();                      /* 初始化触摸屏 */
//    nfc_hw_init_once();                 /* NFC上电初始化（仅一次） */
#if (APP_BOOT_VTOR_RELOCATE == 1)
    SCB->VTOR = BOOT_APP_VTOR_ADDR;     /* BootLoader + APP 双工程: APP中断向量重定位 */
#endif

    /* 用户数据在外部 Flash(SPI1 PB3/4/5)，须在 LVGL 占用 SPI1(PA5/6/7 屏)之前访问，并在驱动内 reclaim LCD 总线 */
    if(!users_storage_load()) {
        strncpy(g_default_admin_account, ADMIN_DEFAULT_ACCOUNT, sizeof(g_default_admin_account) - 1u);
        g_default_admin_account[sizeof(g_default_admin_account) - 1u] = '\0';
        strncpy(g_default_admin_password, ADMIN_DEFAULT_PASSWORD, sizeof(g_default_admin_password) - 1u);
        g_default_admin_password[sizeof(g_default_admin_password) - 1u] = '\0';
        (void)users_try_register("2", "2222", false);
        users_storage_mark_boot_save_pending();
    }
    app_iwdg_feed();
    app_unlock_uart4_init();            /* UART4 解锁成功发送 '1' */

    /* MFRC522 必须做一次软复位才能进入正常读卡状态；否则首页"刷卡"读不到 UID。
     * 之前只在录入页才走 init_once（含 Reset），所以重启回到首页直接刷卡无法解锁。 */
    app_nfc_hw_init_once();
    app_iwdg_feed();

    btim_timx_int_init(1000 - 1, 84);   // 1ms中断初始化，重装载值、预分频值
    
    lv_init();                          // lvgl初始化
    app_iwdg_feed();
    lv_port_disp_init();                // 显示设备初始化
    app_iwdg_feed();
    lv_port_indev_init();               // 输入设备初始化
    app_iwdg_feed();
    
    setup_ui(&guider_ui);
    events_init(&guider_ui);
#if APP_RS485_IS_SLAVE
    app_slave_ui_hide_corner_ok_esc();
    /* LVGL/触摸初始化后再拉起 MFRC522，避免读卡器在屏初始化后失效 */
    app_nfc_hw_init_once();
    enter_screen_1();
#else
    g_app_scr = APP_SCR_HOME;
    lock_btn_set_selected(0);
    menu_btn_set_selected(0);
#endif
    g_home_fp_last_poll_ms = HAL_GetTick();
#if (APP_RS485_ENABLE == 1) && APP_RS485_IS_SLAVE
    app_home_nfc_poll_arm_immediate();
    app_home_fp_poll_arm_immediate();
#else
    app_home_unlock_set_poll_base(HAL_GetTick());
#endif

#if (APP_USE_FREERTOS == 1)
    app_iwdg_feed();
    app_create_tasks();
    vTaskStartScheduler();
#endif

    while (1) {
    }
}
