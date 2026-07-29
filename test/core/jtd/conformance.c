#include "jtd_test.h"

typedef struct {
  const c8* name;
} conformance_case_t;

static const conformance_case_t cases [] = {
  { "null_schema" },
  { "boolean_schema" },
  { "integer_schema" },
  { "float_schema" },
  { "string_schema" },
  { "array_schema" },
  { "illegal_keyword" },
  { "nullable_not_boolean" },
  { "definitions_not_object" },
  { "definition_not_object" },
  { "non_root_definitions" },
  { "ref_not_string" },
  { "ref_but_no_definitions" },
  { "ref_to_non_existent_definition" },
  { "sub_schema_ref_to_non_existent_definition" },
  { "type_not_string" },
  { "type_not_valid_string_value" },
  { "enum_not_array" },
  { "enum_empty_array" },
  { "enum_not_array_of_strings" },
  { "enum_contains_duplicates" },
  { "elements_not_object" },
  { "elements_not_correct_schema" },
  { "properties_not_object" },
  { "properties_value_not_correct_schema" },
  { "optionalproperties_not_object" },
  { "optionalproperties_value_not_correct_schema" },
  { "additionalproperties_not_boolean" },
  { "properties_shares_keys_with_optionalproperties" },
  { "values_not_object" },
  { "values_not_correct_schema" },
  { "discriminator_not_string" },
  { "mapping_not_object" },
  { "mapping_value_not_correct_schema" },
  { "mapping_value_not_of_properties_form" },
  { "mapping_value_has_nullable_set_to_true" },
  { "discriminator_shares_keys_with_mapping_properties" },
  { "discriminator_shares_keys_with_mapping_optionalproperties" },
  { "invalid_form_ref_and_type" },
  { "invalid_form_type_and_enum" },
  { "invalid_form_enum_and_elements" },
  { "invalid_form_elements_and_properties" },
  { "invalid_form_elements_and_optionalproperties" },
  { "invalid_form_elements_and_additionalproperties" },
  { "invalid_form_additionalproperties_alone" },
  { "invalid_form_properties_and_values" },
  { "invalid_form_values_and_discriminator" },
  { "invalid_form_discriminator_alone" },
  { "invalid_form_mapping_alone" },
};

sp_test_each(conformance, reject, conformance_case_t, cases) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t repo = test_repo_path(mem, sp_str_lit(JTD_TEST_JSON_DIR));
  sp_str_t conformance = sp_fs_join_path(mem, repo, sp_str_lit("conformance"));
  sp_str_t path = sp_fs_join_path(mem, conformance, sp_fmt(mem, "{}.json", sp_fmt_cstr(it->name)).value);
  sp_str_t json = sp_zero; sp_io_read_file(mem, path, &json);
  sp_test_kv(t, "fixture", path);
  sp_must(t, !sp_str_empty(json));

  jtd_result_t result = jtd_parse(mem, json);
  sp_expect(t, !result.ok);
  jtd_free(&result);

  return SP_OK;
}
