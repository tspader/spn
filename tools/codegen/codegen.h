#ifndef CODEGEN_H
#define CODEGEN_H

#include "sp.h"
#include "sp_om.h"
#include "jtd.h"
#include "sp_template.h"

typedef enum {
  FIELD_STR,
  FIELD_BOOL,
  FIELD_CONV,
  FIELD_STR_ARRAY,
  FIELD_OBJECT_ARRAY,
  FIELD_OBJECT,
  FIELD_OBJECT_PTR,
  FIELD_MAP_STR,
  FIELD_MAP_OBJECT,
} field_kind_t;

typedef struct {
  sp_str_t name;
  sp_str_t type;
  sp_str_t from;
  sp_str_t to;
} conv_t;

typedef struct {
  sp_str_t key;
  field_kind_t kind;
  bool required;
  sp_str_t object;
  sp_str_t entry;
  conv_t* conv;
} field_t;

typedef struct {
  sp_str_t name;
  sp_da(field_t) fields;
} type_t;

typedef struct {
  sp_str_t name;
  sp_str_t value_type;
} entry_t;

typedef struct {
  sp_mem_t mem;
  sp_str_om(type_t) types;
  sp_da(entry_t) entries;
  sp_str_om(type_t*) array_types;
  sp_str_om(conv_t) convs;
  sp_ht(sp_str_t, u8) visited;
  type_t* root;
} gen_t;

// walk.c
typedef enum {
  WALK_OK = 0,
  WALK_ERR_SCALAR_TYPE,
  WALK_ERR_ELEMENT_FORM,
  WALK_ERR_MAP_VALUE_FORM,
  WALK_ERR_SCHEMA_FORM,
  WALK_ERR_CONV_BINDING,
} walk_err_t;

typedef struct {
  walk_err_t err;
  sp_str_t type;
  sp_str_t key;
  union {
    jtd_type_t scalar_type;
    jtd_form_t form;
  } as;
} walk_result_t;

walk_result_t register_type(gen_t* g, jtd_ref_t ref);
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
render_result_t render_file(gen_t* g, sp_io_writer_t* out, sp_template_registry_t* reg);
sp_str_t        render_result_to_str(sp_mem_t mem, render_result_t result);

#endif // CODEGEN_H
