#include "sp.h"
#include "sp/macro.h"
#include "tui/table.h"

#include <stdarg.h>

static u32 sp_str_visual_len(sp_str_t str) {
  u32 visual_len = 0;
  bool in_escape = false;

  for (u32 it = 0; it < str.len; it++) {
    if (str.data[it] == '\033') {
      in_escape = true;
    } else if (in_escape && str.data[it] == 'm') {
      in_escape = false;
    } else if (!in_escape) {
      visual_len++;
    }
  }

  return visual_len;
}

static void sp_tui_apply_indent(sp_io_writer_t* w, u32 indent) {
  for (u32 it = 0; it < indent; it++) {
    sp_io_write_c8(w, ' ');
    sp_io_write_c8(w, ' ');
  }
}

void sp_tui_begin_table(sp_mem_t mem, sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_NONE);

  table->mem = mem;
  sp_da_init(mem, table->cols);
  sp_da_init(mem, table->rows);
  table->cursor = (sp_tui_cursor_t) { .row = 0, .col = 0 };
  table->state = SP_TUI_TABLE_SETUP;
  table->columns = 0;
  table->indent = 0;
}

void sp_tui_table_setup_column(sp_tui_table_t* table, sp_str_t name) {
  sp_tui_table_setup_column_ex(table, name, 0);
}

void sp_tui_table_setup_column_ex(sp_tui_table_t* table, sp_str_t name, u32 min_width) {
  SP_ASSERT(table->state == SP_TUI_TABLE_SETUP);
  sp_tui_column_t col = { .name = name, .min_width = min_width };
  sp_da_push(table->cols, col);
  table->columns++;
}

void sp_tui_table_header_row(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_SETUP);
  SP_ASSERT(table->columns > 0);
  table->state = SP_TUI_TABLE_BUILDING;
  table->cursor.row = 0;
  table->cursor.col = 0;
}

void sp_tui_table_next_row(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);

  table->cursor.row = sp_da_size(table->rows);
  sp_da(sp_str_t) new_row = sp_da_new(table->mem, sp_str_t);
  sp_da_push(table->rows, new_row);
  table->cursor.col = 0;
}

void sp_tui_table_column(sp_tui_table_t* table, u32 n) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);
  SP_ASSERT(n < table->columns);
  table->cursor.col = n;
}

void sp_tui_table_column_named(sp_tui_table_t* table, sp_str_t name) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);

  sp_da_for(table->cols, it) {
    if (sp_str_equal(table->cols[it].name, name)) {
      table->cursor.col = it;
      return;
    }
  }

  SP_ASSERT(false && "Column name not found");
}

void sp_tui_table_str(sp_tui_table_t* table, sp_str_t str) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);
  SP_ASSERT(table->cursor.col < table->columns);
  SP_ASSERT(table->cursor.row < sp_da_size(table->rows));

  sp_da(sp_str_t)* row = &table->rows[table->cursor.row];

  while (sp_da_size(*row) <= table->cursor.col) {
    sp_da_push(*row, sp_str_lit(""));
  }

  (*row)[table->cursor.col] = str;
  table->cursor.col++;
}

void sp_tui_table_fmt(sp_tui_table_t* table, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_fmt_mem_v(table->mem, sp_str_view(fmt), args).value;
  va_end(args);

  sp_tui_table_str(table, str);
}

void sp_tui_table_set_indent(sp_tui_table_t* table, u32 indent) {
  table->indent = indent;
}

void sp_tui_table_end(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);
  table->state = SP_TUI_TABLE_NONE;
}

sp_str_t sp_tui_table_render(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_NONE);

  if (table->columns == 0) {
    return sp_str_lit("");
  }

  sp_da(u32) widths = sp_da_new(table->mem, u32);
  for (u32 col = 0; col < table->columns; col++) {
    u32 max_width = table->cols[col].min_width;

    sp_da_for(table->rows, row_idx) {
      sp_da(sp_str_t)* row = &table->rows[row_idx];
      if (col < sp_da_size(*row)) {
        max_width = SP_MAX(max_width, sp_str_visual_len((*row)[col]));
      }
    }

    sp_da_push(widths, max_width);
  }

  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(table->mem, &w);

  sp_da_for(table->rows, row_idx) {
    sp_da(sp_str_t)* row = &table->rows[row_idx];
    sp_tui_apply_indent(&w.base, table->indent);

    for (u32 col = 0; col < table->columns; col++) {
      sp_str_t cell = (col < sp_da_size(*row)) ? (*row)[col] : sp_str_lit("");
      sp_io_write_str(&w.base, cell, SP_NULLPTR);

      u32 visual = sp_str_visual_len(cell);
      u32 pad = (visual < widths[col]) ? (widths[col] - visual) : 0;
      for (u32 p = 0; p < pad; p++) {
        sp_io_write_c8(&w.base, ' ');
      }

      if (col < table->columns - 1) {
        sp_io_write_c8(&w.base, ' ');
      }
    }
    sp_io_write_c8(&w.base, '\n');
  }

  return sp_io_dyn_mem_writer_take_str(&w);
}
