#include "app_wall_clock.h"

#include <stdint.h>
#include <stddef.h>

static struct {
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    uint8_t valid;
} s_wall;

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


void app_wall_clock_on_set(int year, int month, int day, int hour, int min, int sec)
{
    s_wall.year = year;
    s_wall.month = month;
    s_wall.day = day;
    s_wall.hour = hour;
    s_wall.min = min;
    s_wall.sec = sec;
    s_wall.valid = 1u;
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
    return ymdhms_to_epoch(s_wall.year, s_wall.month, s_wall.day,
                           s_wall.hour, s_wall.min, s_wall.sec);
}

uint8_t app_wall_clock_get_datetime(int *year, int *month, int *day, int *hour, int *min, int *sec)
{
    if(app_wall_clock_valid() == 0u) {
        return 0u;
    }
    if(year != NULL) *year = s_wall.year;
    if(month != NULL) *month = s_wall.month;
    if(day != NULL) *day = s_wall.day;
    if(hour != NULL) *hour = s_wall.hour;
    if(min != NULL) *min = s_wall.min;
    if(sec != NULL) *sec = s_wall.sec;
    return 1u;
}
