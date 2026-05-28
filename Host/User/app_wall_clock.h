#ifndef APP_WALL_CLOCK_H
#define APP_WALL_CLOCK_H

#include <stdint.h>

void app_home_wall_clock_set(int year, int month, int day, int hour, int min, int sec);
uint8_t app_wall_clock_valid(void);
uint32_t app_wall_clock_epoch_sec(void);
void app_wall_clock_on_set(int year, int month, int day, int hour, int min, int sec);
uint8_t app_wall_clock_get_datetime(int *year, int *month, int *day, int *hour, int *min, int *sec);

#endif
