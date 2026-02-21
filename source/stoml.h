#ifndef SPN_TOML_H
#define SPN_TOML_H

typedef struct toml_table_t toml_table_t;
typedef struct toml_array_t toml_array_t;

typedef enum {
  SPN_TOML_CONTEXT_ROOT,
  SPN_TOML_CONTEXT_TABLE,
  SPN_TOML_CONTEXT_ARRAY,
} spn_toml_context_kind_t;

typedef struct {
  spn_toml_context_kind_t kind;
  sp_str_t key;
  bool header_written;
} spn_toml_context_t;

typedef struct {
  sp_str_builder_t builder;
  sp_da(spn_toml_context_t) stack;
} spn_toml_writer_t;

u32                spn_toml_array_len(toml_array_t* array);
toml_table_t*      spn_toml_parse(sp_str_t path);
const c8*          spn_toml_cstr(toml_table_t* toml, const c8* key);
const c8*          spn_toml_cstr_opt(toml_table_t* toml, const c8* key, const c8* fallback);
const c8*          spn_toml_arr_cstr(toml_array_t* toml, u32 it);
sp_str_t           spn_toml_arr_str(toml_array_t* toml, u32 it);
sp_str_t           spn_toml_str(toml_table_t* toml, const c8* key);
sp_str_t           spn_toml_str_opt(toml_table_t* toml, const c8* key, const c8* fallback);
sp_da(sp_str_t)    spn_toml_arr_to_str_arr(toml_array_t* toml);
spn_toml_writer_t  spn_toml_writer_new(void);
sp_str_t           spn_toml_writer_write(spn_toml_writer_t* writer);
void               spn_toml_ensure_header_written(spn_toml_writer_t* writer);
void               spn_toml_begin_table(spn_toml_writer_t* writer, sp_str_t key);
void               spn_toml_begin_table_cstr(spn_toml_writer_t* writer, const c8* key);
void               spn_toml_end_table(spn_toml_writer_t* writer);
void               spn_toml_begin_array(spn_toml_writer_t* writer, sp_str_t key);
void               spn_toml_begin_array_cstr(spn_toml_writer_t* writer, const c8* key);
void               spn_toml_end_array(spn_toml_writer_t* writer);
void               spn_toml_append_array_table(spn_toml_writer_t* writer);
void               spn_toml_append_str(spn_toml_writer_t* writer, sp_str_t key, sp_str_t value);
void               spn_toml_append_str_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t value);
void               spn_toml_append_s64(spn_toml_writer_t* writer, sp_str_t key, s64 value);
void               spn_toml_append_s64_cstr(spn_toml_writer_t* writer, const c8* key, s64 value);
void               spn_toml_append_bool(spn_toml_writer_t* writer, sp_str_t key, bool value);
void               spn_toml_append_bool_cstr(spn_toml_writer_t* writer, const c8* key, bool value);
void               spn_toml_append_str_array(spn_toml_writer_t* writer, sp_str_t key, sp_da(sp_str_t) values);
void               spn_toml_append_str_array_cstr(spn_toml_writer_t* writer, const c8* key, sp_da(sp_str_t) values);
void               spn_toml_append_str_carr(spn_toml_writer_t* writer, sp_str_t key, sp_str_t* values, u32 len);
void               spn_toml_append_str_carr_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t* values, u32 len);

#define spn_toml_arr_for(arr, it) for (u32 it = 0; it < spn_toml_array_len(arr); it++)
#define spn_toml_for(tbl, it, key) \
  for (s32 it = 0, SP_UNIQUE_ID() = 0; it < toml_table_len(tbl) && (key = toml_table_key(tbl, it, &SP_UNIQUE_ID())); it++)

#ifdef SPN_TOML_IMPLEMENTATION

u32 spn_toml_array_len(toml_array_t* array) {
  if (!array) {
    return 0;
  }

  return toml_array_len(array);
}

toml_table_t* spn_toml_parse(sp_str_t path) {
  if (!sp_fs_exists(path)) {
    return SP_NULLPTR;
  }

  sp_str_t file = sp_io_read_file(path);
  return toml_parse(sp_str_to_cstr(file), SP_NULLPTR, 0);
}

