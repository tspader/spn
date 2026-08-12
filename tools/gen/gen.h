#ifndef GEN_H
#define GEN_H

#include "sp.h"
#include "jtd.h"
#include "sp_template.h"
#include "yyjson.h"
#include "codegen.h"

typedef enum {
  PRIM_BOOL,
  PRIM_U64,
} gen_prim_t;

typedef enum {
  FIELD_STR,
  FIELD_PRIM,
  FIELD_ENUM,
  FIELD_OBJECT,
  FIELD_EXTERN,
  FIELD_STR_ARRAY,
  FIELD_ENUM_ARRAY,
  FIELD_OBJECT_ARRAY,
  FIELD_KEYED,
  FIELD_MAP,
} gen_field_kind_t;

typedef struct gen_type gen_type_t;

typedef struct {
  sp_str_t name;
  gen_type_t* object;
  sp_str_t shorthand;
} gen_entry_t;

typedef struct {
  gen_field_kind_t kind;
  sp_str_t key;
  sp_str_t name;
  bool required;
  union {
    gen_prim_t prim;
    sp_str_t enumeration;
    gen_type_t* object;
    sp_str_t ext;
    struct {
      gen_type_t* object;
      sp_str_t shorthand;
    } array;
    struct {
      gen_type_t* object;
      sp_str_t field;
    } keyed;
    gen_entry_t* map;
  } as;
} gen_field_t;

struct gen_type {
  sp_str_t name;
  sp_da(gen_field_t) fields;
  jtd_schema_t* schema;
  bool shared;
};

typedef struct {
  jtd_schema_t* schema;
  gen_type_t* type;
} gen_visit_t;

typedef struct {
  gen_type_t* object;
  sp_str_t field;
} gen_shorthand_t;

typedef struct {
  gen_type_t* object;
  sp_str_t key_field;
} gen_keyed_t;

typedef enum {
  GEN_FORMAT_TOML,
  GEN_FORMAT_JSON,
} gen_format_t;

typedef struct {
  sp_mem_t mem;
  sp_da(gen_type_t*) types;
  sp_ht(sp_str_t, gen_visit_t) visited;
  struct {
    sp_da(gen_type_t*) array;
    sp_da(gen_shorthand_t) shorthand;
    sp_da(gen_keyed_t) keyed;
    sp_da(gen_type_t*) object;
  } containers;
  sp_da(gen_entry_t*) entries;
  sp_da(sp_str_t) includes;
  gen_type_t* root;
  gen_format_t format;
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
  sp_da(abi_type_t*) types;
  sp_da(abi_export_t) exports;
  sp_str_t err;
} abi_t;

gen_t*      gen_new(sp_mem_t mem);
bool        gen_lower(gen_t* g, sp_str_t name, jtd_schema_t* schema);
gen_type_t* gen_find(gen_t* g, sp_str_t name);

bool gen_render_common(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg);
bool gen_render_decls(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg);
bool gen_render_impl(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg);

yyjson_alc gen_yyjson_alc(sp_mem_t* mem);
abi_t* abi_parse(sp_mem_t mem, sp_str_t path);
bool   abi_render_decls(abi_t* abi, sp_io_writer_t* io, sp_template_registry_t* reg);
bool   abi_render_impl(abi_t* abi, sp_io_writer_t* io, sp_template_registry_t* reg);

#endif
