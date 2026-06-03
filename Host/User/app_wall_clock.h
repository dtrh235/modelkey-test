#ifndef APP_WALL_CLOCK_H
#define APP_WALL_CLOCK_H

#include <stdint.h>
#include <stddef.h>

void app_home_wall_clock_set(int year, int month, int day, int hour, int min, int sec);
void app_home_wall_clock_refresh_ui(void);
/* CloudTask 不得直接调 LVGL；仅更新墙钟数据，UI 在 GuiTask 刷新 */
void app_wall_clock_schedule_home_ui(int year, int month, int day, int hour, int min, int sec);
void app_wall_clock_schedule_ui_refresh(void);
void app_wall_clock_gui_poll(void);
uint8_t app_wall_clock_valid(void);
uint32_t app_wall_clock_epoch_sec(void);
void app_wall_clock_on_set(int year, int month, int day, int hour, int min, int sec);
/* 设置本地日历时刻并加 offset_sec 秒（用于 SNTP 查询滞后补偿） */
void app_wall_clock_set_local(int year, int month, int day, int hour, int min, int sec,
                              int offset_sec);
uint8_t app_wall_clock_get_datetime(int *year, int *month, int *day, int *hour, int *min, int *sec);
/* 当前墙钟时刻，Aliyun unlock_time 文本：YYYY.MM.DD HH:MM */
uint8_t app_wall_clock_format_now(char *buf, size_t buf_sz);
/* 根据开锁时存的 epoch/uptime 还原真实开锁时刻文本 */
uint8_t app_wall_clock_format_unlock_event(char *buf, size_t buf_sz,
                                           uint32_t stored_epoch, uint32_t event_uptime_sec);

#endif
