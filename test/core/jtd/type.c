#include "jtd_test.h"

static void compare_type_primitives(s32* utest_result, const jtd_root_t* root, const void* expect) {
  (void)expect;
  EXPECT_EQ((s32)JTD_FORM_PROPERTIES, (s32)root->root->form);
  EXPECT_EQ((u64)11, (u64)sp_da_size(root->root->as.properties.required));
  sp_da_for(root->root->as.properties.required, i) {
    const jtd_property_t* p = &root->root->as.properties.required[i];
    EXPECT_EQ((s32)JTD_FORM_TYPE, (s32)p->schema->form);
    jtd_expect_str(utest_result, p->key, jtd_type_name(p->schema->as.type));
  }
}

UTEST(type, primitives) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "type.primitives.json",
    .compare = compare_type_primitives,
  });
}

UTEST(type, value_not_string) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "type.value_not_string.json",
    .error      = JTD_ERR_INVALID_TYPE,
    .error_path = "#",
  });
}

UTEST(type, unknown) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "type.unknown.json",
    .error      = JTD_ERR_INVALID_TYPE,
    .error_path = "#",
  });
}

UTEST(type, metadata_not_object) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "type.metadata_not_object.json",
    .error      = JTD_ERR_METADATA_NOT_OBJECT,
    .error_path = "#/metadata",
  });
}

UTEST(type, unknown_key) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "type.unknown_key.json",
    .error      = JTD_ERR_UNSUPPORTED,
    .error_path = "#/foo",
  });
}
