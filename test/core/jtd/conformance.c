#include "jtd_test.h"

// Every schema in test/core/jtd/conformance/ is an invalid JTD schema, taken
// verbatim from the JTD spec's invalid_schemas.json conformance suite. The spec
// declares each invalid; the only assertion is that the parser rejects it. The
// specific error code/path is an implementation detail and is not checked here.

static void run_conformance_reject(s32* utest_result, const c8* file) {
  sp_mem_t mem = sp_mem_os_new();
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_str_t path = sp_fmt(sp_mem_get_scratch(), "{}/conformance/{}", sp_fmt_cstr(JTD_TEST_JSON_DIR), sp_fmt_cstr(file)).value;
  sp_str_t json = sp_zero; sp_io_read_file(sp_mem_get_scratch(), path, &json);

  if (sp_str_empty(json)) {
    EXPECT_TRUE(sp_str_equal_cstr(path, "<conformance fixture exists and is non-empty>"));
  }
  else {
    jtd_result_t result = jtd_parse(mem, json);
    EXPECT_FALSE(result.ok);
    jtd_free(&result);
  }

  sp_mem_end_scratch(scratch);
}

UTEST(conformance, null_schema) { run_conformance_reject(utest_result, "null_schema.json"); }
UTEST(conformance, boolean_schema) { run_conformance_reject(utest_result, "boolean_schema.json"); }
UTEST(conformance, integer_schema) { run_conformance_reject(utest_result, "integer_schema.json"); }
UTEST(conformance, float_schema) { run_conformance_reject(utest_result, "float_schema.json"); }
UTEST(conformance, string_schema) { run_conformance_reject(utest_result, "string_schema.json"); }
UTEST(conformance, array_schema) { run_conformance_reject(utest_result, "array_schema.json"); }
UTEST(conformance, illegal_keyword) { run_conformance_reject(utest_result, "illegal_keyword.json"); }
UTEST(conformance, nullable_not_boolean) { run_conformance_reject(utest_result, "nullable_not_boolean.json"); }
UTEST(conformance, definitions_not_object) { run_conformance_reject(utest_result, "definitions_not_object.json"); }
UTEST(conformance, definition_not_object) { run_conformance_reject(utest_result, "definition_not_object.json"); }
UTEST(conformance, non_root_definitions) { run_conformance_reject(utest_result, "non_root_definitions.json"); }
UTEST(conformance, ref_not_string) { run_conformance_reject(utest_result, "ref_not_string.json"); }
UTEST(conformance, ref_but_no_definitions) { run_conformance_reject(utest_result, "ref_but_no_definitions.json"); }
UTEST(conformance, ref_to_non_existent_definition) { run_conformance_reject(utest_result, "ref_to_non_existent_definition.json"); }
UTEST(conformance, sub_schema_ref_to_non_existent_definition) { run_conformance_reject(utest_result, "sub_schema_ref_to_non_existent_definition.json"); }
UTEST(conformance, type_not_string) { run_conformance_reject(utest_result, "type_not_string.json"); }
UTEST(conformance, type_not_valid_string_value) { run_conformance_reject(utest_result, "type_not_valid_string_value.json"); }
UTEST(conformance, enum_not_array) { run_conformance_reject(utest_result, "enum_not_array.json"); }
UTEST(conformance, enum_empty_array) { run_conformance_reject(utest_result, "enum_empty_array.json"); }
UTEST(conformance, enum_not_array_of_strings) { run_conformance_reject(utest_result, "enum_not_array_of_strings.json"); }
UTEST(conformance, enum_contains_duplicates) { run_conformance_reject(utest_result, "enum_contains_duplicates.json"); }
UTEST(conformance, elements_not_object) { run_conformance_reject(utest_result, "elements_not_object.json"); }
UTEST(conformance, elements_not_correct_schema) { run_conformance_reject(utest_result, "elements_not_correct_schema.json"); }
UTEST(conformance, properties_not_object) { run_conformance_reject(utest_result, "properties_not_object.json"); }
UTEST(conformance, properties_value_not_correct_schema) { run_conformance_reject(utest_result, "properties_value_not_correct_schema.json"); }
UTEST(conformance, optionalproperties_not_object) { run_conformance_reject(utest_result, "optionalproperties_not_object.json"); }
UTEST(conformance, optionalproperties_value_not_correct_schema) { run_conformance_reject(utest_result, "optionalproperties_value_not_correct_schema.json"); }
UTEST(conformance, additionalproperties_not_boolean) { run_conformance_reject(utest_result, "additionalproperties_not_boolean.json"); }
UTEST(conformance, properties_shares_keys_with_optionalproperties) { run_conformance_reject(utest_result, "properties_shares_keys_with_optionalproperties.json"); }
UTEST(conformance, values_not_object) { run_conformance_reject(utest_result, "values_not_object.json"); }
UTEST(conformance, values_not_correct_schema) { run_conformance_reject(utest_result, "values_not_correct_schema.json"); }
UTEST(conformance, discriminator_not_string) { run_conformance_reject(utest_result, "discriminator_not_string.json"); }
UTEST(conformance, mapping_not_object) { run_conformance_reject(utest_result, "mapping_not_object.json"); }
UTEST(conformance, mapping_value_not_correct_schema) { run_conformance_reject(utest_result, "mapping_value_not_correct_schema.json"); }
UTEST(conformance, mapping_value_not_of_properties_form) { run_conformance_reject(utest_result, "mapping_value_not_of_properties_form.json"); }
UTEST(conformance, mapping_value_has_nullable_set_to_true) { run_conformance_reject(utest_result, "mapping_value_has_nullable_set_to_true.json"); }
UTEST(conformance, discriminator_shares_keys_with_mapping_properties) { run_conformance_reject(utest_result, "discriminator_shares_keys_with_mapping_properties.json"); }
UTEST(conformance, discriminator_shares_keys_with_mapping_optionalproperties) { run_conformance_reject(utest_result, "discriminator_shares_keys_with_mapping_optionalproperties.json"); }
UTEST(conformance, invalid_form_ref_and_type) { run_conformance_reject(utest_result, "invalid_form_ref_and_type.json"); }
UTEST(conformance, invalid_form_type_and_enum) { run_conformance_reject(utest_result, "invalid_form_type_and_enum.json"); }
UTEST(conformance, invalid_form_enum_and_elements) { run_conformance_reject(utest_result, "invalid_form_enum_and_elements.json"); }
UTEST(conformance, invalid_form_elements_and_properties) { run_conformance_reject(utest_result, "invalid_form_elements_and_properties.json"); }
UTEST(conformance, invalid_form_elements_and_optionalproperties) { run_conformance_reject(utest_result, "invalid_form_elements_and_optionalproperties.json"); }
UTEST(conformance, invalid_form_elements_and_additionalproperties) { run_conformance_reject(utest_result, "invalid_form_elements_and_additionalproperties.json"); }
UTEST(conformance, invalid_form_additionalproperties_alone) { run_conformance_reject(utest_result, "invalid_form_additionalproperties_alone.json"); }
UTEST(conformance, invalid_form_properties_and_values) { run_conformance_reject(utest_result, "invalid_form_properties_and_values.json"); }
UTEST(conformance, invalid_form_values_and_discriminator) { run_conformance_reject(utest_result, "invalid_form_values_and_discriminator.json"); }
UTEST(conformance, invalid_form_discriminator_alone) { run_conformance_reject(utest_result, "invalid_form_discriminator_alone.json"); }
UTEST(conformance, invalid_form_mapping_alone) { run_conformance_reject(utest_result, "invalid_form_mapping_alone.json"); }
