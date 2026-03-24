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
void            sp_tui_begin_table(sp_tui_table_t* table);
void            sp_tui_table_setup_column(sp_tui_table_t* table, sp_str_t name);
void            sp_tui_table_setup_column_ex(sp_tui_table_t* table, sp_str_t name, u32 min_width);
void            sp_tui_table_header_row(sp_tui_table_t* table);
void            sp_tui_table_next_row(sp_tui_table_t* table);
void            sp_tui_table_column(sp_tui_table_t* table, u32 n);
void            sp_tui_table_column_named(sp_tui_table_t* table, sp_str_t name);
void            sp_tui_table_fmt(sp_tui_table_t* table, const c8* fmt, ...);
void            sp_tui_table_str(sp_tui_table_t* table, sp_str_t str);
void            sp_tui_table_set_indent(sp_tui_table_t* table, u32 indent);
void            sp_tui_table_end(sp_tui_table_t* table);
sp_str_t        sp_tui_table_render(sp_tui_table_t* table);
void            spn_tui_init(spn_tui_t* tui, spn_tui_mode_t mode);
sp_str_t        spn_tui_render_event(spn_build_event_t* event, u32 max_name);
spn_tui_mode_t  spn_output_mode_from_str(sp_str_t str);
sp_str_t        spn_output_mode_to_str(spn_tui_mode_t mode);

#endif
