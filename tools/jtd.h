#ifndef JTD_H
#define JTD_H

#include "sp.h"

typedef enum {
  JTD_FORM_EMPTY,
  JTD_FORM_TYPE,
  JTD_FORM_ENUM,
  JTD_FORM_ELEMENTS,
  JTD_FORM_PROPERTIES,
  JTD_FORM_VALUES,
  JTD_FORM_DISCRIMINATOR,
  JTD_FORM_REF,
} jtd_form_t;

typedef enum {
  JTD_TYPE_BOOLEAN,
  JTD_TYPE_FLOAT32,
  JTD_TYPE_FLOAT64,
  JTD_TYPE_INT8,
  JTD_TYPE_UINT8,
  JTD_TYPE_INT16,
  JTD_TYPE_UINT16,
  JTD_TYPE_INT32,
  JTD_TYPE_UINT32,
  JTD_TYPE_STRING,
  JTD_TYPE_TIMESTAMP,
} jtd_type_t;

typedef struct jtd_schema jtd_schema_t;

typedef struct {
  sp_str_t key;
  jtd_schema_t* schema;
} jtd_property_t;

typedef struct {
  sp_str_t tag;
  jtd_schema_t* schema;
} jtd_mapping_t;

typedef struct {
  sp_str_t name;
  jtd_schema_t* schema;
} jtd_definition_t;

struct jtd_schema {
  jtd_form_t form;
  bool nullable;

  union {
    jtd_type_t type;

    struct {
      sp_da(sp_str_t) values;
    } enumeration;

    struct {
      jtd_schema_t* schema;
    } elements;

    struct {
      sp_da(jtd_property_t) required;
      sp_da(jtd_property_t) optional;
      bool additional;
    } properties;

    struct {
      jtd_schema_t* schema;
    } values;

    struct {
      sp_str_t tag;
      sp_da(jtd_mapping_t) mapping;
    } discriminator;

    struct {
      sp_str_t name;
    } ref;
  } as;
};

typedef enum {
  JTD_OK = 0,
  JTD_ERR_JSON,
  JTD_ERR_SCHEMA_NOT_OBJECT,
  JTD_ERR_MULTIPLE_FORMS,
  JTD_ERR_TYPE_NOT_STRING,
  JTD_ERR_UNKNOWN_TYPE,
  JTD_ERR_ENUM_EMPTY,
  JTD_ERR_ENUM_NOT_STRING,
  JTD_ERR_ENUM_DUPLICATE,
  JTD_ERR_PROPERTIES_NOT_OBJECT,
  JTD_ERR_DISCRIMINATOR_NOT_STRING,
  JTD_ERR_MAPPING_NOT_OBJECT,
  JTD_ERR_MAPPING_NOT_PROPERTIES,
  JTD_ERR_REF_NOT_STRING,
  JTD_ERR_METADATA_NOT_OBJECT,
  JTD_ERR_NULLABLE_NOT_BOOL,
  JTD_ERR_ADDITIONAL_PROPERTIES_NOT_BOOL,
  JTD_ERR_ADDITIONAL_PROPERTIES_WITHOUT_PROPERTIES,
  JTD_ERR_PROPERTIES_DUPLICATE,
  JTD_ERR_DISCRIMINATOR_TAG_REDEFINED,
  JTD_ERR_DEFINITIONS_NOT_ROOT,
  JTD_ERR_REF_UNRESOLVED,
  JTD_ERR_REF_CYCLE,
  JTD_ERR_UNKNOWN_MEMBER,
  JTD_ERR_UNRECOGNIZED_FORM,
} jtd_err_t;

typedef struct {
  jtd_err_t code;
  sp_str_t path;
  union {
    struct { sp_str_t name; } unknown_type;
    struct { sp_str_t value; } enum_duplicate;
    struct { sp_str_t name; } ref_unresolved;
    struct { sp_str_t name; } ref_cycle;
    struct { sp_str_t key; } unknown_member;
  } as;
} jtd_diagnostic_t;

typedef struct {
  sp_mem_arena_t* arena;
  bool ok;
  jtd_schema_t* root;
  sp_da(jtd_definition_t) definitions;
  jtd_diagnostic_t diag;
} jtd_result_t;

SP_API jtd_result_t jtd_parse(sp_mem_t mem, sp_str_t json);
SP_API void jtd_free(jtd_result_t* result);

SP_API jtd_schema_t* jtd_definition(const jtd_result_t* result, sp_str_t name);
SP_API jtd_schema_t* jtd_resolve(const jtd_result_t* result, jtd_schema_t* schema);
SP_API jtd_schema_t* jtd_resolve_deep(sp_mem_t mem, const jtd_result_t* result, jtd_schema_t* schema, jtd_diagnostic_t* diag);

typedef bool (*jtd_visit_fn)(jtd_schema_t* schema, sp_str_t path, void* user);
SP_API void jtd_walk(sp_mem_t mem, const jtd_result_t* result, jtd_visit_fn fn, void* user);

SP_API sp_str_t jtd_diagnostic_message(sp_mem_t mem, const jtd_diagnostic_t* diag);

SP_API const c8* jtd_err_name(jtd_err_t err);
SP_API const c8* jtd_form_name(jtd_form_t form);
SP_API const c8* jtd_type_name(jtd_type_t type);

#endif
