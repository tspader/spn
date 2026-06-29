#ifndef SPN_TUI_TUI_H
#define SPN_TUI_TUI_H

#include "tui/types.h"
#include "event/types.h"

void            sp_tui_print(sp_str_t str);
void            sp_tui_up(u32 n);
void            sp_tui_down(u32 n);
void            sp_tui_clear_line(void);
void            sp_tui_show_cursor(void);
void            sp_tui_hide_cursor(void);
void            sp_tui_home(void);
void            sp_tui_flush(void);
void            sp_tui_checkpoint(spn_tui_t* tui);
void            sp_tui_restore(spn_tui_t* tui);
void            sp_tui_setup_raw_mode(spn_tui_t* tui);
void            spn_tui_init(spn_tui_t* tui, spn_tui_mode_t mode);
sp_str_t        spn_tui_render_event(spn_build_event_t* event, u32 max_name);
sp_str_t spn_tui_render_coarse_event(spn_build_event_t* event, u32 max_name, sp_str_t root_qualified);
spn_tui_mode_t  spn_output_mode_from_str(sp_str_t str);
sp_str_t        spn_output_mode_to_str(spn_tui_mode_t mode);

#endif
