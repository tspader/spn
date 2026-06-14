#include "jtd_test.h"

typedef struct {
  const c8* values[8];
  u32       count;
} enum_expect_t;

static void compare_enum(s32* utest_result, const jtd_root_t* root, const void* expect_v) {
  const enum_expect_t* expect = (const enum_expect_t*)expect_v;
  EXPECT_EQ((s32)JTD_FORM_ENUM, (s32)root->root->form);
  u32 actual = (u32)sp_da_size(root->root->as.enumeration.values);
  EXPECT_EQ((u64)expect->count, (u64)actual);
  u32 n = actual < expect->count ? actual : expect->count;
  sp_for(i, n) {
    jtd_expect_str(utest_result, root->root->as.enumeration.values[i], expect->values[i]);
  }
}

UTEST(enums, ok) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "enum.ok.json",
    .expect  = &(enum_expect_t){ .values = { "a", "b", "c" }, .count = 3 },
    .compare = compare_enum,
  });
}

UTEST(enums, empty) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "enum.empty.json",
    .error      = JTD_ERR_ENUM_EMPTY,
    .error_path = "#",
  });
}

UTEST(enums, duplicate) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "enum.duplicate.json",
    .error      = JTD_ERR_ENUM_DUPLICATE,
    .error_path = "#",
  });
}

UTEST(enums, value_not_string) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "enum.value_not_string.json",
    .error      = JTD_ERR_ENUM_NOT_STRING,
    .error_path = "#",
  });
}

UTEST(enums, not_array) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "enum.not_array.json",
    .error      = JTD_ERR_ENUM_NOT_STRING,
    .error_path = "#",
  });
}
