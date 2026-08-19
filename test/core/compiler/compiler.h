#pragma once

#include "spn_test.h"

#include "compiler/driver.h"
#include "compiler/exports.h"
#include "compiler/toc.h"
#include "ctx/types.h"
#include "paths/paths.h"
#include "session/invocation.h"

#define render_args_max 20

typedef struct {
  spn_err_t err;
  spn_cc_feature_t feature;
  const c8* command;
  const c8* args [render_args_max];
} render_expect_t;

typedef struct {
  spn_arch_t arch;
  spn_os_t os;
  spn_abi_t abi;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
  spn_opt_level_t opt;
  spn_sanitizer_set_t sanitizers;
  const c8* sysroot;
} test_profile_t;

spn_path_t         test_arg_path(const c8* value);
sp_err_t           expect_args(sp_test_t* t, spn_invocation_t* invocation, render_expect_t expect);
spn_cc_toolchain_t test_toolchain(spn_cc_driver_t driver);
spn_profile_info_t test_profile(test_profile_t desc);
