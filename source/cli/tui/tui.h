#ifndef SPN_TUI_TUI_H
#define SPN_TUI_TUI_H

#include "sp/sp_cli.h"

#include "tui/types.h"

void            spn_tui_init(spn_tui_t* tui, spn_tui_desc_t desc);
void            spn_tui_flush(spn_tui_t* tui);
void            spn_tui_poll(spn_tui_t* tui, spn_op_t* op);
bool            spn_tui_wants_input(spn_tui_t* tui);
void            spn_tui_op_done(spn_tui_t* tui, spn_op_t* op);
void            spn_tui_handoff(spn_tui_t* tui);
void            spn_tui_log_event(spn_tui_t* tui, spn_event_t* event);
void            spn_prompt_stop(spn_tui_t* tui, sp_prompt_state_t state);
void            spn_tui_error(spn_tui_t* tui, const c8* fmt, ...);
void            spn_tui_error_v(spn_tui_t* tui, const c8* fmt, va_list args);
void            spn_tui_usage(spn_tui_t* tui, sp_cli_err_t err);
void            spn_print(spn_tui_t* tui, const c8* fmt, ...);
void            spn_print_err(spn_tui_t* tui, const c8* fmt, ...);

#endif
