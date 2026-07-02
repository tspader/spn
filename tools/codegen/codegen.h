#ifndef CODEGEN_H
#define CODEGEN_H

#include "sp.h"
#include "sp_om.h"
#include "jtd.h"
#include "sp_template.h"

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
  sp_ht(sp_str_t, u8) visited;
  type_t* root;
  sp_str_t err;
} gen_t;

// extract.c
gen_t*  gen_new(sp_mem_t mem);
bool    gen_extract(gen_t* g, sp_str_t name, jtd_schema_t* schema);
type_t* gen_type(gen_t* g, sp_str_t name);

// render.c
bool render_types(gen_t* g, sp_io_writer_t* out, sp_template_registry_t* reg);
bool render_decls(gen_t* g, sp_io_writer_t* out, sp_template_registry_t* reg);
bool render_impl(gen_t* g, sp_io_writer_t* out, sp_template_registry_t* reg);

#endif // CODEGEN_H
