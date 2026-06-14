#include "jtd_test.h"

static void compare_values_ok(s32* utest_result, const jtd_root_t* root, const void* expect) {
  (void)expect;
  EXPECT_EQ((s32)JTD_FORM_VALUES, (s32)root->root->form);
  EXPECT_EQ((s32)JTD_FORM_TYPE, (s32)root->root->as.values.schema->form);
  EXPECT_EQ((s32)JTD_TYPE_BOOLEAN, (s32)root->root->as.values.schema->as.type);
}

UTEST(values, ok) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "values.ok.json",
    .compare = compare_values_ok,
  });
}

UTEST(values, nested_error_path) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "values.nested_error.json",
    .error      = JTD_ERR_ENUM_EMPTY,
    .error_path = "#/values",
  });
}

UTEST(values, child_not_object) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "values.child_not_object.json",
    .error      = JTD_ERR_SCHEMA_NOT_OBJECT,
    .error_path = "#/values",
  });
}
