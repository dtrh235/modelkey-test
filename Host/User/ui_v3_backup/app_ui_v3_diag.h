#ifndef APP_UI_V3_DIAG_H
#define APP_UI_V3_DIAG_H

#include "app_config.h"
#include "app_ui_v3_types.h"

#if (APP_UI_V3_ENABLE == 1) && (APP_UI_V3_DIAG != 0)

#include <stdint.h>

void ui3_diag_str(const char *s);
void ui3_diag_fmt(const char *tag, const char *msg);
void ui3_diag_mem(const char *tag);
void ui3_diag_scr(const char *tag, ui3_scr_id_t id, const void *scr);
void ui3_diag_nav(const char *op, ui3_scr_id_t id, uint8_t push);
void ui3_diag_boot_done(void);
void ui3_diag_disp_snap(const char *tag);
void ui3_diag_home_tree(void);
void ui3_diag_gui_first_poll(void);
void ui3_diag_gui_heartbeat(ui3_scr_id_t scr);

#else

static inline void ui3_diag_str(const char *s) { (void)s; }
static inline void ui3_diag_fmt(const char *tag, const char *msg)
{
    (void)tag;
    (void)msg;
}
static inline void ui3_diag_mem(const char *tag) { (void)tag; }
static inline void ui3_diag_scr(const char *tag, ui3_scr_id_t id, const void *scr)
{
    (void)tag;
    (void)id;
    (void)scr;
}
static inline void ui3_diag_nav(const char *op, ui3_scr_id_t id, uint8_t push)
{
    (void)op;
    (void)id;
    (void)push;
}
static inline void ui3_diag_boot_done(void) {}
static inline void ui3_diag_disp_snap(const char *tag) { (void)tag; }
static inline void ui3_diag_home_tree(void) {}
static inline void ui3_diag_gui_first_poll(void) {}
static inline void ui3_diag_gui_heartbeat(ui3_scr_id_t scr) { (void)scr; }

#endif

#endif
