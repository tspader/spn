#include "jtd_test.h"

static void compare_empty(s32* utest_result, const jtd_result_t* root, const void* expect) {
  (void)expect;
  EXPECT_EQ((s32)JTD_FORM_EMPTY, (s32)root->root->form);
}

static void compare_nullable(s32* utest_result, const jtd_result_t* root, const void* expect) {
  (void)expect;
  EXPECT_EQ((s32)JTD_FORM_TYPE, (s32)root->root->form);
  EXPECT_EQ((s32)JTD_TYPE_STRING, (s32)root->root->as.type);
  EXPECT_TRUE(root->root->nullable);
}

UTEST(document, empty_form) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "document.empty.json",
    .compare = compare_empty,
  });
}

UTEST(document, nullable) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "document.nullable.json",
    .compare = compare_nullable,
  });
}

UTEST(document, metadata) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "document.metadata.json",
    .compare = compare_empty,
  });
}

UTEST(document, metadata_not_object) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "document.metadata_not_object.json",
    .error      = JTD_ERR_METADATA_NOT_OBJECT,
    .error_path = "#/metadata",
  });
}

UTEST(document, nullable_not_bool) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "document.nullable_not_bool.json",
    .error      = JTD_ERR_NULLABLE_NOT_BOOL,
    .error_path = "#",
  });
}

UTEST(document, multiple_forms) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "document.multiple_forms.json",
    .error      = JTD_ERR_MULTIPLE_FORMS,
    .error_path = "#",
  });
}

UTEST(document, unknown_key) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "document.unknown_key.json",
    .error      = JTD_ERR_UNKNOWN_MEMBER,
    .error_path = "#/foo",
  });
}

UTEST(document, root_not_object) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "document.root_not_object.json",
    .error      = JTD_ERR_SCHEMA_NOT_OBJECT,
    .error_path = "#",
  });
}

UTEST(document, invalid_json) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "document.invalid_json.json",
    .error      = JTD_ERR_JSON,
    .error_path = "#",
  });
}
