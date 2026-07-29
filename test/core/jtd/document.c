#include "jtd_test.h"

static sp_err_t compare_empty(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  sp_expect_eq(t, (s32)JTD_FORM_EMPTY, (s32)root->root->form);
  return SP_OK;
}

static sp_err_t compare_nullable(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  sp_expect_eq(t, (s32)JTD_FORM_TYPE, (s32)root->root->form);
  sp_expect_eq(t, (s32)JTD_TYPE_STRING, (s32)root->root->as.type);
  sp_expect(t, root->root->nullable);
  return SP_OK;
}

static const jtd_case_t cases [] = {
  {
    .name    = "empty_form",
    .json    = "document.empty.json",
    .compare = compare_empty,
  },
  {
    .name    = "nullable",
    .json    = "document.nullable.json",
    .compare = compare_nullable,
  },
  {
    .name    = "metadata",
    .json    = "document.metadata.json",
    .compare = compare_empty,
  },
  {
    .name       = "metadata_not_object",
    .json       = "document.metadata_not_object.json",
    .error      = JTD_ERR_METADATA_NOT_OBJECT,
    .error_path = "#/metadata",
  },
  {
    .name       = "nullable_not_bool",
    .json       = "document.nullable_not_bool.json",
    .error      = JTD_ERR_NULLABLE_NOT_BOOL,
    .error_path = "#",
  },
  {
    .name       = "multiple_forms",
    .json       = "document.multiple_forms.json",
    .error      = JTD_ERR_MULTIPLE_FORMS,
    .error_path = "#",
  },
  {
    .name       = "unknown_key",
    .json       = "document.unknown_key.json",
    .error      = JTD_ERR_UNKNOWN_MEMBER,
    .error_path = "#/foo",
  },
  {
    .name       = "root_not_object",
    .json       = "document.root_not_object.json",
    .error      = JTD_ERR_SCHEMA_NOT_OBJECT,
    .error_path = "#",
  },
  {
    .name       = "invalid_json",
    .json       = "document.invalid_json.json",
    .error      = JTD_ERR_JSON,
    .error_path = "#",
  },
};

sp_test_each_fn(document, parse, jtd_case_t, cases, run_jtd_case);
