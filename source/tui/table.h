#ifndef SPN_TUI_TABLE_H
#define SPN_TUI_TABLE_H

#include "tui/types.h"

void     sp_tui_begin_table(sp_mem_t mem, sp_tui_table_t* table);
void     sp_tui_table_setup_column(sp_tui_table_t* table, sp_str_t name);
void     sp_tui_table_setup_column_ex(sp_tui_table_t* table, sp_str_t name, u32 min_width);
void     sp_tui_table_header_row(sp_tui_table_t* table);
void     sp_tui_table_next_row(sp_tui_table_t* table);
void     sp_tui_table_column(sp_tui_table_t* table, u32 n);
void     sp_tui_table_column_named(sp_tui_table_t* table, sp_str_t name);
void     sp_tui_table_fmt(sp_tui_table_t* table, const c8* fmt, ...);
void     sp_tui_table_str(sp_tui_table_t* table, sp_str_t str);
void     sp_tui_table_set_indent(sp_tui_table_t* table, u32 indent);
void     sp_tui_table_end(sp_tui_table_t* table);
sp_str_t sp_tui_table_render(sp_tui_table_t* table);

#endif
