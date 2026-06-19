#include "jtd_test.h"

UTEST(names, types) {
  jtd_expect_str(utest_result, sp_str_view(jtd_type_name(JTD_TYPE_BOOLEAN)),   "boolean");
  jtd_expect_str(utest_result, sp_str_view(jtd_type_name(JTD_TYPE_FLOAT32)),   "float32");
  jtd_expect_str(utest_result, sp_str_view(jtd_type_name(JTD_TYPE_FLOAT64)),   "float64");
  jtd_expect_str(utest_result, sp_str_view(jtd_type_name(JTD_TYPE_INT8)),      "int8");
  jtd_expect_str(utest_result, sp_str_view(jtd_type_name(JTD_TYPE_UINT8)),     "uint8");
  jtd_expect_str(utest_result, sp_str_view(jtd_type_name(JTD_TYPE_INT16)),     "int16");
  jtd_expect_str(utest_result, sp_str_view(jtd_type_name(JTD_TYPE_UINT16)),    "uint16");
  jtd_expect_str(utest_result, sp_str_view(jtd_type_name(JTD_TYPE_INT32)),     "int32");
  jtd_expect_str(utest_result, sp_str_view(jtd_type_name(JTD_TYPE_UINT32)),    "uint32");
  jtd_expect_str(utest_result, sp_str_view(jtd_type_name(JTD_TYPE_STRING)),    "string");
  jtd_expect_str(utest_result, sp_str_view(jtd_type_name(JTD_TYPE_TIMESTAMP)), "timestamp");
}

UTEST(names, forms) {
  jtd_expect_str(utest_result, sp_str_view(jtd_form_name(JTD_FORM_EMPTY)),         "empty");
  jtd_expect_str(utest_result, sp_str_view(jtd_form_name(JTD_FORM_TYPE)),          "type");
  jtd_expect_str(utest_result, sp_str_view(jtd_form_name(JTD_FORM_ENUM)),          "enum");
  jtd_expect_str(utest_result, sp_str_view(jtd_form_name(JTD_FORM_ELEMENTS)),      "elements");
  jtd_expect_str(utest_result, sp_str_view(jtd_form_name(JTD_FORM_PROPERTIES)),    "properties");
  jtd_expect_str(utest_result, sp_str_view(jtd_form_name(JTD_FORM_VALUES)),        "values");
  jtd_expect_str(utest_result, sp_str_view(jtd_form_name(JTD_FORM_DISCRIMINATOR)), "discriminator");
  jtd_expect_str(utest_result, sp_str_view(jtd_form_name(JTD_FORM_REF)),           "ref");
}

UTEST(names, errors) {
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_OK)),                        "ok");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_JSON)),                   "invalid-json");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_SCHEMA_NOT_OBJECT)),      "schema-not-object");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_MULTIPLE_FORMS)),         "multiple-forms");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_TYPE_NOT_STRING)),        "type-not-string");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_UNKNOWN_TYPE)),           "unknown-type");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_ENUM_EMPTY)),             "enum-empty");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_ENUM_NOT_STRING)),        "enum-not-string");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_ENUM_DUPLICATE)),         "enum-duplicate");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_PROPERTIES_NOT_OBJECT)),  "properties-not-object");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_DISCRIMINATOR_NOT_STRING)),"discriminator-not-string");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_MAPPING_NOT_OBJECT)),     "mapping-not-object");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_MAPPING_NOT_PROPERTIES)), "mapping-not-properties");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_REF_NOT_STRING)),         "ref-not-string");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_METADATA_NOT_OBJECT)),    "metadata-not-object");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_NULLABLE_NOT_BOOL)),      "nullable-not-bool");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_ADDITIONAL_PROPERTIES_NOT_BOOL)), "additional-properties-not-bool");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_ADDITIONAL_PROPERTIES_WITHOUT_PROPERTIES)), "additional-properties-without-properties");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_PROPERTIES_DUPLICATE)),   "properties-duplicate");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_DISCRIMINATOR_TAG_REDEFINED)), "discriminator-tag-redefined");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_DEFINITIONS_NOT_ROOT)),   "definitions-not-root");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_REF_UNRESOLVED)),         "ref-unresolved");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_REF_CYCLE)),              "ref-cycle");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_UNKNOWN_MEMBER)),         "unknown-member");
  jtd_expect_str(utest_result, sp_str_view(jtd_err_name(JTD_ERR_UNRECOGNIZED_FORM)),      "unrecognized-form");
}
