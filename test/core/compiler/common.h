#pragma once

#include "compiler/driver.h"
#include "sp/macro.h"
#include "utest.h"

#define render_args_max 20

typedef struct {
  spn_err_t err;
  spn_cc_feature_t feature;
  const c8* command;
  const c8* args [render_args_max];
} render_expect_t;

void               expect_args(s32* utest_result, sp_ps_config_t* ps, render_expect_t expect);
spn_cc_toolchain_t test_toolchain(spn_cc_driver_t driver);
