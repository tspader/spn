#include "jtd_test.h"

static sp_err_t compare_properties_ok(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  sp_expect_eq(t, (s32)JTD_FORM_PROPERTIES, (s32)root->root->form);
  sp_expect_eq(t, (u64)2, (u64)sp_da_size(root->root->as.properties.required));
  sp_expect_eq(t, (u64)2, (u64)sp_da_size(root->root->as.properties.optional));
  sp_expect(t, root->root->as.properties.additional);

  if (sp_da_size(root->root->as.properties.required) == 2) {
    sp_expect_str_eq_c(t, root->root->as.properties.required[0].key, "name");
    sp_expect_eq(t, (s32)JTD_TYPE_STRING, (s32)root->root->as.properties.required[0].schema->as.type);
    sp_expect_str_eq_c(t, root->root->as.properties.required[1].key, "count");
    sp_expect_eq(t, (s32)JTD_TYPE_INT32, (s32)root->root->as.properties.required[1].schema->as.type);
  }
  if (sp_da_size(root->root->as.properties.optional) == 2) {
    sp_expect_str_eq_c(t, root->root->as.properties.optional[0].key, "age");
    sp_expect_eq(t, (s32)JTD_TYPE_INT32, (s32)root->root->as.properties.optional[0].schema->as.type);
    sp_expect_str_eq_c(t, root->root->as.properties.optional[1].key, "label");
    sp_expect_eq(t, (s32)JTD_TYPE_STRING, (s32)root->root->as.properties.optional[1].schema->as.type);
  }
  return SP_OK;
}

static sp_err_t compare_properties_additional_false(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  sp_expect_eq(t, (s32)JTD_FORM_PROPERTIES, (s32)root->root->form);
  sp_expect_eq(t, (u64)1, (u64)sp_da_size(root->root->as.properties.required));
  sp_expect(t, !root->root->as.properties.additional);
  return SP_OK;
}

static const jtd_case_t cases [] = {
  {
    .name    = "required_optional_additional",
    .json    = "properties.ok.json",
    .compare = compare_properties_ok,
  },
  {
    .name       = "not_object",
    .json       = "properties.not_object.json",
    .error      = JTD_ERR_PROPERTIES_NOT_OBJECT,
    .error_path = "#",
  },
  {
    .name       = "optional_not_object",
    .json       = "properties.optional_not_object.json",
    .error      = JTD_ERR_PROPERTIES_NOT_OBJECT,
    .error_path = "#",
  },
  {
    .name    = "additional_false",
    .json    = "properties.additional_false.json",
    .compare = compare_properties_additional_false,
  },
  {
    .name       = "additional_not_bool",
    .json       = "properties.additional_not_bool.json",
    .error      = JTD_ERR_ADDITIONAL_PROPERTIES_NOT_BOOL,
    .error_path = "#/additionalProperties",
  },
  {
    .name       = "additional_without_properties",
    .json       = "properties.additional_without_properties.json",
    .error      = JTD_ERR_ADDITIONAL_PROPERTIES_WITHOUT_PROPERTIES,
    .error_path = "#/additionalProperties",
  },
  {
    .name       = "duplicate_required_optional",
    .json       = "properties.duplicate_required_optional.json",
    .error      = JTD_ERR_PROPERTIES_DUPLICATE,
    .error_path = "#/optionalProperties/name",
  },
  {
    .name       = "nested_field_error_path",
    .json       = "properties.nested_error.json",
    .error      = JTD_ERR_UNKNOWN_TYPE,
    .error_path = "#/properties/x",
  },
  {
    .name       = "nested_field_escaped_path",
    .json       = "properties.nested_escaped_error.json",
    .error      = JTD_ERR_UNKNOWN_TYPE,
    .error_path = "#/properties/a~1b",
  },
  {
    .name       = "nested_optional_error_path",
    .json       = "properties.optional_nested_error.json",
    .error      = JTD_ERR_UNKNOWN_TYPE,
    .error_path = "#/optionalProperties/x",
  },
  {
    .name       = "nested_optional_escaped_path",
    .json       = "properties.optional_nested_escaped_error.json",
    .error      = JTD_ERR_UNKNOWN_TYPE,
    .error_path = "#/optionalProperties/a~0b",
  },
};

sp_test_each_fn(properties, parse, jtd_case_t, cases, run_jtd_case);
