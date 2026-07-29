#pragma once

#include "spn_test.h"

#include "compiler/driver.h"
#include "compiler/exports.h"
#include "compiler/toc.h"

#define render_args_max 20

typedef struct {
  spn_err_t err;
  spn_cc_feature_t feature;
  const c8* command;
  const c8* args [render_args_max];
} render_expect_t;

sp_err_t           expect_args(sp_test_t* t, spn_invocation_t* invocation, render_expect_t expect);
spn_cc_toolchain_t test_toolchain(spn_cc_driver_t driver);
