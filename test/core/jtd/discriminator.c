#include "jtd_test.h"

static sp_err_t compare_discriminator_ok(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  sp_expect_eq(t, (s32)JTD_FORM_DISCRIMINATOR, (s32)root->root->form);
  sp_expect_str_eq_c(t, root->root->as.discriminator.tag, "kind");
  sp_expect_eq(t, (u64)2, (u64)sp_da_size(root->root->as.discriminator.mapping));
  if (sp_da_size(root->root->as.discriminator.mapping) == 2) {
    sp_expect_str_eq_c(t, root->root->as.discriminator.mapping[0].tag, "a");
    sp_expect_str_eq_c(t, root->root->as.discriminator.mapping[1].tag, "b");
    sp_expect_eq(t, (s32)JTD_FORM_PROPERTIES, (s32)root->root->as.discriminator.mapping[0].schema->form);
    sp_expect_eq(t, (s32)JTD_FORM_PROPERTIES, (s32)root->root->as.discriminator.mapping[1].schema->form);
  }
  return SP_OK;
}

static const jtd_case_t cases [] = {
  {
    .name    = "ok",
    .json    = "discriminator.ok.json",
    .compare = compare_discriminator_ok,
  },
  {
    .name       = "not_string",
    .json       = "discriminator.not_string.json",
    .error      = JTD_ERR_DISCRIMINATOR_NOT_STRING,
    .error_path = "#",
  },
  {
    .name       = "mapping_not_object",
    .json       = "discriminator.mapping_not_object.json",
    .error      = JTD_ERR_MAPPING_NOT_OBJECT,
    .error_path = "#",
  },
  {
    .name       = "mapping_value_not_properties",
    .json       = "discriminator.mapping_not_properties.json",
    .error      = JTD_ERR_MAPPING_NOT_PROPERTIES,
    .error_path = "#/mapping/a",
  },
  {
    .name       = "mapping_escaped_path",
    .json       = "discriminator.mapping_escaped_path.json",
    .error      = JTD_ERR_MAPPING_NOT_PROPERTIES,
    .error_path = "#/mapping/a~1b",
  },
  {
    .name       = "mapping_value_nullable",
    .json       = "discriminator.mapping_nullable.json",
    .error      = JTD_ERR_MAPPING_NOT_PROPERTIES,
    .error_path = "#/mapping/a",
  },
  {
    .name       = "tag_in_properties",
    .json       = "discriminator.tag_in_properties.json",
    .error      = JTD_ERR_DISCRIMINATOR_TAG_REDEFINED,
    .error_path = "#/mapping/a/properties/kind",
  },
  {
    .name       = "tag_in_optional_properties",
    .json       = "discriminator.tag_in_optional_properties.json",
    .error      = JTD_ERR_DISCRIMINATOR_TAG_REDEFINED,
    .error_path = "#/mapping/a/optionalProperties/kind",
  },
};

sp_test_each_fn(discriminator, parse, jtd_case_t, cases, run_jtd_case);
