#ifndef APP_WALL_CLOCK_H
#define APP_WALL_CLOCK_H

/**
 * 更新首页数字时钟与日期标签（与 widgets_init / setup_scr_screen 中的控件绑定）。
 */
void app_home_wall_clock_set(int year, int month, int day, int hour, int min, int sec);

#endif
