#pragma once

#include "spn_test.h"

#include "jtd.h"

typedef sp_err_t (*jtd_compare_fn)(sp_test_t* t, const jtd_result_t* root, const void* expect);

typedef struct {
  const c8*      name;
  const c8*      json;
  jtd_err_t      error;
  const c8*      error_path;
  const void*    expect;
  jtd_compare_fn compare;
} jtd_case_t;

sp_err_t run_jtd_case(sp_test_t* t, const jtd_case_t* c);
