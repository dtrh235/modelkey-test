#include "app_wall_clock.h"
#include "app_wifi_diag.h"

#include "stm32f4xx_hal.h"

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

static struct {
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    uint8_t valid;
} s_wall;

static uint32_t s_anchor_epoch;
static uint32_t s_anchor_uptime_sec;
static uint8_t s_anchor_valid;

static uint8_t is_leap(int y)
{
    if((y % 400) == 0) return 1u;
    if((y % 100) == 0) return 0u;
    return ((y % 4) == 0) ? 1u : 0u;
}

static int dim(int y, int m)
{
    static const uint8_t d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if(m < 1 || m > 12) return 30;
    if(m == 2 && is_leap(y) != 0u) return 29;
    return (int)d[m - 1];
}

static uint32_t ymdhms_to_epoch(int y, int mo, int d, int h, int mi, int s)
{
    uint32_t days = 0u;
    int yy;

    if(y < 1970) return 0u;
    for(yy = 1970; yy < y; yy++) {
        days += is_leap(yy) ? 366u : 365u;
    }
    for(int m = 1; m < mo; m++) {
        days += (uint32_t)dim(y, m);
    }
    if(d > 0) {
        days += (uint32_t)(d - 1);
    }
    return days * 86400u + (uint32_t)h * 3600u + (uint32_t)mi * 60u + (uint32_t)s;
}

static uint8_t epoch_to_ymdhms(uint32_t epoch, int *y, int *mo, int *d, int *h, int *mi, int *s)
{
    uint32_t days;
    int year;
    int month;

    if(epoch < 86400u) {
        return 0u;
    }
    days = epoch / 86400u;
    *h = (int)((epoch % 86400u) / 3600u);
    *mi = (int)((epoch % 3600u) / 60u);
    *s = (int)(epoch % 60u);

    year = 1970;
    while(1) {
        uint32_t ydays = is_leap(year) ? 366u : 365u;
        if(days < ydays) {
            break;
        }
        days -= ydays;
        year++;
    }
    month = 1;
    while(month <= 12) {
        int md = dim(year, month);
        if((uint32_t)md > days) {
            break;
        }
        days -= (uint32_t)md;
        month++;
    }
    *y = year;
    *mo = month;
    *d = (int)days + 1;
    return 1u;
}

static uint8_t format_ymdhm(int y, int mo, int d, int h, int mi, char *buf, size_t buf_sz)
{
    if(buf == NULL || buf_sz == 0u) {
        return 0u;
    }
    if(y < 2020 || mo < 1 || mo > 12 || d < 1 || d > 31) {
        return 0u;
    }
    (void)snprintf(buf, buf_sz, "%04d.%02d.%02d %02d:%02d", y, mo, d, h, mi);
    return 1u;
}

static uint8_t format_epoch_local(uint32_t epoch, char *buf, size_t buf_sz)
{
    int y;
    int mo;
    int d;
    int h;
    int mi;
    int s;

    if(epoch_to_ymdhms(epoch, &y, &mo, &d, &h, &mi, &s) == 0u) {
        return 0u;
    }
    return format_ymdhm(y, mo, d, h, mi, buf, buf_sz);
}

static uint32_t wall_clock_now_epoch(void)
{
    uint32_t now_uptime;
    uint32_t delta;

    if(s_anchor_valid == 0u) {
        return 0u;
    }
    now_uptime = HAL_GetTick() / 1000u;
    if(now_uptime >= s_anchor_uptime_sec) {
        delta = now_uptime - s_anchor_uptime_sec;
        return s_anchor_epoch + delta;
    }
    delta = s_anchor_uptime_sec - now_uptime;
    if(s_anchor_epoch > delta) {
        return s_anchor_epoch - delta;
    }
    return s_anchor_epoch;
}

static volatile uint8_t s_home_ui_pending;
static uint8_t s_home_ui_has_set;
static int s_home_ui_y;
static int s_home_ui_mo;
static int s_home_ui_d;
static int s_home_ui_h;
static int s_home_ui_mi;
static int s_home_ui_s;

void app_wall_clock_schedule_home_ui(int year, int month, int day, int hour, int min, int sec)
{
    s_home_ui_y = year;
    s_home_ui_mo = month;
    s_home_ui_d = day;
    s_home_ui_h = hour;
    s_home_ui_mi = min;
    s_home_ui_s = sec;
    s_home_ui_has_set = 1u;
    app_wall_clock_on_set(year, month, day, hour, min, sec);
    s_home_ui_pending = 1u;
}

