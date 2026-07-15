#include "sp.h"
#include "sp/macro.h"
#include "tom.h"

u32 spn_toml_array_len(toml_array_t* array) {
  if (!array) {
    return 0;
  }

  return toml_array_len(array);
}

toml_table_t* spn_toml_parse(sp_str_t path) {
  return spn_toml_parse_ex(path, SP_NULLPTR);
}

toml_table_t* spn_toml_parse_ex(sp_str_t path, bool* parse_error) {
  if (parse_error) {
    *parse_error = false;
  }

  if (!sp_fs_exists(path)) {
    return SP_NULLPTR;
  }

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t file = sp_zero;
  sp_io_read_file(scratch.mem, path, &file);
  c8 parse_err[1024] = {0};
  toml_table_t* toml = toml_parse(sp_str_to_cstr(scratch.mem, file), parse_err, SP_CARR_LEN(parse_err));
  sp_mem_end_scratch(scratch);

  if (!toml && parse_error) {
    *parse_error = true;
  }

  return toml;
}

static sp_str_t spn_toml_value_take(sp_mem_t mem, toml_value_t value) {
  sp_str_t result = sp_str_copy(mem, sp_str(value.u.s, (u32)value.u.sl));
  free(value.u.s);
  return result;
}

sp_str_t spn_toml_arr_str(sp_mem_t mem, toml_array_t* toml, u32 it) {
  toml_value_t value = toml_array_string(toml, it);
  SP_ASSERT(value.ok);
  return spn_toml_value_take(mem, value);
}

sp_str_t spn_toml_str(sp_mem_t mem, toml_table_t* toml, const c8* key) {
  toml_value_t value = toml_table_string(toml, key);
  SP_ASSERT_FMT(value.ok, "missing string key: {.cyan}", SP_FMT_CSTR(key));
  return spn_toml_value_take(mem, value);
}

sp_str_t spn_toml_str_opt(sp_mem_t mem, toml_table_t* toml, const c8* key, const c8* fallback) {
  toml_value_t value = toml_table_string(toml, key);
  if (!value.ok) {
    return sp_str_view(fallback);
  }
  return spn_toml_value_take(mem, value);
}

sp_da(sp_str_t) spn_toml_arr_to_str_arr(sp_mem_t mem, toml_array_t* toml) {
  if (!toml) {
    return SP_NULLPTR;
  }

  sp_da(sp_str_t) strs = sp_da_new(mem, sp_str_t);
  spn_toml_arr_for(toml, it) {
    sp_da_push(strs, spn_toml_arr_str(mem, toml, it));
  }

  return strs;
}

spn_toml_writer_t spn_toml_writer_new(sp_mem_t mem) {
  spn_toml_writer_t writer = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &writer.writer);
  sp_da_init(mem, writer.stack);

  spn_toml_context_t root = {
    .kind = SPN_TOML_CONTEXT_ROOT,
    .key = sp_str_lit(""),
    .header_written = true,
  };
  sp_da_push(writer.stack, root);

  return writer;
}

void spn_toml_ensure_header_written(spn_toml_writer_t* writer) {
  u32 depth = sp_da_size(writer->stack);
  SP_ASSERT(depth > 0);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  if (top->header_written) {
    return;
  }

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(writer->writer.allocator);
  sp_da(sp_str_t) path_parts = sp_da_new(scratch.mem, sp_str_t);
  for (u32 it = 1; it < depth; it++) {
    sp_da_push(path_parts, writer->stack[it].key);
  }

  sp_str_t path = sp_str_join_n(scratch.mem, path_parts, sp_da_size(path_parts), sp_str_lit("."));
  if (top->kind == SPN_TOML_CONTEXT_TABLE) {
    sp_fmt_io(&writer->writer.base, "[{}]", sp_fmt_str(path));
  }
  sp_mem_end_scratch(scratch);

  sp_io_write_c8(&writer->writer.base, '\n');
  top->header_written = true;
}

void spn_toml_begin_table(spn_toml_writer_t* writer, sp_str_t key) {
  spn_toml_context_t context = {
    .kind = SPN_TOML_CONTEXT_TABLE,
    .key = key,
    .header_written = false,
  };
  sp_da_push(writer->stack, context);
}

void spn_toml_begin_table_cstr(spn_toml_writer_t* writer, const c8* key) {
  spn_toml_begin_table(writer, sp_str_view(key));
}

void spn_toml_end_table(spn_toml_writer_t* writer) {
  u32 depth = sp_da_size(writer->stack);
  SP_ASSERT(depth > 1);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  SP_ASSERT(top->kind == SPN_TOML_CONTEXT_TABLE);

  sp_da_pop(writer->stack);
  sp_io_write_c8(&writer->writer.base, '\n');
}

