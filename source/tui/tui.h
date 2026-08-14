#ifndef SPN_TUI_TUI_H
#define SPN_TUI_TUI_H

#include "tui/types.h"

void            spn_tui_init(spn_tui_t* tui);
void            spn_tui_open(spn_tui_t* tui, spn_ctx_t* ctx, spn_tui_mode_t mode, spn_verbosity_t verbosity, sp_sys_fd_t wake_read, sp_sys_fd_t wake_write);
void            spn_tui_flush(spn_tui_t* tui);
void            spn_tui_poll(spn_tui_t* tui, spn_op_t* op);
bool            spn_tui_wants_input(spn_tui_t* tui);
void            spn_tui_op_done(spn_tui_t* tui, spn_op_t* op);
void            spn_tui_handoff(spn_tui_t* tui);
void            spn_tui_log_event(spn_tui_t* tui, spn_event_t* event);
void            spn_prompt_stop(spn_tui_t* tui, sp_prompt_state_t state);
spn_tui_mode_t  spn_output_mode_from_str(sp_str_t str);
void            spn_print(spn_tui_t* tui, const c8* fmt, ...);
void            spn_print_err(spn_tui_t* tui, const c8* fmt, ...);

#endif
