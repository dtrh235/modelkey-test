#ifndef APP_BOARD_GPIO_H
#define APP_BOARD_GPIO_H

void board_default_gpio_init(void);
/* JDQ_IN(PE9) 高电平吸合继电器；开锁成功时脉冲吸合 */
void board_relay_unlock_pulse(void);
void board_relay_poll(void);

#endif
