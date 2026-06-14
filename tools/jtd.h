#ifndef JTD_H
#define JTD_H

#include "sp.h"
#include "yyjson.h"

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
  sp_str_t      key;
  jtd_schema_t* schema;
} jtd_property_t;

typedef struct {
  sp_str_t      tag;
  jtd_schema_t* schema;
} jtd_mapping_t;

typedef struct {
  sp_str_t      name;
  jtd_schema_t* schema;
} jtd_definition_t;

struct jtd_schema {
  jtd_form_t form;
  bool       nullable;

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
      bool                  additional;
    } properties;

    struct {
      jtd_schema_t* schema;
    } values;

    struct {
      sp_str_t             tag;
      sp_da(jtd_mapping_t) mapping;
    } discriminator;

    struct {
      sp_str_t name;
    } ref;
  } as;
};

typedef struct {
  jtd_schema_t*           root;
  sp_da(jtd_definition_t) definitions;
} jtd_root_t;

typedef enum {
  JTD_OK = 0,
  JTD_ERR_JSON,
  JTD_ERR_SCHEMA_NOT_OBJECT,
  JTD_ERR_MULTIPLE_FORMS,
  JTD_ERR_INVALID_TYPE,
  JTD_ERR_ENUM_EMPTY,
  JTD_ERR_ENUM_NOT_STRING,
  JTD_ERR_ENUM_DUPLICATE,
  JTD_ERR_ELEMENTS_NOT_SCHEMA,
  JTD_ERR_VALUES_NOT_SCHEMA,
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
  JTD_ERR_UNSUPPORTED,
} jtd_err_t;

typedef struct {
  jtd_err_t code;
  sp_str_t  message;
  sp_str_t  path;
} jtd_diagnostic_t;

bool jtd_parse(sp_str_t json, jtd_root_t* out, jtd_diagnostic_t* diag);
bool jtd_parse_val(yyjson_val* root, jtd_root_t* out, jtd_diagnostic_t* diag);

jtd_schema_t* jtd_definition(const jtd_root_t* root, sp_str_t name);
jtd_schema_t* jtd_resolve(const jtd_root_t* root, jtd_schema_t* schema);
jtd_schema_t* jtd_resolve_deep(const jtd_root_t* root, jtd_schema_t* schema, jtd_diagnostic_t* diag);

typedef bool (*jtd_visit_fn)(jtd_schema_t* schema, sp_str_t path, void* user);
void jtd_walk(const jtd_root_t* root, jtd_visit_fn fn, void* user);

const c8* jtd_err_name(jtd_err_t err);
const c8* jtd_form_name(jtd_form_t form);
const c8* jtd_type_name(jtd_type_t type);

#endif
