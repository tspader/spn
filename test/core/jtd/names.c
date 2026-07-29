#include "jtd_test.h"

typedef struct {
  const c8* name;
  jtd_type_t type;
} type_name_t;

typedef struct {
  const c8* name;
  jtd_form_t form;
} form_name_t;

typedef struct {
  const c8* name;
  jtd_err_t err;
} err_name_t;

static const type_name_t type_names [] = {
  { "boolean",   JTD_TYPE_BOOLEAN },
  { "float32",   JTD_TYPE_FLOAT32 },
  { "float64",   JTD_TYPE_FLOAT64 },
  { "int8",      JTD_TYPE_INT8 },
  { "uint8",     JTD_TYPE_UINT8 },
  { "int16",     JTD_TYPE_INT16 },
  { "uint16",    JTD_TYPE_UINT16 },
  { "int32",     JTD_TYPE_INT32 },
  { "uint32",    JTD_TYPE_UINT32 },
  { "string",    JTD_TYPE_STRING },
  { "timestamp", JTD_TYPE_TIMESTAMP },
};

static const form_name_t form_names [] = {
  { "empty",         JTD_FORM_EMPTY },
  { "type",          JTD_FORM_TYPE },
  { "enum",          JTD_FORM_ENUM },
  { "elements",      JTD_FORM_ELEMENTS },
  { "properties",    JTD_FORM_PROPERTIES },
  { "values",        JTD_FORM_VALUES },
  { "discriminator", JTD_FORM_DISCRIMINATOR },
  { "ref",           JTD_FORM_REF },
};

static const err_name_t err_names [] = {
  { "ok",                                       JTD_OK },
  { "invalid-json",                             JTD_ERR_JSON },
  { "schema-not-object",                        JTD_ERR_SCHEMA_NOT_OBJECT },
  { "multiple-forms",                           JTD_ERR_MULTIPLE_FORMS },
  { "type-not-string",                          JTD_ERR_TYPE_NOT_STRING },
  { "unknown-type",                             JTD_ERR_UNKNOWN_TYPE },
  { "enum-empty",                               JTD_ERR_ENUM_EMPTY },
  { "enum-not-string",                          JTD_ERR_ENUM_NOT_STRING },
  { "enum-duplicate",                           JTD_ERR_ENUM_DUPLICATE },
  { "properties-not-object",                    JTD_ERR_PROPERTIES_NOT_OBJECT },
  { "discriminator-not-string",                 JTD_ERR_DISCRIMINATOR_NOT_STRING },
  { "mapping-not-object",                       JTD_ERR_MAPPING_NOT_OBJECT },
  { "mapping-not-properties",                   JTD_ERR_MAPPING_NOT_PROPERTIES },
  { "ref-not-string",                           JTD_ERR_REF_NOT_STRING },
  { "metadata-not-object",                      JTD_ERR_METADATA_NOT_OBJECT },
  { "nullable-not-bool",                        JTD_ERR_NULLABLE_NOT_BOOL },
  { "additional-properties-not-bool",           JTD_ERR_ADDITIONAL_PROPERTIES_NOT_BOOL },
  { "additional-properties-without-properties", JTD_ERR_ADDITIONAL_PROPERTIES_WITHOUT_PROPERTIES },
  { "properties-duplicate",                     JTD_ERR_PROPERTIES_DUPLICATE },
  { "discriminator-tag-redefined",              JTD_ERR_DISCRIMINATOR_TAG_REDEFINED },
  { "definitions-not-root",                     JTD_ERR_DEFINITIONS_NOT_ROOT },
  { "ref-unresolved",                           JTD_ERR_REF_UNRESOLVED },
  { "ref-cycle",                                JTD_ERR_REF_CYCLE },
  { "unknown-member",                           JTD_ERR_UNKNOWN_MEMBER },
  { "unrecognized-form",                        JTD_ERR_UNRECOGNIZED_FORM },
};

sp_test_each(names, types, type_name_t, type_names) {
  sp_expect_str_eq_c(t, sp_str_view(jtd_type_name(it->type)), it->name);
  return SP_OK;
}

sp_test_each(names, forms, form_name_t, form_names) {
  sp_expect_str_eq_c(t, sp_str_view(jtd_form_name(it->form)), it->name);
  return SP_OK;
}

sp_test_each(names, errors, err_name_t, err_names) {
  sp_expect_str_eq_c(t, sp_str_view(jtd_err_name(it->err)), it->name);
  return SP_OK;
}
