#include "harness.h"

sp_test(macho, ld64_apple) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/macho_ld64_apple",
    .when = { .host = SPN_OS_MACOS, .programs = { "clang", "ld", "ar" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
    },
  });
}

sp_test(macho, ld64_lld) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/macho_ld64_lld",
    .when = { .host = SPN_OS_MACOS, .programs = { "clang", "ld64.lld", "llvm-ar" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
    },
  });
}

sp_test(macho, zig_build) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/macho_zig",
    .when = { .host = SPN_OS_MACOS, .driver = SPN_CC_DRIVER_ZIG },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
    },
  });
}

sp_test(macho, script_rejected) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/macho_script_rejected",
    .copy = { "main.ld" },
    .when = { .host = SPN_OS_MACOS, .driver = SPN_CC_DRIVER_ZIG },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED },
    },
  });
}
