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

static sp_str_t sp_str_visual_pad(sp_str_t str, u32 target_visual_width) {
  u32 current_visual_len = sp_str_visual_len(str);
  s32 delta = (s32)target_visual_width - (s32)current_visual_len;

  if (delta <= 0) {
    return str;
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&builder, str);
  for (s32 it = 0; it < delta; it++) {
    sp_str_builder_append_c8(&builder, ' ');
  }

  return sp_str_builder_to_str(&builder);
}

static void sp_tui_apply_indent(sp_str_builder_t* builder, u32 indent) {
  for (u32 it = 0; it < indent; it++) {
    sp_str_builder_append_c8(builder, ' ');
    sp_str_builder_append_c8(builder, ' ');
  }
}

void sp_tui_begin_table(sp_tui_table_t* table) {
  SP_ASSERT(table->state == SP_TUI_TABLE_NONE);

  table->cols = SP_NULLPTR;
  table->rows = SP_NULLPTR;
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
  sp_dyn_array_push(table->cols, col);
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

  table->cursor.row = sp_dyn_array_size(table->rows);
  sp_da(sp_str_t) new_row = SP_NULLPTR;
  sp_dyn_array_push(table->rows, new_row);
  table->cursor.col = 0;
}

void sp_tui_table_column(sp_tui_table_t* table, u32 n) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);
  SP_ASSERT(n < table->columns);
  table->cursor.col = n;
}

void sp_tui_table_column_named(sp_tui_table_t* table, sp_str_t name) {
  SP_ASSERT(table->state == SP_TUI_TABLE_BUILDING);

  sp_dyn_array_for(table->cols, it) {
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
  SP_ASSERT(table->cursor.row < sp_dyn_array_size(table->rows));

  sp_da(sp_str_t)* row = &table->rows[table->cursor.row];

  while (sp_dyn_array_size(*row) <= table->cursor.col) {
    sp_dyn_array_push(*row, sp_str_lit(""));
  }

  (*row)[table->cursor.col] = str;
  table->cursor.col++;
}

void sp_tui_table_fmt(sp_tui_table_t* table, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t str = sp_format_v(SP_CSTR(fmt), args);
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

  sp_da(u32) widths = SP_NULLPTR;
  for (u32 col = 0; col < table->columns; col++) {
    u32 max_width = table->cols[col].min_width;

    sp_dyn_array_for(table->rows, row_idx) {
      sp_da(sp_str_t)* row = &table->rows[row_idx];
      if (col < sp_dyn_array_size(*row)) {
        max_width = SP_MAX(max_width, sp_str_visual_len((*row)[col]));
      }
    }

    sp_dyn_array_push(widths, max_width);
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  sp_dyn_array_for(table->rows, row_idx) {
    sp_da(sp_str_t)* row = &table->rows[row_idx];
    sp_tui_apply_indent(&builder, table->indent);

    for (u32 col = 0; col < table->columns; col++) {
      sp_str_t cell = (col < sp_dyn_array_size(*row)) ? (*row)[col] : sp_str_lit("");
      sp_str_t padded = sp_str_visual_pad(cell, widths[col]);

      sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(padded));

      if (col < table->columns - 1) {
        sp_str_builder_append_c8(&builder, ' ');
      }
    }
    sp_str_builder_new_line(&builder);
  }

  return sp_str_builder_to_str(&builder);
}
