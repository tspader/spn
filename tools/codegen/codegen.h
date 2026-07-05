#ifndef CODEGEN_H
#define CODEGEN_H

#include "sp.h"
#include "sp_om.h"
#include "jtd.h"
#include "sp_template.h"
#include "yyjson.h"

typedef enum {
  FIELD_STR,
  FIELD_BOOL,
  FIELD_ENUM,
  FIELD_STRUCT,
} field_kind_t;

typedef enum {
  CARD_SCALAR,
  CARD_ARRAY,
  CARD_MAP,
} cardinality_t;

typedef struct {
  sp_str_t key;
  bool required;
  field_kind_t kind;
  cardinality_t card;
  sp_str_t type_name;
  sp_str_t key_field;
  sp_str_t entry;
} field_t;

typedef struct {
  sp_str_t name;
  sp_da(field_t) fields;
  jtd_schema_t* schema;
  bool shared;
} type_t;

typedef struct {
  sp_str_t name;
  field_kind_t kind;
  sp_str_t object;
} entry_t;

typedef struct {
  sp_str_t object;
  sp_str_t key_field;
} om_type_t;

typedef struct {
  sp_mem_t mem;
  sp_str_om(type_t) types;
  sp_da(entry_t) entries;
  struct {
    sp_da(sp_str_t) array;
    sp_da(om_type_t) map;
    sp_da(sp_str_t) object;
  } containers;
  sp_da(sp_str_t) includes;
  sp_ht(sp_str_t, jtd_schema_t*) visited;
  type_t* root;
  sp_str_t err;
} gen_t;

typedef enum {
  ABI_VAL_VOID,
  ABI_VAL_S32,
  ABI_VAL_STR,
  ABI_VAL_STR_OPT,
  ABI_VAL_HANDLE,
  ABI_VAL_STRUCT,
} abi_val_kind_t;

typedef struct {
  abi_val_kind_t kind;
  sp_str_t name;
} abi_val_t;

typedef struct {
  sp_str_t name;
  abi_val_t val;
  u32 cap;
} abi_field_t;

typedef struct {
  sp_str_t name;
  bool fn;
  sp_da(abi_field_t) fields;
} abi_type_t;

typedef struct {
  sp_str_t name;
  sp_str_t host;
  bool wasm_ctx;
  abi_val_t ret;
  sp_da(abi_val_t) args;
} abi_export_t;

typedef struct {
  sp_mem_t mem;
  sp_ht(sp_str_t, u8) handles;
  sp_str_om(abi_type_t) types;
  sp_da(abi_export_t) exports;
  sp_str_t err;
} abi_t;

// extract.c
gen_t*  gen_new(sp_mem_t mem);
bool    gen_extract(gen_t* g, sp_str_t name, jtd_schema_t* schema);
type_t* gen_type(gen_t* g, sp_str_t name);

// render.c
bool cg_render(sp_mem_t mem, sp_str_t* err, sp_io_writer_t* io, sp_template_registry_t* reg, const c8* name, sp_template_scope_t* scope);
bool render_common(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg);
bool render_decls(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg);
bool render_impl(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg);

// abi.c
yyjson_alc cg_yyjson_alc(sp_mem_t* mem);
abi_t* abi_parse(sp_mem_t mem, sp_str_t path);
bool   abi_render_decls(abi_t* abi, sp_io_writer_t* io, sp_template_registry_t* reg);
bool   abi_render_impl(abi_t* abi, sp_io_writer_t* io, sp_template_registry_t* reg);

#endif // CODEGEN_H
