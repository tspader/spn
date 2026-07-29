#include "jtd_test.h"

typedef struct {
  const c8* values [8];
} enum_expect_t;

static sp_err_t compare_enum(sp_test_t* t, const jtd_result_t* root, const void* expect_v) {
  const enum_expect_t* expect = (const enum_expect_t*)expect_v;
  sp_expect_eq(t, (s32)JTD_FORM_ENUM, (s32)root->root->form);

  u32 expected = 0;
  sp_carr_detect_len(expect->values, expected, expect->values[expected]);
  sp_must_eq(t, expected, sp_da_size(root->root->as.enumeration.values));
  sp_for(i, expected) {
    sp_expect_str_eq_c(t, root->root->as.enumeration.values[i], expect->values[i]);
  }
  return SP_OK;
}

static const jtd_case_t cases [] = {
  {
    .name    = "ok",
    .json    = "enum.ok.json",
    .expect  = &(enum_expect_t){ .values = { "a", "b", "c" } },
    .compare = compare_enum,
  },
  {
    .name       = "empty",
    .json       = "enum.empty.json",
    .error      = JTD_ERR_ENUM_EMPTY,
    .error_path = "#",
  },
  {
    .name       = "duplicate",
    .json       = "enum.duplicate.json",
    .error      = JTD_ERR_ENUM_DUPLICATE,
    .error_path = "#",
  },
  {
    .name       = "value_not_string",
    .json       = "enum.value_not_string.json",
    .error      = JTD_ERR_ENUM_NOT_STRING,
    .error_path = "#",
  },
  {
    .name       = "not_array",
    .json       = "enum.not_array.json",
    .error      = JTD_ERR_ENUM_NOT_STRING,
    .error_path = "#",
  },
};

sp_test_each_fn(enums, parse, jtd_case_t, cases, run_jtd_case);
