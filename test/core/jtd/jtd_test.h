#ifndef JTD_TEST_H
#define JTD_TEST_H

#include "utest.h"
#include "jtd.h"

typedef void (*jtd_compare_fn)(s32* utest_result, const jtd_root_t* root, const void* expect);

typedef struct {
  const c8*      json;
  jtd_err_t      error;
  const c8*      error_path;
  const void*    expect;
  jtd_compare_fn compare;
} jtd_case_t;

void run_jtd_case(s32* utest_result, jtd_case_t c);
void jtd_expect_str(s32* utest_result, sp_str_t actual, const c8* expected);

#endif
