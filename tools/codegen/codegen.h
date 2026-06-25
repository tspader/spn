#ifndef CODEGEN_H
#define CODEGEN_H

#include "sp.h"
#include "sp_om.h"
#include "jtd.h"
#include "sp_template.h"

typedef enum {
  CARD_SCALAR,
  CARD_ARRAY,
  CARD_MAP,
} cardinality_t;

typedef enum {
  NODE_STR,
  NODE_BOOL,
  NODE_CONVERSION,
  NODE_STRUCT,
} node_kind_t;

typedef struct type_t type_t;

typedef struct {
  sp_str_t c_type;
  sp_str_t from;
  sp_str_t to;
  sp_str_t present;
  bool custom;
} converter_t;

typedef struct {
  node_kind_t kind;
  sp_str_t name;
  bool use_optional;
  union {
    converter_t conv;
    type_t* type;
  } as;
} node_t;

typedef struct {
  sp_str_t key;
  bool required;
  cardinality_t card;
  node_t* node;
  sp_str_t entry;
  sp_str_t key_field;
  sp_str_t validate;
} field_t;

struct type_t {
  sp_str_t name;
  sp_da(field_t) fields;
  sp_str_t validate;
};

typedef struct {
  sp_str_t name;
  sp_str_t value_type;
  sp_str_t object;
} entry_t;

typedef struct {
  type_t* type;
  sp_str_t key_field;
} om_type_t;

typedef struct {
  sp_mem_t mem;
  sp_str_om(type_t) types;
  sp_da(entry_t) entries;
  sp_str_om(type_t*) array_types;
  sp_str_om(om_type_t) om_types;
  sp_str_om(type_t*) object_types;
  sp_str_om(node_t) nodes;
  sp_ht(sp_str_t, u8) visited;
  type_t* root;
} gen_t;

// walk.c
typedef enum {
  WALK_OK = 0,
  WALK_ERR_SCALAR_TYPE,
  WALK_ERR_CONV_UNKNOWN,
  WALK_ERR_UNSUPPORTED_FORM,
} walk_err_t;

typedef struct {
  walk_err_t err;
  sp_str_t type;
  sp_str_t key;
  sp_str_t name;
  union {
    jtd_type_t scalar_type;
    jtd_form_t form;
  } as;
} walk_result_t;

walk_result_t register_type(gen_t* g, sp_str_t name, jtd_schema_t* schema);
sp_str_t      walk_result_to_str(sp_mem_t mem, walk_result_t result);

// render.c
typedef enum {
  RENDER_OK = 0,
  RENDER_ERR_TEMPLATE_MISSING,
  RENDER_ERR_TEMPLATE_RENDER,
} render_err_t;

typedef struct {
  render_err_t err;
  sp_str_t tmpl;
  sp_template_err_t code;
} render_result_t;

type_t*         find_type(gen_t* g, sp_str_t name);
sp_str_t        node_c_type(gen_t* g, node_t* node);
render_result_t render_file(gen_t* g, sp_io_writer_t* out, sp_template_registry_t* reg);
sp_str_t        render_result_to_str(sp_mem_t mem, render_result_t result);

#endif // CODEGEN_H
