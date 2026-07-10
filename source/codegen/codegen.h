#ifndef SPN_CODEGEN_H
#define SPN_CODEGEN_H

#include "codegen/types.h"
#include "error/types.h"
#include "when/types.h"
#include "sp.h"
#include "spn.h"
#include "toml.h"

void               spn_toml_loader_push_key(spn_toml_loader_t* t, const c8* key);
void               spn_toml_loader_push_index(spn_toml_loader_t* t, u32 index);
void               spn_toml_loader_pop(spn_toml_loader_t* t);
bool               spn_toml_loader_issue(spn_toml_loader_t* t, spn_err_t code, const c8* key);
bool               spn_toml_loader_issue_at(spn_toml_loader_t* t, spn_err_t code, sp_str_t detail);
sp_str_t           spn_toml_loader_intern(spn_toml_loader_t* t, sp_str_t value);
bool               spn_toml_loader_field_present(toml_table_t* table, const c8* key);
sp_str_t           spn_toml_loader_str_required(spn_toml_loader_t* t, toml_table_t* table, const c8* k);
bool               spn_toml_loader_str_optional(spn_toml_loader_t* t, toml_table_t* table, const c8* k, sp_str_t* v);
sp_str_t           spn_toml_loader_raw_required(spn_toml_loader_t* t, toml_table_t* table, const c8* k);
bool               spn_toml_loader_raw_optional(spn_toml_loader_t* t, toml_table_t* table, const c8* k, sp_str_t* v);
bool               spn_toml_loader_read_bool(spn_toml_loader_t* t, toml_table_t* table, const c8* k, bool* v);
sp_da(sp_str_t)    spn_toml_loader_read_str_array(spn_toml_loader_t* t, toml_table_t* table, const c8* key);
spn_option_value_t spn_toml_loader_value_str(spn_toml_loader_t* t, toml_value_t value);
spn_option_value_t spn_toml_loader_value_bool(toml_value_t value);
void               spn_toml_loader_write_option_value(sp_io_writer_t* out, spn_option_value_t value);
void               spn_toml_loader_read_when(spn_toml_loader_t* t, toml_table_t* table, const c8* key, spn_when_t* out);
void               spn_toml_loader_read_option_defaults(spn_toml_loader_t* t, toml_table_t* table, const c8* k, spn_option_defaults_t* out);

typedef struct yyjson_val yyjson_val;
spn_option_value_t spn_json_option_value(yyjson_val* value, sp_mem_t mem);
void spn_json_read_when(yyjson_val* obj, const c8* key, spn_when_t* out, sp_mem_t mem);
void spn_json_read_option_defaults(yyjson_val* obj, const c8* key, spn_option_defaults_t* out, sp_mem_t mem);

void spn_codegen_write_option_defaults(sp_io_writer_t* out, const spn_option_defaults_t* in);

bool spn_codegen_option_defaults_present(const spn_option_defaults_t* in);
void spn_codegen_write_when(sp_io_writer_t* out, const spn_when_t* in);
bool spn_codegen_when_present(const spn_when_t* in);

const c8* spn_codegen_err_name(spn_err_t code);

typedef struct {
  sp_io_writer_t base;
  sp_io_writer_t* inner;
  u32 depth;
  bool in_string;
  bool escape;
  bool pending;
} spn_codegen_json_writer_t;

void spn_codegen_json_writer_init(spn_codegen_json_writer_t* writer, sp_io_writer_t* inner);

void spn_codegen_json_key(sp_io_writer_t* out, bool* first, sp_str_t key);
void spn_codegen_json_str(sp_io_writer_t* out, sp_str_t value);
void spn_codegen_json_bool(sp_io_writer_t* out, bool value);
void spn_codegen_json_str_array(sp_io_writer_t* out, sp_da(sp_str_t) values);
void spn_codegen_json_issues(sp_io_writer_t* out, sp_da(spn_codegen_issue_t) issues);
sp_str_t spn_codegen_issues_to_str(sp_mem_t mem, sp_da(spn_codegen_issue_t) issues);

#endif