const c8* spn_toml_cstr(toml_table_t* toml, const c8* key) {
  toml_value_t value = toml_table_string(toml, key);
  SP_ASSERT_FMT(value.ok, "missing string key: {:fg brightcyan}", SP_FMT_CSTR(key));
  return value.u.s;
}

const c8* spn_toml_cstr_opt(toml_table_t* toml, const c8* key, const c8* fallback) {
  toml_value_t value = toml_table_string(toml, key);
  if (!value.ok) {
    return fallback;
  }

  return value.u.s;
}

const c8* spn_toml_arr_cstr(toml_array_t* toml, u32 it) {
  toml_value_t value = toml_array_string(toml, it);
  SP_ASSERT(value.ok);
  return value.u.s;
}

sp_str_t spn_toml_arr_str(toml_array_t* toml, u32 it) {
  return sp_str_view(spn_toml_arr_cstr(toml, it));
}

sp_str_t spn_toml_str(toml_table_t* toml, const c8* key) {
  return sp_str_view(spn_toml_cstr(toml, key));
}

sp_str_t spn_toml_str_opt(toml_table_t* toml, const c8* key, const c8* fallback) {
  return sp_str_view(spn_toml_cstr_opt(toml, key, fallback));
}

sp_da(sp_str_t) spn_toml_arr_to_str_arr(toml_array_t* toml) {
  if (!toml) {
    return SP_NULLPTR;
  }

  sp_da(sp_str_t) strs = SP_NULLPTR;
  spn_toml_arr_for(toml, it) {
    sp_dyn_array_push(strs, spn_toml_arr_str(toml, it));
  }

  return strs;
}

spn_toml_writer_t spn_toml_writer_new(void) {
  spn_toml_writer_t writer = SP_ZERO_INITIALIZE();

  spn_toml_context_t root = {
    .kind = SPN_TOML_CONTEXT_ROOT,
    .key = sp_str_lit(""),
    .header_written = true,
  };
  sp_dyn_array_push(writer.stack, root);

  return writer;
}

void spn_toml_ensure_header_written(spn_toml_writer_t* writer) {
  u32 depth = sp_dyn_array_size(writer->stack);
  SP_ASSERT(depth > 0);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  if (top->header_written) {
    return;
  }

  sp_dyn_array(sp_str_t) path_parts = SP_NULLPTR;
  for (u32 it = 1; it < depth; it++) {
    sp_dyn_array_push(path_parts, writer->stack[it].key);
  }

  sp_str_t path = sp_str_join_n(path_parts, sp_dyn_array_size(path_parts), sp_str_lit("."));
  if (top->kind == SPN_TOML_CONTEXT_TABLE) {
    sp_str_builder_append_fmt(&writer->builder, "[{}]", SP_FMT_STR(path));
  }

  sp_str_builder_new_line(&writer->builder);
  top->header_written = true;
}

void spn_toml_begin_table(spn_toml_writer_t* writer, sp_str_t key) {
  spn_toml_context_t context = {
    .kind = SPN_TOML_CONTEXT_TABLE,
    .key = key,
    .header_written = false,
  };
  sp_dyn_array_push(writer->stack, context);
}

void spn_toml_begin_table_cstr(spn_toml_writer_t* writer, const c8* key) {
  spn_toml_begin_table(writer, sp_str_view(key));
}

