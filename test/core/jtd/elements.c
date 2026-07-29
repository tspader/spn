#include "jtd_test.h"

static sp_err_t compare_elements_scalar(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  sp_expect_eq(t, (s32)JTD_FORM_ELEMENTS, (s32)root->root->form);
  sp_expect_eq(t, (s32)JTD_FORM_TYPE, (s32)root->root->as.elements.schema->form);
  sp_expect_eq(t, (s32)JTD_TYPE_STRING, (s32)root->root->as.elements.schema->as.type);
  return SP_OK;
}

static sp_err_t compare_elements_ref(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  sp_expect_eq(t, (s32)JTD_FORM_ELEMENTS, (s32)root->root->form);
  sp_expect_eq(t, (s32)JTD_FORM_REF, (s32)root->root->as.elements.schema->form);
  sp_expect_str_eq_c(t, root->root->as.elements.schema->as.ref.name, "x");
  return SP_OK;
}

static const jtd_case_t cases [] = {
  {
    .name    = "scalar",
    .json    = "elements.scalar.json",
    .compare = compare_elements_scalar,
  },
  {
    .name    = "ref",
    .json    = "elements.ref.json",
    .compare = compare_elements_ref,
  },
  {
    .name       = "child_not_object",
    .json       = "elements.child_not_object.json",
    .error      = JTD_ERR_SCHEMA_NOT_OBJECT,
    .error_path = "#/elements",
  },
};

sp_test_each_fn(elements, parse, jtd_case_t, cases, run_jtd_case);