void app_wall_clock_schedule_ui_refresh(void)
{
    s_home_ui_has_set = 0u;
    s_home_ui_pending = 1u;
}

void app_wall_clock_gui_poll(void)
{
    uint8_t pending;

    pending = s_home_ui_pending;
    if(pending == 0u) {
        return;
    }
    s_home_ui_pending = 0u;
    if(s_home_ui_has_set != 0u) {
        app_home_wall_clock_set(s_home_ui_y, s_home_ui_mo, s_home_ui_d,
                                s_home_ui_h, s_home_ui_mi, s_home_ui_s);
    } else {
        app_home_wall_clock_refresh_ui();
    }
}

void app_wall_clock_on_set(int year, int month, int day, int hour, int min, int sec)
{
    s_wall.year = year;
    s_wall.month = month;
    s_wall.day = day;
    s_wall.hour = hour;
    s_wall.min = min;
    s_wall.sec = sec;
    s_wall.valid = 1u;
    s_anchor_epoch = ymdhms_to_epoch(year, month, day, hour, min, sec);
    s_anchor_uptime_sec = HAL_GetTick() / 1000u;
    s_anchor_valid = 1u;
    if(s_anchor_epoch < 1600000000u) {
        TIME_TRACE_MSG("[TIME]Wbd\r\n");
    } else {
        TIME_TRACE_MSG("[TIME]Wok\r\n");
    }
}

void app_wall_clock_set_local(int year, int month, int day, int hour, int min, int sec,
                              int offset_sec)
{
    uint32_t epoch;

    if(year < 2020 || month < 1 || month > 12 || day < 1 || day > 31) {
        return;
    }
    epoch = ymdhms_to_epoch(year, month, day, hour, min, sec);
    if(offset_sec > 0) {
        epoch += (uint32_t)offset_sec;
    }
    if(epoch_to_ymdhms(epoch, &year, &month, &day, &hour, &min, &sec) == 0u) {
        return;
    }
    app_wall_clock_on_set(year, month, day, hour, min, sec);
}

uint8_t app_wall_clock_valid(void)
{
    return s_wall.valid;
}

uint32_t app_wall_clock_epoch_sec(void)
{
    if(app_wall_clock_valid() == 0u) {
        return 0u;
    }
    return wall_clock_now_epoch();
}

uint8_t app_wall_clock_format_now(char *buf, size_t buf_sz)
{
    if(app_wall_clock_valid() == 0u) {
        return 0u;
    }
    return format_epoch_local(wall_clock_now_epoch(), buf, buf_sz);
}

uint8_t app_wall_clock_format_unlock_event(char *buf, size_t buf_sz,
                                           uint32_t stored_epoch, uint32_t event_uptime_sec)
{
    uint32_t epoch = stored_epoch;

    if(buf == NULL || buf_sz == 0u) {
        return 0u;
    }
    if(epoch >= 1600000000u) {
        return format_epoch_local(epoch, buf, buf_sz);
    }
    if(app_wall_clock_valid() != 0u) {
        return format_epoch_local(wall_clock_now_epoch(), buf, buf_sz);
    }
    if(s_anchor_valid != 0u && event_uptime_sec > 0u) {
        if(event_uptime_sec <= s_anchor_uptime_sec) {
            uint32_t delta = s_anchor_uptime_sec - event_uptime_sec;
            if(s_anchor_epoch > delta) {
                epoch = s_anchor_epoch - delta;
                return format_epoch_local(epoch, buf, buf_sz);
            }
        } else {
            uint32_t delta = event_uptime_sec - s_anchor_uptime_sec;
            epoch = s_anchor_epoch + delta;
            return format_epoch_local(epoch, buf, buf_sz);
        }
    }
    return 0u;
}

uint8_t app_wall_clock_get_datetime(int *year, int *month, int *day, int *hour, int *min, int *sec)
{
    int y;
    int mo;
    int d;
    int h;
    int mi;
    int s;

    if(app_wall_clock_valid() == 0u || s_anchor_valid == 0u) {
        return 0u;
    }
    if(epoch_to_ymdhms(wall_clock_now_epoch(), &y, &mo, &d, &h, &mi, &s) == 0u) {
        return 0u;
    }
    s_wall.year = y;
    s_wall.month = mo;
    s_wall.day = d;
    s_wall.hour = h;
    s_wall.min = mi;
    s_wall.sec = s;
    if(year != NULL) *year = y;
    if(month != NULL) *month = mo;
    if(day != NULL) *day = d;
    if(hour != NULL) *hour = h;
    if(min != NULL) *min = mi;
    if(sec != NULL) *sec = s;
    return 1u;
}