void spn_toml_end_table(spn_toml_writer_t* writer) {
  u32 depth = sp_dyn_array_size(writer->stack);
  SP_ASSERT(depth > 1);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  SP_ASSERT(top->kind == SPN_TOML_CONTEXT_TABLE);

  sp_dyn_array_pop(writer->stack);
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_begin_array(spn_toml_writer_t* writer, sp_str_t key) {
  spn_toml_context_t context = {
    .kind = SPN_TOML_CONTEXT_ARRAY,
    .key = key,
    .header_written = false,
  };
  sp_dyn_array_push(writer->stack, context);
}

void spn_toml_begin_array_cstr(spn_toml_writer_t* writer, const c8* key) {
  spn_toml_begin_array(writer, sp_str_view(key));
}

void spn_toml_end_array(spn_toml_writer_t* writer) {
  u32 depth = sp_dyn_array_size(writer->stack);
  SP_ASSERT(depth > 1);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  SP_ASSERT(top->kind == SPN_TOML_CONTEXT_ARRAY);

  sp_dyn_array_pop(writer->stack);
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_array_table(spn_toml_writer_t* writer) {
  u32 depth = sp_dyn_array_size(writer->stack);
  SP_ASSERT(depth > 1);

  spn_toml_context_t* top = &writer->stack[depth - 1];
  SP_ASSERT(top->kind == SPN_TOML_CONTEXT_ARRAY);

  if (top->header_written) {
    sp_str_builder_new_line(&writer->builder);
  }

  sp_dyn_array(sp_str_t) path_parts = SP_NULLPTR;
  for (u32 it = 1; it < depth; it++) {
    sp_dyn_array_push(path_parts, writer->stack[it].key);
  }

  sp_str_t path = sp_str_join_n(path_parts, sp_dyn_array_size(path_parts), sp_str_lit("."));
  sp_str_builder_append_fmt(&writer->builder, "[[{}]]", SP_FMT_STR(path));
  sp_str_builder_new_line(&writer->builder);

  top->header_written = true;
}

void spn_toml_append_str(spn_toml_writer_t* writer, sp_str_t key, sp_str_t value) {
  spn_toml_ensure_header_written(writer);
  sp_str_builder_append_fmt(
    &writer->builder,
    "{} = {}",
    SP_FMT_STR(key),
    SP_FMT_QUOTED_STR(value)
  );
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_str_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t value) {
  spn_toml_append_str(writer, sp_str_view(key), value);
}

void spn_toml_append_s64(spn_toml_writer_t* writer, sp_str_t key, s64 value) {
  spn_toml_ensure_header_written(writer);
  sp_str_builder_append_fmt(
    &writer->builder,
    "{} = {}",
    SP_FMT_STR(key),
    SP_FMT_S64(value)
  );
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_s64_cstr(spn_toml_writer_t* writer, const c8* key, s64 value) {
  spn_toml_append_s64(writer, sp_str_view(key), value);
}

void spn_toml_append_bool(spn_toml_writer_t* writer, sp_str_t key, bool value) {
  spn_toml_ensure_header_written(writer);
  sp_str_builder_append_fmt(
    &writer->builder,
    "{} = {}",
    SP_FMT_STR(key),
    SP_FMT_CSTR(value ? "true" : "false")
  );
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_bool_cstr(spn_toml_writer_t* writer, const c8* key, bool value) {
  spn_toml_append_bool(writer, sp_str_view(key), value);
}

void spn_toml_append_str_array(spn_toml_writer_t* writer, sp_str_t key, sp_da(sp_str_t) values) {
  spn_toml_ensure_header_written(writer);
  sp_str_builder_append_fmt(&writer->builder, "{} = [", SP_FMT_STR(key));

  u32 count = sp_dyn_array_size(values);
  for (u32 it = 0; it < count; it++) {
    sp_str_builder_append_fmt(&writer->builder, "{}", SP_FMT_QUOTED_STR(values[it]));
    if (it < count - 1) {
      sp_str_builder_append_cstr(&writer->builder, ", ");
    }
  }

  sp_str_builder_append_c8(&writer->builder, ']');
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_str_array_cstr(spn_toml_writer_t* writer, const c8* key, sp_da(sp_str_t) values) {
  spn_toml_append_str_array(writer, sp_str_view(key), values);
}

void spn_toml_append_str_carr(spn_toml_writer_t* writer, sp_str_t key, sp_str_t* values, u32 len) {
  spn_toml_ensure_header_written(writer);
  sp_str_builder_append_fmt(&writer->builder, "{} = [", SP_FMT_STR(key));

  for (u32 it = 0; it < len; it++) {
    sp_str_builder_append_fmt(&writer->builder, "{}", SP_FMT_QUOTED_STR(values[it]));
    if (it < len - 1) {
      sp_str_builder_append_cstr(&writer->builder, ", ");
    }
  }

  sp_str_builder_append_c8(&writer->builder, ']');
  sp_str_builder_new_line(&writer->builder);
}

void spn_toml_append_str_carr_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t* values, u32 len) {
  spn_toml_append_str_carr(writer, sp_str_view(key), values, len);
}

sp_str_t spn_toml_writer_write(spn_toml_writer_t* writer) {
  u32 depth = sp_dyn_array_size(writer->stack);
  SP_ASSERT(depth == 1);

  return sp_str_builder_to_str(&writer->builder);
}

#endif
#endif
