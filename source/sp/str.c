#include "str.h"

static void sp_str_builder_init(sp_str_builder_t* b) {
  if (!b->initialized) {
    sp_io_dyn_mem_writer_init(spn_allocator, &b->writer);
    b->initialized = true;
  }
}

void sp_str_builder_append(sp_str_builder_t* b, sp_str_t str) {
  sp_str_builder_init(b);
  sp_io_write_str(&b->writer.base, str, SP_NULLPTR);
}

void sp_str_builder_append_cstr(sp_str_builder_t* b, const c8* str) {
  sp_str_builder_append(b, sp_str_view(str));
}

void sp_str_builder_append_c8(sp_str_builder_t* b, c8 c) {
  sp_str_builder_init(b);
  sp_io_write_c8(&b->writer.base, c);
}

void sp_str_builder_append_fmt(sp_str_builder_t* b, const c8* fmt, ...) {
  sp_str_builder_init(b);
  va_list args;
  va_start(args, fmt);
  sp_fmt_io_v(&b->writer.base, sp_str_view(fmt), args);
  va_end(args);
}

void sp_str_builder_append_fmt_str(sp_str_builder_t* b, sp_str_t fmt, ...) {
  sp_str_builder_init(b);
  va_list args;
  va_start(args, fmt);
  sp_fmt_io_v(&b->writer.base, fmt, args);
  va_end(args);
}

void sp_str_builder_new_line(sp_str_builder_t* b) {
  sp_str_builder_append_c8(b, '\n');
}

sp_str_t sp_str_builder_as_str(sp_str_builder_t* b) {
  sp_str_builder_init(b);
  return sp_io_dyn_mem_writer_as_str(&b->writer);
}

sp_str_t sp_str_builder_to_str(sp_str_builder_t* b) {
  return sp_str_builder_as_str(b);
}

void sp_str_builder_free(sp_str_builder_t* b) {
  if (b->initialized) {
    sp_io_dyn_mem_writer_close(&b->writer);
    b->initialized = false;
  }
}

bool sp_str_line_it_valid(const sp_str_line_it_t* it) {
  return !it->done;
}

void sp_str_line_it_next(sp_str_line_it_t* it) {
  if (it->cursor >= it->str.len) {
    it->done = true;
    it->line = sp_zero_s(sp_str_t);
    return;
  }

  u32 start = it->cursor;
  while (it->cursor < it->str.len && it->str.data[it->cursor] != '\n') {
    it->cursor++;
  }

  it->index = start;
  it->line = sp_str_sub(it->str, start, it->cursor - start);

  if (it->cursor < it->str.len) {
    it->cursor++;
  }
}

sp_str_line_it_t sp_str_line_it_begin(sp_str_t str) {
  sp_str_line_it_t it = {
    .str = str,
    .line = sp_zero_s(sp_str_t),
    .index = 0,
    .cursor = 0,
    .done = sp_str_empty(str),
  };

  if (!it.done) {
    sp_str_line_it_next(&it);
  }

  return it;
}

sp_str_t sp_format_color_id_to_ansi_fg(sp_str_t id) {
  if (sp_str_equal_cstr(id, "black")) return sp_str_lit(SP_ANSI_FG_BLACK);
  if (sp_str_equal_cstr(id, "red")) return sp_str_lit(SP_ANSI_FG_RED);
  if (sp_str_equal_cstr(id, "green")) return sp_str_lit(SP_ANSI_FG_GREEN);
  if (sp_str_equal_cstr(id, "yellow")) return sp_str_lit(SP_ANSI_FG_YELLOW);
  if (sp_str_equal_cstr(id, "blue")) return sp_str_lit(SP_ANSI_FG_BLUE);
  if (sp_str_equal_cstr(id, "magenta")) return sp_str_lit(SP_ANSI_FG_MAGENTA);
  if (sp_str_equal_cstr(id, "cyan")) return sp_str_lit(SP_ANSI_FG_CYAN);
  if (sp_str_equal_cstr(id, "white")) return sp_str_lit(SP_ANSI_FG_WHITE);
  if (sp_str_equal_cstr(id, "brightblack")) return sp_str_lit(SP_ANSI_FG_BRIGHT_BLACK);
  if (sp_str_equal_cstr(id, "brightred")) return sp_str_lit(SP_ANSI_FG_BRIGHT_RED);
  if (sp_str_equal_cstr(id, "brightgreen")) return sp_str_lit(SP_ANSI_FG_BRIGHT_GREEN);
  if (sp_str_equal_cstr(id, "brightyellow")) return sp_str_lit(SP_ANSI_FG_BRIGHT_YELLOW);
  if (sp_str_equal_cstr(id, "brightblue")) return sp_str_lit(SP_ANSI_FG_BRIGHT_BLUE);
  if (sp_str_equal_cstr(id, "brightmagenta")) return sp_str_lit(SP_ANSI_FG_BRIGHT_MAGENTA);
  if (sp_str_equal_cstr(id, "brightcyan")) return sp_str_lit(SP_ANSI_FG_BRIGHT_CYAN);
  if (sp_str_equal_cstr(id, "brightwhite")) return sp_str_lit(SP_ANSI_FG_BRIGHT_WHITE);
  return sp_str_lit(SP_ANSI_RESET);
}

sp_hash_t sp_hash_str(sp_str_t str) {
  return sp_hash_bytes(str.data, str.len, 0);
}

void sp_str_builder_indent(sp_str_builder_t* b) {
  (void)b;
}

void sp_str_builder_dedent(sp_str_builder_t* b) {
  (void)b;
}

sp_mem_buffer_t sp_str_builder_into_buffer(sp_str_builder_t* b) {
  sp_str_t str = sp_str_builder_to_str(b);
  return (sp_mem_buffer_t) { .data = (u8*)str.data, .len = str.len, .capacity = str.len };
}

sp_str_t sp_str_pad_ex(sp_str_t str, u32 n, c8 c) {
  if (str.len >= n) {
    return sp_str_copy(spn_allocator, str);
  }

  sp_str_builder_t builder = sp_zero;
  sp_str_builder_append(&builder, str);
  for (u32 it = str.len; it < n; it++) {
    sp_str_builder_append_c8(&builder, c);
  }

  return sp_str_builder_to_str(&builder);
}

sp_str_t sp_str_repeat(c8 c, u32 len) {
  if (!len) {
    return sp_zero_s(sp_str_t);
  }

  c8* buffer = (c8*)sp_alloc(spn_allocator, len);
  sp_mem_fill(buffer, len, &c, sizeof(c8));
  return sp_str(buffer, len);
}

sp_str_t sp_str_map_kernel_colorize(sp_str_map_context_t* context) {
  sp_str_t id = *(sp_str_t*)context->user_data;
  sp_str_t ansi = sp_format_color_id_to_ansi_fg(id);
  return sp_format("{}{}{}", SP_FMT_STR(ansi), SP_FMT_STR(context->str), SP_FMT_CSTR(SP_ANSI_RESET));
}
