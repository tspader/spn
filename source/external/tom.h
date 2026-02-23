#ifndef SPN_TOML_H
#define SPN_TOML_H

#include "sp.h"
#include "toml.h"

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

u32               spn_toml_array_len(toml_array_t* array);
toml_table_t*     spn_toml_parse(sp_str_t path);
const c8*         spn_toml_cstr(toml_table_t* toml, const c8* key);
const c8*         spn_toml_cstr_opt(toml_table_t* toml, const c8* key, const c8* fallback);
const c8*         spn_toml_arr_cstr(toml_array_t* toml, u32 it);
sp_str_t          spn_toml_arr_str(toml_array_t* toml, u32 it);
sp_str_t          spn_toml_str(toml_table_t* toml, const c8* key);
sp_str_t          spn_toml_str_opt(toml_table_t* toml, const c8* key, const c8* fallback);
sp_da(sp_str_t)   spn_toml_arr_to_str_arr(toml_array_t* toml);
spn_toml_writer_t spn_toml_writer_new(void);
sp_str_t          spn_toml_writer_write(spn_toml_writer_t* writer);
void              spn_toml_ensure_header_written(spn_toml_writer_t* writer);
void              spn_toml_begin_table(spn_toml_writer_t* writer, sp_str_t key);
void              spn_toml_begin_table_cstr(spn_toml_writer_t* writer, const c8* key);
void              spn_toml_end_table(spn_toml_writer_t* writer);
void              spn_toml_begin_array(spn_toml_writer_t* writer, sp_str_t key);
void              spn_toml_begin_array_cstr(spn_toml_writer_t* writer, const c8* key);
void              spn_toml_end_array(spn_toml_writer_t* writer);
void              spn_toml_append_array_table(spn_toml_writer_t* writer);
void              spn_toml_append_str(spn_toml_writer_t* writer, sp_str_t key, sp_str_t value);
void              spn_toml_append_str_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t value);
void              spn_toml_append_s64(spn_toml_writer_t* writer, sp_str_t key, s64 value);
void              spn_toml_append_s64_cstr(spn_toml_writer_t* writer, const c8* key, s64 value);
void              spn_toml_append_bool(spn_toml_writer_t* writer, sp_str_t key, bool value);
void              spn_toml_append_bool_cstr(spn_toml_writer_t* writer, const c8* key, bool value);
void              spn_toml_append_str_array(spn_toml_writer_t* writer, sp_str_t key, sp_da(sp_str_t) values);
void              spn_toml_append_str_array_cstr(spn_toml_writer_t* writer, const c8* key, sp_da(sp_str_t) values);
void              spn_toml_append_str_carr(spn_toml_writer_t* writer, sp_str_t key, sp_str_t* values, u32 len);
void              spn_toml_append_str_carr_cstr(spn_toml_writer_t* writer, const c8* key, sp_str_t* values, u32 len);

#define spn_toml_arr_for(arr, it) for (u32 it = 0; it < spn_toml_array_len(arr); it++)
#define spn_toml_for(tbl, it, key) \
  for (s32 it = 0, SP_UNIQUE_ID() = 0; it < toml_table_len(tbl) && (key = toml_table_key(tbl, it, &SP_UNIQUE_ID())); it++)

#endif
