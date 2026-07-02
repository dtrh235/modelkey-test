#ifndef APP_UI_V3_H
#define APP_UI_V3_H

#include "app_config.h"

#if (APP_UI_V3_ENABLE == 1)

#include "app_ui_v3_types.h"

void app_ui_v3_init(void);
void app_ui_v3_tick(void);
void app_ui_v3_poll(void);
void ui3_nav_to(ui3_scr_id_t id, bool push);
void ui3_nav_back(void);
void ui3_reload_current(void);
ui3_state_t *ui3_state(void);

#else

static inline void app_ui_v3_init(void) {}
static inline void app_ui_v3_tick(void) {}
static inline void app_ui_v3_poll(void) {}

#endif

#endif
