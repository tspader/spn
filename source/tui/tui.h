#ifndef SPN_TUI_TUI_H
#define SPN_TUI_TUI_H

#include "tui/types.h"
#include "event/types.h"

void            sp_tui_flush(void);
void            spn_tui_init(spn_tui_t* tui, spn_tui_mode_t mode, spn_logger_t* logger);
void            spn_tui_log_event(spn_tui_t* tui, spn_build_event_t* event);
sp_str_t        spn_tui_render_event_detail(sp_mem_t mem, spn_build_event_t* event);
void            spn_tui_attach_prompt(spn_tui_t* tui, sp_prompt_ctx_t* ctx);
void            spn_tui_detach_prompt(spn_tui_t* tui);
void            spn_prompt_stop(spn_tui_t* tui, bool ok);
void            spn_prompt_pump(spn_tui_t* tui);
spn_tui_mode_t  spn_output_mode_from_str(sp_str_t str);
sp_str_t        spn_output_mode_to_str(spn_tui_mode_t mode);

#endif
