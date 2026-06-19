#include "jtd_test.h"

static void compare_discriminator_ok(s32* utest_result, const jtd_result_t* root, const void* expect) {
  (void)expect;
  EXPECT_EQ((s32)JTD_FORM_DISCRIMINATOR, (s32)root->root->form);
  jtd_expect_str(utest_result, root->root->as.discriminator.tag, "kind");
  EXPECT_EQ((u64)2, (u64)sp_da_size(root->root->as.discriminator.mapping));
  if (sp_da_size(root->root->as.discriminator.mapping) == 2) {
    jtd_expect_str(utest_result, root->root->as.discriminator.mapping[0].tag, "a");
    jtd_expect_str(utest_result, root->root->as.discriminator.mapping[1].tag, "b");
    EXPECT_EQ((s32)JTD_FORM_PROPERTIES, (s32)root->root->as.discriminator.mapping[0].schema->form);
    EXPECT_EQ((s32)JTD_FORM_PROPERTIES, (s32)root->root->as.discriminator.mapping[1].schema->form);
  }
}

UTEST(discriminator, ok) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "discriminator.ok.json",
    .compare = compare_discriminator_ok,
  });
}

UTEST(discriminator, not_string) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "discriminator.not_string.json",
    .error      = JTD_ERR_DISCRIMINATOR_NOT_STRING,
    .error_path = "#",
  });
}

UTEST(discriminator, mapping_not_object) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "discriminator.mapping_not_object.json",
    .error      = JTD_ERR_MAPPING_NOT_OBJECT,
    .error_path = "#",
  });
}

UTEST(discriminator, mapping_value_not_properties) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "discriminator.mapping_not_properties.json",
    .error      = JTD_ERR_MAPPING_NOT_PROPERTIES,
    .error_path = "#/mapping/a",
  });
}

UTEST(discriminator, mapping_escaped_path) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "discriminator.mapping_escaped_path.json",
    .error      = JTD_ERR_MAPPING_NOT_PROPERTIES,
    .error_path = "#/mapping/a~1b",
  });
}

UTEST(discriminator, mapping_value_nullable) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "discriminator.mapping_nullable.json",
    .error      = JTD_ERR_MAPPING_NOT_PROPERTIES,
    .error_path = "#/mapping/a",
  });
}

UTEST(discriminator, tag_in_properties) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "discriminator.tag_in_properties.json",
    .error      = JTD_ERR_DISCRIMINATOR_TAG_REDEFINED,
    .error_path = "#/mapping/a/properties/kind",
  });
}

UTEST(discriminator, tag_in_optional_properties) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "discriminator.tag_in_optional_properties.json",
    .error      = JTD_ERR_DISCRIMINATOR_TAG_REDEFINED,
    .error_path = "#/mapping/a/optionalProperties/kind",
  });
}
