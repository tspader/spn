#include "jtd_test.h"

static sp_err_t compare_values_ok(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  sp_expect_eq(t, (s32)JTD_FORM_VALUES, (s32)root->root->form);
  sp_expect_eq(t, (s32)JTD_FORM_TYPE, (s32)root->root->as.values.schema->form);
  sp_expect_eq(t, (s32)JTD_TYPE_BOOLEAN, (s32)root->root->as.values.schema->as.type);
  return SP_OK;
}

static const jtd_case_t cases [] = {
  {
    .name    = "ok",
    .json    = "values.ok.json",
    .compare = compare_values_ok,
  },
  {
    .name       = "nested_error_path",
    .json       = "values.nested_error.json",
    .error      = JTD_ERR_ENUM_EMPTY,
    .error_path = "#/values",
  },
  {
    .name       = "child_not_object",
    .json       = "values.child_not_object.json",
    .error      = JTD_ERR_SCHEMA_NOT_OBJECT,
    .error_path = "#/values",
  },
};

sp_test_each_fn(values, parse, jtd_case_t, cases, run_jtd_case);
