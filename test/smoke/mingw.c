#include "harness.h"

sp_test(mingw, gnu_script) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/mingw_gnu_script",
    .copy = { "main.ld" },
    .when = { .toolchain = "M", .programs = { "x86_64-w64-mingw32-gcc", "x86_64-w64-mingw32-ar" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-gnu" } } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = target_exe("main", "x86_64-windows-gnu") },
    },
  });
}

sp_test(mingw, zig_build) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/mingw_zig",
    .when = { .driver = SPN_CC_DRIVER_ZIG, .target = "x86_64-windows-gnu" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-gnu" } } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
    },
  });
}

sp_test(mingw, zig_script_rejected) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/mingw_zig_script",
    .copy = { "main.ld" },
    .when = { .driver = SPN_CC_DRIVER_ZIG, .target = "x86_64-windows-gnu" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-gnu" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED },
    },
  });
}

sp_test(mingw, clang_gnu) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/mingw_clang_gnu",
    .when = { .toolchain = "W", .programs = { "clang", "x86_64-w64-mingw32-ld", "x86_64-w64-mingw32-ar" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-gnu" } } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
    },
  });
}
