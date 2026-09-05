#include "harness.h"

sp_test(wasm, zig_build) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/wasm_zig",
    .when = { .driver = SPN_CC_DRIVER_ZIG, .target = "wasm32-wasi" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "wasm32-wasi" } } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
    },
  });
}

sp_test(wasm, zig_script_rejected) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/wasm_zig_script",
    .copy = { "main.ld" },
    .when = { .driver = SPN_CC_DRIVER_ZIG, .target = "wasm32-wasi" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "wasm32-wasi" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED },
    },
  });
}
