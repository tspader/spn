#include "jtd_test.h"

static sp_err_t compare_type_primitives(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  sp_expect_eq(t, (s32)JTD_FORM_PROPERTIES, (s32)root->root->form);
  sp_expect_eq(t, (u64)13, (u64)sp_da_size(root->root->as.properties.required));
  sp_da_for(root->root->as.properties.required, i) {
    const jtd_property_t* p = &root->root->as.properties.required[i];
    sp_expect_eq(t, (s32)JTD_FORM_TYPE, (s32)p->schema->form);
    sp_expect_str_eq_c(t, p->key, jtd_type_name(p->schema->as.type));
  }
  return SP_OK;
}

static const jtd_case_t cases [] = {
  {
    .name    = "primitives",
    .json    = "type.primitives.json",
    .compare = compare_type_primitives,
  },
  {
    .name       = "value_not_string",
    .json       = "type.value_not_string.json",
    .error      = JTD_ERR_TYPE_NOT_STRING,
    .error_path = "#",
  },
  {
    .name       = "unknown",
    .json       = "type.unknown.json",
    .error      = JTD_ERR_UNKNOWN_TYPE,
    .error_path = "#",
  },
  {
    .name       = "metadata_not_object",
    .json       = "type.metadata_not_object.json",
    .error      = JTD_ERR_METADATA_NOT_OBJECT,
    .error_path = "#/metadata",
  },
  {
    .name       = "unknown_key",
    .json       = "type.unknown_key.json",
    .error      = JTD_ERR_UNKNOWN_MEMBER,
    .error_path = "#/foo",
  },
};

sp_test_each_fn(type, parse, jtd_case_t, cases, run_jtd_case);
