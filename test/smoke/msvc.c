#include "harness.h"

sp_test(msvc, link) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/msvc_link",
    .when = { .toolchain = "MV", .host = SPN_OS_WINDOWS, .programs = { "cl" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-msvc" } } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
    },
  });
}

sp_test(msvc, clang_lld) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/msvc_clang_lld",
    .when = { .toolchain = "CM", .host = SPN_OS_WINDOWS, .programs = { "clang" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-msvc" } } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
    },
  });
}

sp_test(msvc, script_rejected) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/msvc_script_rejected",
    .copy = { "main.ld" },
    .when = { .toolchain = "MV", .host = SPN_OS_WINDOWS, .programs = { "cl" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-msvc" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED },
    },
  });
}
