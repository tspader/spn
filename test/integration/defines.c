#include "harness.h"

sp_test(defines, build_facts) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/defines/simple",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli.cmd = "build" },
      { .kind = ACTION_VERIFY_CC_ARG, .verify_cc_arg = { "-DSPN_BUILD", "/DSPN_BUILD" } },
    },
  });
}

sp_test(defines, windows_target) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/defines/simple",
    .when.target = "x86_64-windows-gnu",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-gnu" } } },
      { .kind = ACTION_VERIFY_CC_ARG, .verify_cc_arg = { "-DSPN_BUILD_OS_WINDOWS" } },
    },
  });
}
