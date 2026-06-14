#include "jtd_test.h"

static void compare_properties_ok(s32* utest_result, const jtd_root_t* root, const void* expect) {
  (void)expect;
  EXPECT_EQ((s32)JTD_FORM_PROPERTIES, (s32)root->root->form);
  EXPECT_EQ((u64)2, (u64)sp_da_size(root->root->as.properties.required));
  EXPECT_EQ((u64)2, (u64)sp_da_size(root->root->as.properties.optional));
  EXPECT_TRUE(root->root->as.properties.additional);

  if (sp_da_size(root->root->as.properties.required) == 2) {
    jtd_expect_str(utest_result, root->root->as.properties.required[0].key, "name");
    EXPECT_EQ((s32)JTD_TYPE_STRING, (s32)root->root->as.properties.required[0].schema->as.type);
    jtd_expect_str(utest_result, root->root->as.properties.required[1].key, "count");
    EXPECT_EQ((s32)JTD_TYPE_INT32, (s32)root->root->as.properties.required[1].schema->as.type);
  }
  if (sp_da_size(root->root->as.properties.optional) == 2) {
    jtd_expect_str(utest_result, root->root->as.properties.optional[0].key, "age");
    EXPECT_EQ((s32)JTD_TYPE_INT32, (s32)root->root->as.properties.optional[0].schema->as.type);
    jtd_expect_str(utest_result, root->root->as.properties.optional[1].key, "label");
    EXPECT_EQ((s32)JTD_TYPE_STRING, (s32)root->root->as.properties.optional[1].schema->as.type);
  }
}

static void compare_properties_additional_false(s32* utest_result, const jtd_root_t* root, const void* expect) {
  (void)expect;
  EXPECT_EQ((s32)JTD_FORM_PROPERTIES, (s32)root->root->form);
  EXPECT_EQ((u64)1, (u64)sp_da_size(root->root->as.properties.required));
  EXPECT_FALSE(root->root->as.properties.additional);
}

UTEST(properties, required_optional_additional) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "properties.ok.json",
    .compare = compare_properties_ok,
  });
}

UTEST(properties, not_object) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "properties.not_object.json",
    .error      = JTD_ERR_PROPERTIES_NOT_OBJECT,
    .error_path = "#",
  });
}

UTEST(properties, optional_not_object) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "properties.optional_not_object.json",
    .error      = JTD_ERR_PROPERTIES_NOT_OBJECT,
    .error_path = "#",
  });
}

UTEST(properties, additional_false) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "properties.additional_false.json",
    .compare = compare_properties_additional_false,
  });
}

UTEST(properties, additional_not_bool) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "properties.additional_not_bool.json",
    .error      = JTD_ERR_ADDITIONAL_PROPERTIES_NOT_BOOL,
    .error_path = "#/additionalProperties",
  });
}

UTEST(properties, additional_without_properties) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "properties.additional_without_properties.json",
    .error      = JTD_ERR_ADDITIONAL_PROPERTIES_WITHOUT_PROPERTIES,
    .error_path = "#/additionalProperties",
  });
}

UTEST(properties, duplicate_required_optional) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "properties.duplicate_required_optional.json",
    .error      = JTD_ERR_PROPERTIES_DUPLICATE,
    .error_path = "#/optionalProperties/name",
  });
}

UTEST(properties, nested_field_error_path) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "properties.nested_error.json",
    .error      = JTD_ERR_INVALID_TYPE,
    .error_path = "#/properties/x",
  });
}

UTEST(properties, nested_field_escaped_path) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "properties.nested_escaped_error.json",
    .error      = JTD_ERR_INVALID_TYPE,
    .error_path = "#/properties/a~1b",
  });
}

UTEST(properties, nested_optional_error_path) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "properties.optional_nested_error.json",
    .error      = JTD_ERR_INVALID_TYPE,
    .error_path = "#/optionalProperties/x",
  });
}

UTEST(properties, nested_optional_escaped_path) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "properties.optional_nested_escaped_error.json",
    .error      = JTD_ERR_INVALID_TYPE,
    .error_path = "#/optionalProperties/a~0b",
  });
}
