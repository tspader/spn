#include "jtd_test.h"

static void compare_elements_scalar(s32* utest_result, const jtd_root_t* root, const void* expect) {
  (void)expect;
  EXPECT_EQ((s32)JTD_FORM_ELEMENTS, (s32)root->root->form);
  EXPECT_EQ((s32)JTD_FORM_TYPE, (s32)root->root->as.elements.schema->form);
  EXPECT_EQ((s32)JTD_TYPE_STRING, (s32)root->root->as.elements.schema->as.type);
}

static void compare_elements_ref(s32* utest_result, const jtd_root_t* root, const void* expect) {
  (void)expect;
  EXPECT_EQ((s32)JTD_FORM_ELEMENTS, (s32)root->root->form);
  EXPECT_EQ((s32)JTD_FORM_REF, (s32)root->root->as.elements.schema->form);
  jtd_expect_str(utest_result, root->root->as.elements.schema->as.ref.name, "x");
}

UTEST(elements, scalar) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "elements.scalar.json",
    .compare = compare_elements_scalar,
  });
}

UTEST(elements, ref) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "elements.ref.json",
    .compare = compare_elements_ref,
  });
}

UTEST(elements, child_not_object) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "elements.child_not_object.json",
    .error      = JTD_ERR_SCHEMA_NOT_OBJECT,
    .error_path = "#/elements",
  });
}