void spn_toml_begin_array(spn_toml_writer_t* writer, sp_str_t key) {
  spn_toml_context_t context = {
    .kind = SPN_TOML_CONTEXT_ARRAY,
    .key = key,
    .header_written = false,
  };
  sp_da_push(writer->stack, context);
}

void spn_toml_begin_array_cstr(spn_toml_writer_t* writer, const c8* key) {
  spn_toml_begin_array(writer, sp_str_view(key));
}

void spn_toml_end_array(spn_toml_writer_t* writer) {
  u32 depth = sp_da_size(writer->stack);
  SP_ASSERT(depth > 1);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  SP_ASSERT(top->kind == SPN_TOML_CONTEXT_ARRAY);

  sp_da_pop(writer->stack);
  sp_io_write_c8(&writer->writer.base, '\n');
}

void spn_toml_append_array_table(spn_toml_writer_t* writer) {
  u32 depth = sp_da_size(writer->stack);
  SP_ASSERT(depth > 1);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  SP_ASSERT(top->kind == SPN_TOML_CONTEXT_ARRAY);

  if (top->header_written) {
    sp_io_write_c8(&writer->writer.base, '\n');
  }

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(writer->writer.allocator);
  sp_da(sp_str_t) path_parts = sp_da_new(scratch.mem, sp_str_t);
  for (u32 it = 1; it < depth; it++) {
    sp_da_push(path_parts, writer->stack[it].key);
  }

  sp_str_t path = sp_str_join_n(scratch.mem, path_parts, sp_da_size(path_parts), sp_str_lit("."));
  sp_fmt_io(&writer->writer.base, "[[{}]]", sp_fmt_str(path));
  sp_mem_end_scratch(scratch);
  sp_io_write_c8(&writer->writer.base, '\n');

  top->header_written = true;
}

void spn_toml_append_str(spn_toml_writer_t* writer, sp_str_t key, sp_str_t value) {
  spn_toml_ensure_header_written(writer);
  sp_fmt_io(
    &writer->writer.base,
    "{} = {.quote}",
    sp_fmt_str(key),
    sp_fmt_str(value)
  );
  sp_io_write_c8(&writer->writer.base, '\n');
}

void spn_toml_append_str_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t value) {
  spn_toml_append_str(writer, sp_str_view(key), value);
}

void spn_toml_append_s64(spn_toml_writer_t* writer, sp_str_t key, s64 value) {
  spn_toml_ensure_header_written(writer);
  sp_fmt_io(
    &writer->writer.base,
    "{} = {}",
    sp_fmt_str(key),
    sp_fmt_int(value)
  );
  sp_io_write_c8(&writer->writer.base, '\n');
}

void spn_toml_append_s64_cstr(spn_toml_writer_t* writer, const c8* key, s64 value) {
  spn_toml_append_s64(writer, sp_str_view(key), value);
}

void spn_toml_append_bool(spn_toml_writer_t* writer, sp_str_t key, bool value) {
  spn_toml_ensure_header_written(writer);
  sp_fmt_io(
    &writer->writer.base,
    "{} = {}",
    sp_fmt_str(key),
    sp_fmt_cstr(value ? "true" : "false")
  );
  sp_io_write_c8(&writer->writer.base, '\n');
}

void spn_toml_append_bool_cstr(spn_toml_writer_t* writer, const c8* key, bool value) {
  spn_toml_append_bool(writer, sp_str_view(key), value);
}

void spn_toml_append_str_array(spn_toml_writer_t* writer, sp_str_t key, sp_da(sp_str_t) values) {
  spn_toml_append_str_carr(writer, key, values, sp_da_size(values));
}

void spn_toml_append_str_array_cstr(spn_toml_writer_t* writer, const c8* key, sp_da(sp_str_t) values) {
  spn_toml_append_str_array(writer, sp_str_view(key), values);
}

void spn_toml_append_str_carr(spn_toml_writer_t* writer, sp_str_t key, sp_str_t* values, u32 len) {
  spn_toml_ensure_header_written(writer);
  sp_fmt_io(&writer->writer.base, "{} = [", sp_fmt_str(key));

  for (u32 it = 0; it < len; it++) {
    sp_fmt_io(&writer->writer.base, "{.quote}", sp_fmt_str(values[it]));
    if (it < len - 1) {
      sp_io_write_str(&writer->writer.base, sp_str_lit(", "), SP_NULLPTR);
    }
  }

  sp_io_write_c8(&writer->writer.base, ']');
  sp_io_write_c8(&writer->writer.base, '\n');
}

void spn_toml_append_str_carr_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t* values, u32 len) {
  spn_toml_append_str_carr(writer, sp_str_view(key), values, len);
}

sp_str_t spn_toml_writer_write(spn_toml_writer_t* writer) {
  u32 depth = sp_da_size(writer->stack);
  SP_ASSERT(depth == 1);

  return sp_io_dyn_mem_writer_take_str(&writer->writer);
}
